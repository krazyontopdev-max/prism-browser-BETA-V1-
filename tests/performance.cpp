#include "browserwindow.h"
#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QWebEngineLoadingInfo>
#include <QWebEngineProfile>
#include <QWebEngineView>
#include <QWebEngineFrame>
#include <QRegularExpression>
#include <algorithm>
#include <memory>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <winternl.h>
#endif

#ifdef Q_OS_WIN
static QString processArgument(HANDLE process, const QString &name)
{
    using Query = LONG (NTAPI *)(HANDLE, ULONG, PVOID, ULONG, PULONG);
    const auto query = reinterpret_cast<Query>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationProcess"));
    if (!query) return {};
    ULONG required=0; query(process, 60, nullptr, 0, &required);
    if (required < sizeof(UNICODE_STRING) || required > 1024*1024) return {};
    QByteArray data(int(required), '\0');
    if (query(process, 60, data.data(), required, &required) < 0) return {};
    const auto *line = reinterpret_cast<const UNICODE_STRING *>(data.constData());
    const auto begin = quintptr(data.constData()), ptr = quintptr(line->Buffer);
    if (ptr < begin || ptr + line->Length > begin + quintptr(data.size())) return {};
    const auto match = QRegularExpression("(?:^|\\s)--" + QRegularExpression::escape(name) + "=([^\\s]+)").match(QString::fromWCharArray(line->Buffer, line->Length / 2));
    return match.hasMatch() ? match.captured(1) : QString();
}
#endif

static QJsonObject resources(QTabWidget *tabs)
{
    double working = 0, privateBytes = 0;
    QJsonArray processes;
#ifdef Q_OS_WIN
    QSet<DWORD> renderers;
    for (int i=0; i<tabs->count(); ++i) renderers.insert(DWORD(qobject_cast<QWebEngineView *>(tabs->widget(i))->page()->renderProcessPid()));
    QHash<DWORD, DWORD> parents;
    QHash<DWORD, QString> names;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 entry{}; entry.dwSize = sizeof(entry);
    if (Process32First(snapshot, &entry)) do {
        parents.insert(entry.th32ProcessID, entry.th32ParentProcessID);
        names.insert(entry.th32ProcessID, QString::fromWCharArray(entry.szExeFile));
    } while (Process32Next(snapshot, &entry));
    CloseHandle(snapshot);
    for (auto pid : parents.keys()) {
        DWORD ancestor = pid;
        QSet<DWORD> seen;
        while (ancestor && ancestor != GetCurrentProcessId() && !seen.contains(ancestor)) {
            seen.insert(ancestor); ancestor = parents.value(ancestor);
        }
        if (ancestor != GetCurrentProcessId()) continue;
        HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (!process) continue;
        PROCESS_MEMORY_COUNTERS_EX memory{};
        FILETIME created{}, exited{}, kernel{}, user{};
        if (GetProcessMemoryInfo(process, reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&memory), sizeof(memory))
            && GetProcessTimes(process, &created, &exited, &kernel, &user)) {
            auto number = [](FILETIME f) { ULARGE_INTEGER n{}; n.LowPart=f.dwLowDateTime; n.HighPart=f.dwHighDateTime; return n.QuadPart; };
            const double ws=double(memory.WorkingSetSize)/1048576, pb=double(memory.PrivateUsage)/1048576;
            working += ws; privateBytes += pb;
            processes.append(QJsonObject{{"pid",double(pid)},{"parent",double(parents[pid])},{"name",names[pid]},
                {"identity",QString::number(pid)+":"+QString::number(number(created))},
                {"role",pid==GetCurrentProcessId()?"browser":renderers.contains(pid)?"main-frame renderer":"other WebEngine descendant"},
                {"processType",processArgument(process,"type")},{"utilityType",processArgument(process,"utility-sub-type")},
                {"rendererClientId",processArgument(process,"renderer-client-id")},
                {"privateMiB",pb},{"workingSetMiB",ws},{"cpuSeconds",double(number(kernel)+number(user))/10000000}});
        }
        CloseHandle(process);
    }
#endif
    return {{"privateMiB",privateBytes},{"workingSetMiB",working},{"processes",processes},{"processCount",processes.size()}};
}

static QJsonArray pageOwners(QTabWidget *tabs)
{
    QJsonArray pages;
    for (int i=0; i<tabs->count(); ++i) {
        auto *page=qobject_cast<QWebEngineView *>(tabs->widget(i))->page();
        QJsonArray frames;
        std::function<void(const QWebEngineFrame &, int)> collect = [&](const QWebEngineFrame &frame, int depth) {
            if (!frame.isValid() || depth > 5 || frames.size() >= 100) return;
            const auto url=frame.url().adjusted(QUrl::RemoveUserInfo | QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment);
            frames.append(QJsonObject{{"origin",url.toString()},{"main",frame.isMainFrame()},{"depth",depth}});
            for (const auto &child : frame.children()) collect(child,depth+1);
        };
        collect(page->mainFrame(),0);
        pages.append(QJsonObject{{"index",i},{"origin",page->url().adjusted(QUrl::RemoveUserInfo | QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment).toString()},
            {"mainRendererPid",double(page->renderProcessPid())},{"state",int(page->lifecycleState())},{"loading",page->isLoading()},
            {"devToolsId",page->devToolsId()},{"frames",frames}});
    }
    return pages;
}

static QJsonObject idleSample(QTabWidget *tabs, int milliseconds)
{
    const auto before=resources(tabs);
    QElapsedTimer elapsed; elapsed.start(); QTest::qWait(milliseconds);
    const auto after=resources(tabs);
    QHash<QString,double> old;
    for (const auto &value : before["processes"].toArray()) { const auto p=value.toObject(); old[p["identity"].toString()]=p["cpuSeconds"].toDouble(); }
    double seconds=0; int added=0;
    for (const auto &value : after["processes"].toArray()) {
        const auto p=value.toObject(); const auto id=p["identity"].toString();
        if (old.contains(id)) { seconds += std::max(0.0,p["cpuSeconds"].toDouble()-old.take(id)); }
        else ++added;
    }
    return {{"resources",after},{"cpuOneCorePercent",seconds/(double(elapsed.elapsed())/1000)*100},
        {"cpuSampleMs",double(elapsed.elapsed())},{"newProcessesDuringSample",added},{"exitedProcessesDuringSample",old.size()}};
}

static QVariant script(QWebEnginePage *page, const QString &code)
{
    auto result=std::make_shared<QVariant>(); auto done=std::make_shared<bool>(false);
    page->runJavaScript(code,[result,done](const QVariant &value){*result=value;*done=true;});
    QElapsedTimer wait; wait.start();
    while (!*done && wait.elapsed()<10000) QTest::qWait(5);
    if (!*done) qFatal("JavaScript diagnostic timed out");
    return *result;
}

static QJsonObject load(QWebEngineView *view, const QUrl &url)
{
    QSignalSpy finished(view,&QWebEngineView::loadFinished);
    int status=0;
    auto connection=QObject::connect(view->page(),&QWebEnginePage::loadingChanged,view,[&status](const QWebEngineLoadingInfo &info){status=info.errorCode();});
    QElapsedTimer timer; timer.start(); view->load(url);
    while (finished.isEmpty() && timer.elapsed()<45000) QTest::qWait(20);
    QObject::disconnect(connection);
    return {{"requested",url.toString()},{"loadMs",double(timer.elapsed())},{"timedOut",finished.isEmpty()},
        {"ok",!finished.isEmpty() && finished.last().first().toBool()},{"errorCode",status}};
}

static QJsonObject switches(QTabWidget *tabs, QJsonArray *owners = nullptr)
{
    QVector<double> handlers, frames;
    if (tabs->count()<2) return {};
    for (int i=0;i<20;++i) {
        const int index=(tabs->currentIndex()+1)%tabs->count();
        QElapsedTimer timer; timer.start(); tabs->setCurrentIndex(index);
        handlers.append(double(timer.nsecsElapsed())/1000000);
        auto *page=qobject_cast<QWebEngineView *>(tabs->currentWidget())->page();
        script(page,"window.prismFrameReady=false;requestAnimationFrame(()=>window.prismFrameReady=true);true");
        while (!script(page,"window.prismFrameReady").toBool() && timer.elapsed()<5000) QTest::qWait(5);
        frames.append(double(timer.nsecsElapsed())/1000000);
        if (owners) owners->append(QJsonObject{{"switch",i},{"pages",pageOwners(tabs)},{"resources",resources(tabs)}});
    }
    std::sort(handlers.begin(),handlers.end()); std::sort(frames.begin(),frames.end());
    return {{"handlerMedianMs",handlers[handlers.size()/2]},{"rafAckMedianMs",frames[frames.size()/2]},
        {"rafAckP95Ms",frames[int(frames.size()*0.95)-1]},{"samples",frames.size()}};
}

int main(int argc,char **argv)
{
    QElapsedTimer startup; startup.start();
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc,argv); QApplication::setStyle("Fusion");
    QCoreApplication::setOrganizationName("PrismBench"); QCoreApplication::setApplicationName("PrismV04Bench");
    QTemporaryDir preferences;
    QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,preferences.path());
    const QString stateRoot=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/NewTab";
    QDir().mkpath(stateRoot);
    QFile options(stateRoot+"/settings.json");
    if (!options.open(QIODevice::WriteOnly)) return 1;
    options.write(QJsonDocument(QJsonObject{{"options",QJsonObject{{"mode","builtinFixed"},{"fixedId","prism-coastlines-02"},{"clock24",true},{"categories",QJsonArray{"coastlines","nature","cities","mountains"}}}},{"shortcuts",QJsonArray{}}}).toJson()); options.close();
    QWebEngineProfile profile;
    BrowserWindow window(&profile); window.resize(1180,800); window.show();
    auto *tabs=window.findChild<QTabWidget *>("tabs");
    auto *first=qobject_cast<QWebEngineView *>(tabs->currentWidget());
    while ((first->title()!="New tab" || first->page()->isLoading()) && startup.elapsed()<45000) QTest::qWait(10);
    const auto loadedAt=startup.elapsed();
    while (!script(first->page(),"document.getElementById('wallpaper')?.style.backgroundImage.includes('file:')").toBool() && startup.elapsed()<45000) QTest::qWait(10);
    script(first->page(),"window.prismPhotoReady=false;const photo=new Image();photo.onload=()=>window.prismPhotoReady=true;photo.src=document.getElementById('wallpaper').style.backgroundImage.slice(5,-2);true");
    while (!script(first->page(),"window.prismPhotoReady").toBool() && startup.elapsed()<45000) QTest::qWait(10);
    QJsonObject output{{"utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},{"mainToLoadMs",double(loadedAt)},
        {"mainToPhotoDecodedMs",double(startup.elapsed())},{"osColdStartup",false},{"profileOffTheRecord",profile.isOffTheRecord()}};
    QTest::qWait(1500);
    output["newTabIdle"]=idleSample(tabs,5000);
    const bool live=app.arguments().contains("--live");
    const bool newTabs=app.arguments().contains("--newtabs");
    const auto compareBase=qEnvironmentVariable("PRISM_COMPARE_BASE");
    if (!compareBase.isEmpty()) {
        QJsonArray samples;
        for (int count : {1,5,10}) {
            const int begin=tabs->count()==1 && count==1?0:tabs->count();
            while(tabs->count()<count) window.addTab(QUrl(),true);
            for(int i=begin;i<count;++i) {
                tabs->setCurrentIndex(i);
                auto *web=qobject_cast<QWebEngineView *>(tabs->widget(i));
                const auto result=load(web,QUrl(compareBase+QString::number(i)));
                if(!result["ok"].toBool()) return 2;
                QTest::qWait(250);
            }
            QTest::qWait(5000);
            QJsonObject ready{{"pid",double(QCoreApplication::applicationPid())},{"tabs",count},
                {"viewport",QJsonValue::fromVariant(script(qobject_cast<QWebEngineView *>(tabs->currentWidget())->page(),"({width:innerWidth,height:innerHeight,scale:devicePixelRatio})"))},
                {"owners",pageOwners(tabs)}};
            QFile marker(QString::fromLocal8Bit(argv[1])+".ready");
            if(!marker.open(QIODevice::WriteOnly)) return 3;
            marker.write(QJsonDocument(ready).toJson());marker.close();
            const QString ack=QString::fromLocal8Bit(argv[1])+".ack";
            QElapsedTimer wait;wait.start();
            while(!QFile::exists(ack) && wait.elapsed()<30000)QTest::qWait(50);
            if(!QFile::remove(ack))return 4;
            samples.append(ready);
        }
        QFile file(QString::fromLocal8Bit(argv[1]));if(!file.open(QIODevice::WriteOnly))return 1;
        file.write(QJsonDocument(QJsonObject{{"case","visible matched comparison"},{"samples",samples}}).toJson());return 0;
    }
    QTcpServer server; server.listen(QHostAddress::LocalHost);
    QObject::connect(&server,&QTcpServer::newConnection,&server,[&server]{while(auto *socket=server.nextPendingConnection()) {
        QObject::connect(socket,&QTcpSocket::readyRead,socket,[socket]{
            auto request=socket->property("request").toByteArray()+socket->readAll();socket->setProperty("request",request);
            if(!request.contains("\r\n\r\n"))return;
            QByteArray body="<!doctype html><title>Prism repeatable workload</title><style>div{height:90px;background:#bcd;margin:8px}</style><input value='unchanged'><script>window.heap=new Uint8Array(32*1024*1024);for(let i=0;i<heap.length;i+=4096)heap[i]=i%251;window.work=0;setInterval(()=>{let sum=0;for(let i=0;i<10000;i++)sum+=Math.sqrt(i);window.work=sum},100);</script>";
            for(int i=0;i<150;++i)body+="<div>Prism test workload</div>";
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nContent-Length: "+QByteArray::number(body.size())+"\r\n\r\n"+body);socket->disconnectFromHost();
        });
        QObject::connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
    }});
    QJsonArray loading, workload;
    const QStringList websites={"https://www.youtube.com/","https://www.tiktok.com/","https://claude.ai/","https://chat.deepseek.com/","https://example.com/"};
    for (int count : (live?QList<int>{1,5}:QList<int>{1,5,10})) {
        while(tabs->count()<count) window.addTab(QUrl(),true);
        const int begin=count==1?0:count==5?1:5;
        for(int i=begin;i<count;++i) {
            tabs->setCurrentIndex(i); auto *view=qobject_cast<QWebEngineView *>(tabs->widget(i));
            const QUrl url=live?QUrl(websites[i]):newTabs?newTabUrl():QUrl(QString("http://127.0.0.1:%1/%2").arg(server.serverPort()).arg(i));
            auto row=load(view,url); QTest::qWait(live?4000:250);
            row["title"]=view->title();row["url"]=view->url().toString();row["error"]=view->property("errorMessage").toString();
            if(live)row["page"]=QJsonValue::fromVariant(script(view->page(),"({text:document.body?.innerText.slice(0,700),images:[...document.images].filter(i=>i.complete&&i.naturalWidth>0).length,resources:performance.getEntriesByType('resource').length})"));
            loading.append(row);
        }
        QTest::qWait(1500);
        workload.append(QJsonObject{{"tabs",count},{"idle",idleSample(tabs,3000)},{"switch",switches(tabs)}});
    }
    output["case"]=live?"five live websites":newTabs?"internal new tabs":"synthetic HTTP pages, 32 MiB touched JS allocation per page";
    output["loads"]=loading;output["workload"]=workload;
    auto savings=[&](const QString &mode) {
        window.findChild<QAction *>("savings_"+mode)->trigger();
        for(int i=0;i<tabs->count();++i)if(tabs->widget(i)!=tabs->currentWidget())tabs->widget(i)->setProperty("inactiveSince",-3600000LL);
        window.evaluateSleepingTabs(); QTest::qWait(500); window.evaluateSleepingTabs(); QTest::qWait(2500);
        QJsonArray states;
        for(int i=0;i<tabs->count();++i) { auto *page=qobject_cast<QWebEngineView *>(tabs->widget(i))->page(); states.append(QJsonObject{{"lifecycle",int(page->lifecycleState())},{"recommended",int(page->recommendedState())},{"rendererPid",double(page->renderProcessPid())}}); }
        return QJsonObject{{"pages",states},{"idle",idleSample(tabs,3000)}};
    };
    output["balanced"]=savings("balanced");
    const bool retention=app.arguments().contains("--retention");
    if (retention) output["beforeDiscardOwners"]=pageOwners(tabs);
    output["maximum"]=savings("maximum");
    QJsonArray wakeOwners;
    output["wakeSwitch"]=switches(tabs, retention ? &wakeOwners : nullptr);
    if (retention) { output["wakeOwners"]=wakeOwners; output["beforeCloseOwners"]=pageOwners(tabs); }
    while(tabs->count()>1)window.closeTab(tabs->count()-1);
    QElapsedTimer closed; closed.start();
    QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(2000);
    output["afterClose"]=idleSample(tabs,2000);
    if (retention) {
        QJsonArray observations;
        for (const int seconds : {5,30,60}) {
            while (closed.elapsed() < seconds*1000) QTest::qWait(int(qMin<qint64>(250, seconds*1000-closed.elapsed())));
            observations.append(QJsonObject{{"elapsedMs",double(closed.elapsed())},{"pages",pageOwners(tabs)},{"resources",resources(tabs)}});
            qInfo("Retention sample at %d seconds",seconds);
        }
        output["retention"]=observations;
    }
    QFile file(argc>1?argv[1]:"benchmark.json"); if(!file.open(QIODevice::WriteOnly))return 1;
    file.write(QJsonDocument(output).toJson());
}
