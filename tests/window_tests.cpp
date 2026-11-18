#include "browserwindow.h"
#include "address.h"
#include "localfilepicker.h"
#include "browserdata.h"
#include <QTreeWidget>
#include <QJsonObject>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QProcess>
#include <QUuid>
#include <QListView>
#include <QScopeGuard>

#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QFile>
#include <QMessageBox>
#include <QJsonDocument>
#include <QWebEngineDownloadRequest>
#include <QLabel>
#include <QLineEdit>
#include <QSettings>
#include <QSignalSpy>
#include <QTabBar>
#include <QTabWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QTest>
#include <QToolButton>
#include <QTimer>
#include <QWebEngineHistory>
#include <QWebEngineNavigationRequest>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>

class WindowTests : public QObject {
    Q_OBJECT

    QTcpServer server;

    QUrl localUrl(const QString &path = "/one") const
    {
        return QUrl(QString("http://127.0.0.1:%1%2").arg(server.serverPort()).arg(path));
    }

private slots:
    void sharesOnlySelectedTestWindow()
    {
        if (!qEnvironmentVariableIsSet("PRISM_CAPTURE_TESTS")) QSKIP("Actual window capture is separately opt-in.");
        const auto trace = [](const char *stage) {
            QFile file(QCoreApplication::applicationDirPath() + "/screen-trace.txt");
            if (file.open(QIODevice::WriteOnly | QIODevice::Append)) { file.write(stage); file.write("\n"); }
        };
        trace("start");
        const auto title = "Prism capture fixture " + QUuid::createUuid().toString(QUuid::WithoutBraces);
        QProcess fixture;
        fixture.start(QCoreApplication::applicationDirPath() + "/prism_capture_fixture.exe", {title});
        QVERIFY(fixture.waitForStarted(5000));
        trace("fixture started");
        const auto cleanup = qScopeGuard([&fixture] {
            if (fixture.state() != QProcess::NotRunning) {
                fixture.terminate();
                if (!fixture.waitForFinished(3000)) { fixture.kill(); fixture.waitForFinished(3000); }
            }
        });
        QWebEngineProfile profile; BrowserWindow window(&profile, true);
        window.resize(800, 600); window.move(450, 300); window.show();
        auto *view = qobject_cast<QWebEngineView *>(window.findChild<QTabWidget *>("tabs")->currentWidget());
        view->settings()->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, true);
        view->setHtml(R"HTML(<title>Window stream</title><button style='display:block' onclick=start()>Share</button><video id=v autoplay muted playsinline width=160></video><canvas id=sample width=1 height=1></canvas><script>
            window.captureResult={state:'pending'};let stream,timer;
            function stop(){clearInterval(timer);if(stream)stream.getTracks().forEach(t=>t.stop())}
            window.addEventListener('pagehide',stop,{once:true});
            async function start(){try{
              stream=await navigator.mediaDevices.getDisplayMedia({video:true,audio:false});v.srcObject=stream;window.captureResult={state:'stream-active'};
              const begin=Date.now();timer=setInterval(()=>{try{
                const frames=v.getVideoPlaybackQuality().totalVideoFrames;
                if(frames>=3&&v.videoWidth){
                  const ctx=sample.getContext('2d');ctx.drawImage(v,v.videoWidth*.2,v.videoHeight*.8,1,1,0,0,1,1);
                  const p=ctx.getImageData(0,0,1,1).data;
                  if(Math.abs(p[0]-p[2])>60){window.captureResult={state:'passed',frames,width:v.videoWidth,height:v.videoHeight,red:p[0],blue:p[2]};stop();document.title='Window capture verified'}
                }
                if(Date.now()-begin>15000){window.captureResult={state:'timeout',frames};stop();document.title='Window capture timeout'}
              }catch(e){stop();window.captureResult={state:'error',name:e.name};document.title='Window capture failed'}},100);
            }catch(e){window.captureResult={state:'error',name:e.name};document.title='Window capture failed '+e.name}}
            </script>)HTML", localUrl());
        QTRY_COMPARE(view->title(), QString("Window stream"));
        QTRY_VERIFY(!view->page()->isLoading()); QTRY_VERIFY(view->focusProxy());
        trace("page loaded");
        QTest::mouseClick(view->focusProxy(), Qt::LeftButton, Qt::NoModifier, QPoint(25, 18));
        trace("clicked Share");
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QDialog *>("desktopCaptureDialog") || window.findChild<QMessageBox *>("permissionPrompt"), 15000);
        if (auto *permission = window.findChild<QMessageBox *>("permissionPrompt")) permission->done(QMessageBox::Yes);
        QTRY_VERIFY(window.findChild<QDialog *>("desktopCaptureDialog"));
        auto *picker = window.findChild<QDialog *>("desktopCaptureDialog");
        trace("picker found");
        picker->findChild<QTabWidget *>()->setCurrentIndex(1);
        auto *list = picker->findChild<QListView *>("captureWindows"); QVERIFY(list);
        const auto findFixture = [list, title] {
            for (int row = 0; row < list->model()->rowCount(); ++row) {
                const auto index = list->model()->index(row, 0);
                if (index.data().toString() == title) return index;
            }
            return QModelIndex();
        };
        QTRY_VERIFY_WITH_TIMEOUT(findFixture().isValid(), 10000);
        trace("fixture row found");
        list->setCurrentIndex(findFixture());
        auto *share = picker->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
        QVERIFY(share->isEnabled()); trace("accepting source"); share->click(); trace("source accepted");
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QMessageBox *>("permissionPrompt") || view->title() != "Window stream", 15000);
        if (auto *permission = window.findChild<QMessageBox *>("permissionPrompt")) {
            trace("permission prompt found");
            QVERIFY(permission->text().contains("127.0.0.1")); permission->done(QMessageBox::Yes);
            trace("permission granted");
        }
        QTimer snapshots;
        connect(&snapshots, &QTimer::timeout, &window, [view] {
            view->page()->runJavaScript("({result:window.captureResult,videoReady:v.readyState,frames:v.getVideoPlaybackQuality().totalVideoFrames,track:stream?stream.getVideoTracks()[0].readyState:null})", [](const QVariant &value) {
                if (!value.isValid()) return;
                QFile file(QCoreApplication::applicationDirPath() + "/screen-debug.json");
                if (file.open(QIODevice::WriteOnly)) file.write(QJsonDocument(QJsonObject::fromVariantMap(value.toMap())).toJson());
            });
        });
        snapshots.start(1000);
        QTRY_COMPARE_WITH_TIMEOUT(view->title(), QString("Window capture verified"), 20000);
        trace("frames verified");
        auto result = std::make_shared<QVariant>();
        view->page()->runJavaScript("window.captureResult", [result](const QVariant &value) { *result = value; });
        QTRY_VERIFY(result->isValid());
        QFile output(QCoreApplication::applicationDirPath() + "/screen-results.json");
        QVERIFY(output.open(QIODevice::WriteOnly));
        output.write(QJsonDocument(QJsonObject::fromVariantMap(result->toMap())).toJson());
    }

    void screenSharingRequiresSourceConsent_data()
    {
        QTest::addColumn<QString>("ending");
        QTest::newRow("cancel") << QString("cancel");
        QTest::newRow("navigate") << QString("navigate");
        QTest::newRow("close") << QString("close");
    }

    void screenSharingRequiresSourceConsent()
    {
        QFETCH(QString, ending);
        QWebEngineProfile profile; BrowserWindow window(&profile); window.show();
        auto *view = qobject_cast<QWebEngineView *>(window.findChild<QTabWidget *>("tabs")->currentWidget());
        if (!view->settings()->testAttribute(QWebEngineSettings::ScreenCaptureEnabled)) {
            if (!qEnvironmentVariableIsSet("PRISM_CAPTURE_TESTS")) QSKIP("Stock engine capture deadlock; requires the validated engine candidate.");
            view->settings()->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, true);
        }
        view->setHtml("<title>Capture</title><button onclick=\"navigator.mediaDevices.getDisplayMedia({video:true}).then(s=>{s.getTracks().forEach(t=>t.stop())}).catch(e=>document.title='Capture denied '+e.name)\">Share</button>", localUrl());
        QTRY_COMPARE(view->title(), QString("Capture"));
        QTRY_VERIFY_WITH_TIMEOUT(!view->page()->isLoading(), 10000);
        QTRY_VERIFY(view->focusProxy());
        QTest::mouseClick(view->focusProxy(), Qt::LeftButton, Qt::NoModifier, QPoint(25, 18));
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QDialog *>("desktopCaptureDialog") || window.findChild<QMessageBox *>("permissionPrompt"), 15000);
        if (auto *permission = window.findChild<QMessageBox *>("permissionPrompt")) permission->done(QMessageBox::Yes);
        QTRY_VERIFY(window.findChild<QDialog *>("desktopCaptureDialog"));
        QPointer<QDialog> picker = window.findChild<QDialog *>("desktopCaptureDialog");
        QVERIFY(!picker->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled());
        if (ending == "cancel") {
            picker->reject();
            QTRY_VERIFY(view->title() == "Capture denied NotAllowedError" || view->title() == "Capture denied AbortError");
            QVERIFY(!view->property("permissionPending").toBool());
        } else if (ending == "navigate") {
            view->load(localUrl("/two"));
            QTRY_COMPARE(view->title(), QString("Second page"));
            QVERIFY(!view->property("permissionPending").toBool());
        } else {
            QPointer<QWebEnginePage> page = view->page();
            window.closeTab(0);
            QTRY_VERIFY(page.isNull());
        }
        QTRY_VERIFY(picker.isNull());
    }

    void invalidBookmarkEditKeepsOriginalAddress()
    {
        QWebEngineProfile profile; BrowserWindow window(&profile); window.show();
        auto *data = window.findChild<BrowserData *>(); QVERIFY(data);
        QVERIFY(data->setBookmark({}, "Validation", QUrl("https://example.test/original")));
        const auto id = data->bookmarks().last().toObject().value("id").toString();
        const auto cleanup = qScopeGuard([data, id] { data->removeBookmarks({id}); });
        window.findChild<QAction *>("bookmarksAction")->trigger();
        auto *dialog = window.findChild<QDialog *>("bookmarksDialog"); QVERIFY(dialog);
        auto *tree = dialog->findChild<QTreeWidget *>(); QVERIFY(tree);
        for (int i = 0; i < tree->topLevelItemCount(); ++i)
            if (tree->topLevelItem(i)->data(0, Qt::UserRole).toString() == id) tree->setCurrentItem(tree->topLevelItem(i));
        for (auto *button : dialog->findChildren<QPushButton *>()) if (button->text() == "Edit…") { button->click(); break; }
        auto *editor = dialog->findChild<QDialog *>("bookmarkEditor"); QVERIFY(editor);
        editor->findChildren<QLineEdit *>().at(1)->setText("words that are not a website");
        editor->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        QVERIFY(editor->isVisible());
        for (const auto &value : data->bookmarks()) if (value.toObject().value("id").toString() == id)
            QCOMPARE(value.toObject().value("url").toString(), QString("https://example.test/original"));
    }

    void shieldsOnPublicSites()
    {
        if (!qEnvironmentVariableIsSet("PRISM_COMPATIBILITY_CHECKS")) QSKIP("External compatibility diagnostic is separately opt-in.");
        BrowserWindow window(nullptr, true); window.show();
        auto *view = qobject_cast<QWebEngineView *>(window.findChild<QTabWidget *>("tabs")->currentWidget());
        QTRY_VERIFY_WITH_TIMEOUT(view->url() == newTabUrl() && !view->page()->isLoading(), 15000);
        bool allLoaded = true;
        for (const auto &url : {"https://www.google.com/search?q=Prism+browser", "https://www.youtube.com/"}) {
            QSignalSpy finished(view, &QWebEngineView::loadFinished);
            view->load(QUrl(url));
            QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty() && !view->page()->isLoading(), 60000);
            allLoaded = allLoaded && finished.last().first().toBool();
            QString text;
            bool sampled = false;
            view->page()->runJavaScript("document.body ? document.body.innerText.slice(0,12000) : ''", [&text, &sampled](const QVariant &value) { text = value.toString(); sampled = true; });
            QTRY_VERIFY_WITH_TIMEOUT(sampled, 5000);
            const bool challenge = text.contains("unusual traffic", Qt::CaseInsensitive) || text.contains("not a robot", Qt::CaseInsensitive);
            qInfo().noquote() << "Standard Shields:" << view->url().host() << "title:" << view->title() << "loaded:" << finished.last().first().toBool() << "challenge:" << challenge << "sample characters:" << text.size();
        }
        QVERIFY(allLoaded);
    }

    void settingsSearchAndApply()
    {
        QWebEngineProfile profile;
        BrowserWindow window(&profile); window.show();
        auto *action = window.findChild<QAction *>("settingsAction");
        QVERIFY(action); action->trigger();
        auto *dialog = window.findChild<QDialog *>("settingsDialog");
        QVERIFY(dialog);
        QVERIFY(dialog->grab().save(QCoreApplication::applicationDirPath() + "/settings-preview.png"));
        auto *search = dialog->findChild<QLineEdit *>("settingsSearch");
        QVERIFY(search); search->setText("homepage");
        auto *home = dialog->findChild<QLineEdit *>("homepageSetting");
        QVERIFY(home); QVERIFY(home->isVisible());
        home->setText("https://example.com/home");
        auto *apply = dialog->findChild<QPushButton *>("settingsApply");
        QVERIFY(apply); apply->click();
        QCOMPARE(QSettings().value("general/homepage").toString(), QString("https://example.com/home"));
        QUrl navigation;
        auto *view = qobject_cast<QWebEngineView *>(window.findChild<QTabWidget *>("tabs")->currentWidget());
        connect(view->page(), &QWebEnginePage::navigationRequested, this, [&navigation](QWebEngineNavigationRequest &request) {
            if (request.isMainFrame() && request.url().host() == "example.com") { navigation = request.url(); request.reject(); }
        });
        window.findChild<QAction *>("homeAction")->trigger();
        QTRY_COMPARE(navigation, QUrl("https://example.com/home"));
        QSettings().remove("general/homepage");
    }

    void recoveryRestoresUrlsWithoutChangingStartupPreference()
    {
        const auto cleanup = qScopeGuard([] { QSettings().remove("general/startup"); QSettings().remove("session"); });
        QSettings().setValue("general/startup", "newtab");
        QSettings().setValue("session/urls", QStringList{localUrl("/two").toString(), "javascript:alert(1)"});
        QWebEngineProfile profile;
        {
            BrowserWindow recovered(&profile, false, true); recovered.show();
            auto *tabs = recovered.findChild<QTabWidget *>("tabs"); QCOMPARE(tabs->count(), 1);
            auto *view = qobject_cast<QWebEngineView *>(tabs->currentWidget());
            QTRY_COMPARE(view->title(), QString("Second page"));
            QCOMPARE(QSettings().value("general/startup").toString(), QString("newtab"));
        }
        BrowserWindow privateWindow(nullptr, true, true);
        auto *tabs = privateWindow.findChild<QTabWidget *>("tabs");
        QCOMPARE(qobject_cast<QWebEngineView *>(tabs->currentWidget())->page()->requestedUrl(), newTabUrl());
    }

    void sessionsRestoreLocalUrls()
    {
        QWebEngineProfile profile;
        {
            BrowserWindow window(&profile); window.show();
            auto *first = qobject_cast<QWebEngineView *>(window.findChild<QTabWidget *>("tabs")->currentWidget());
            first->load(localUrl("/with space")); QTRY_COMPARE(first->title(), QString("First page"));
            auto *second = window.addTab(localUrl("/two")); QTRY_COMPARE(second->title(), QString("Second page"));
            window.close();
        }
        QSettings().setValue("general/startup", "session");
        {
            BrowserWindow restored(&profile); restored.show();
            auto *tabs = restored.findChild<QTabWidget *>("tabs"); QCOMPARE(tabs->count(), 2);
            QCOMPARE(qobject_cast<QWebEngineView *>(tabs->widget(0))->page()->requestedUrl(), localUrl("/with space"));
            auto *view = qobject_cast<QWebEngineView *>(tabs->currentWidget()); QTRY_COMPARE(view->title(), QString("Second page"));
        }
        QSettings().remove("general/startup"); QSettings().remove("session");
    }

    void automaticDownloadsKeepExistingFiles()
    {
        const auto cleanup = qScopeGuard([] { QSettings().remove("downloads/ask"); QSettings().remove("downloads/directory"); });
        QTemporaryDir directory;
        QFile existing(directory.path() + "/download.html"); QVERIFY(existing.open(QIODevice::WriteOnly)); existing.write("keep"); existing.close();
        QSettings().setValue("downloads/ask", false); QSettings().setValue("downloads/directory", directory.path());
        bool completed = false;
        QWebEngineProfile profile;
        {
            BrowserWindow window(&profile); window.show();
            auto *view = qobject_cast<QWebEngineView *>(window.findChild<QTabWidget *>("tabs")->currentWidget());
            view->load(localUrl()); QTRY_COMPARE(view->title(), QString("First page"));
            connect(&profile, &QWebEngineProfile::downloadRequested, &window, [&window, &completed](QWebEngineDownloadRequest *request) {
                connect(request, &QWebEngineDownloadRequest::stateChanged, &window, [request, &completed] {
                    if (request->state() == QWebEngineDownloadRequest::DownloadCompleted) completed = true;
                    qInfo() << "Automatic download:" << request->downloadFileName() << request->state() << request->interruptReasonString();
                });
            });
            view->page()->download(localUrl(), "download.html");
            QTRY_VERIFY_WITH_TIMEOUT(completed, 15000);
            QTRY_VERIFY_WITH_TIMEOUT(QFileInfo(directory.path() + "/download (1).html").size() > 0, 15000);
            QVERIFY(existing.open(QIODevice::ReadOnly)); QCOMPARE(existing.readAll(), QByteArray("keep")); existing.close();
        }
        QSettings().remove("downloads/ask"); QSettings().remove("downloads/directory");
    }

    void privateFileChooserDoesNotSaveSettings()
    {
        QTemporaryDir files;
        const auto keys = QSettings().allKeys();
        auto *picker = new LocalFilePicker(nullptr, true, "download.txt");
        QSignalSpy selected(picker, &LocalFilePicker::pathSelected);
        picker->findChild<QLineEdit *>("pickerDirectory")->setText(files.path());
        picker->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        QCOMPARE(selected.count(), 1);
        QCOMPARE(selected.first().first().toString(), files.path() + "/download.txt");
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(QSettings().allKeys(), keys);
    }

    void privateWindowsDoNotShareCookies()
    {
        QWebEngineProfile profile;
        BrowserWindow window(&profile); window.show();
        auto *view = window.findChild<QTabWidget *>("tabs")->currentWidget();
        auto *regular = qobject_cast<QWebEngineView *>(view);
        regular->load(localUrl());
        QTRY_COMPARE(regular->title(), QString("First page"));
        bool set = false;
        regular->page()->runJavaScript("document.cookie='ordinary=present; path=/';true", [&set](const QVariant &value) { set = value.toBool(); });
        QTRY_VERIFY(set);
        QString ordinaryCookie;
        regular->page()->runJavaScript("document.cookie", [&ordinaryCookie](const QVariant &value) { ordinaryCookie = value.toString(); });
        QTRY_VERIFY(ordinaryCookie.contains("ordinary=present"));
        auto *action = window.findChild<QAction *>("privateWindowAction");
        QVERIFY(action);
        action->trigger();
        BrowserWindow *privateWindow = nullptr;
        for (auto *widget : QApplication::topLevelWidgets())
            if (widget->objectName() == "privateBrowser") privateWindow = qobject_cast<BrowserWindow *>(widget);
        QVERIFY(privateWindow);
        QPointer<BrowserWindow> guard(privateWindow);
        auto *privateView = qobject_cast<QWebEngineView *>(privateWindow->findChild<QTabWidget *>("tabs")->currentWidget());
        QVERIFY(privateView->page()->profile()->isOffTheRecord());
        QVERIFY(privateView->page()->profile() != &profile);
        privateView->load(localUrl());
        QTRY_COMPARE(privateView->title(), QString("First page"));
        QString cookie = "waiting";
        privateView->page()->runJavaScript("document.cookie", [&cookie](const QVariant &value) { cookie = value.toString(); });
        QTRY_VERIFY(cookie != "waiting");
        QVERIFY(!cookie.contains("ordinary=present"));
        privateWindow->close();
        QTRY_VERIFY(guard.isNull());
    }

    void initTestCase()
    {
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, this, [this] {
            while (auto *socket = server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [socket] {
                    auto request = socket->property("request").toByteArray() + socket->readAll();
                    socket->setProperty("request", request);
                    if (!request.contains("\r\n\r\n"))
                        return;
                    const auto path = request.split(' ').value(1);
                    if (path == "/status429") {
                        const QByteArray body = "<!doctype html><title>Verification required</title><h1>Follow this website's verification instructions</h1>";
                        socket->write("HTTP/1.1 429 Too Many Requests\r\nContent-Type: text/html\r\nConnection: close\r\nContent-Length: "
                            + QByteArray::number(body.size()) + "\r\n\r\n" + body);
                        socket->disconnectFromHost();
                        return;
                    }
                    if (path == "/slow")
                        return;
                    if (path == "/redirect") {
                        socket->write("HTTP/1.1 302 Found\r\nLocation: /two\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                        socket->disconnectFromHost();
                        return;
                    }
                    if (path == "/auth" && !request.contains("Authorization: Basic dXNlcjpwYXNz")) {
                        socket->write("HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Basic realm=\"PrismTest\"\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                        socket->disconnectFromHost();
                        return;
                    }
                    const QByteArray title = path == "/two" ? "Second page" : "First page";
                    const QByteArray body = "<!doctype html><title>" + title
                        + "</title><h1>" + title + "</h1><a href='/two'>Next page</a>"
                          "<a href='/two' target='_blank'>New tab</a>";
                    socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
                                  "Connection: close\r\nContent-Length: "
                                  + QByteArray::number(body.size()) + "\r\n\r\n" + body);
                    socket->disconnectFromHost();
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }

    void websiteNavigation()
    {
        QWebEngineProfile profile;
        BrowserWindow window(&profile);
        window.show();
        auto *tabs = window.findChild<QTabWidget *>("tabs");
        auto *view = qobject_cast<QWebEngineView *>(tabs->currentWidget());
        view->load(localUrl());
        QTRY_COMPARE_WITH_TIMEOUT(view->title(), "First page", 15000);
        QVERIFY(!view->page()->webChannel());
        QTRY_COMPARE(tabs->tabText(tabs->currentIndex()), "First page");
        view->load(localUrl("/two"));
        QTRY_COMPARE_WITH_TIMEOUT(view->title(), "Second page", 15000);
        QVERIFY(view->history()->canGoBack());
        window.findChild<QToolButton *>("back")->click();
        QTRY_COMPARE(view->title(), "First page");
        QTRY_VERIFY(window.findChild<QToolButton *>("forward")->isEnabled());
        window.findChild<QToolButton *>("forward")->click();
        QTRY_COMPARE(view->title(), "Second page");
        QSignalSpy reload(view, &QWebEngineView::loadFinished);
        window.findChild<QToolButton *>("reload")->click();
        QTRY_VERIFY(!reload.isEmpty());
        window.findChild<QToolButton *>("home")->click();
        QTRY_COMPARE(view->url(), newTabUrl());
        QTRY_COMPARE(view->title(), "New tab");
        QVERIFY(view->page()->webChannel());
    }

    void closeUsesRecencyAfterMovingTabs()
    {
        QWebEngineProfile profile;
        BrowserWindow window(&profile);
        auto *tabs = window.findChild<QTabWidget *>("tabs");
        auto *first = tabs->currentWidget();
        auto *second = window.addTab(QUrl("about:blank"));
        auto *third = window.addTab(QUrl("about:blank"));
        tabs->setCurrentWidget(first);
        tabs->setCurrentWidget(third);
        tabs->tabBar()->moveTab(0, 2);
        auto *close = tabs->tabBar()->tabButton(tabs->indexOf(third), QTabBar::RightSide);
        QVERIFY(close);
        QTest::mouseClick(close, Qt::LeftButton);
        QCOMPARE(tabs->currentWidget(), first);
        window.closeTab(tabs->indexOf(second));
        QCOMPARE(tabs->currentWidget(), first);
        window.closeTab(tabs->currentIndex());
        QCOMPARE(tabs->count(), 1);
        QTRY_COMPARE(qobject_cast<QWebEngineView *>(tabs->currentWidget())->url(), newTabUrl());
    }

    void restoreClosedTab()
    {
        QWebEngineProfile profile;
        BrowserWindow window(&profile);
        auto *tabs = window.findChild<QTabWidget *>("tabs");
        auto *view = window.addTab(localUrl("/two"));
        QTRY_COMPARE_WITH_TIMEOUT(view->title(), "Second page", 15000);
        window.closeTab(tabs->indexOf(view));
        QCOMPARE(tabs->count(), 1);
        window.restoreClosedTab();
        QCOMPARE(tabs->count(), 2);
        auto *restored = qobject_cast<QWebEngineView *>(tabs->currentWidget());
        QTRY_COMPARE(restored->title(), "Second page");
        QCOMPARE(restored->url(), localUrl("/two"));
    }

    void invalidInputKeepsCurrentPage()
    {
        QWebEngineProfile profile;
        BrowserWindow window(&profile);
        window.show();
        auto *view = window.addTab(localUrl());
        QTRY_COMPARE_WITH_TIMEOUT(view->title(), "First page", 15000);
        auto *address = window.findChild<QLineEdit *>("address");
        address->setText("https://bad host.com");
        QTest::keyClick(address, Qt::Key_Return);
        QCOMPARE(view->url(), localUrl());
        QVERIFY(window.findChild<QWidget *>("errorBanner")->isVisible());
    }

    void toolbarNavigationUpdatesFocusedAddress()
    {
        QWebEngineProfile profile;
        BrowserWindow window(&profile);
        window.show();
        auto *view = window.addTab(localUrl());
        QTRY_COMPARE_WITH_TIMEOUT(view->title(), "First page", 15000);
        QTRY_VERIFY(!view->page()->isLoading());
        auto *address = window.findChild<QLineEdit *>("address");
        address->setFocus();
        address->selectAll();
        QTest::keyClicks(address, "unfinished draft");
        QTest::mouseClick(window.findChild<QToolButton *>("home"), Qt::LeftButton);
        QTRY_COMPARE(view->url(), newTabUrl());
        QTRY_VERIFY(address->text().isEmpty());
    }

    void failedLoadCanRecover()
    {
        QWebEngineProfile profile;
        BrowserWindow window(&profile);
        window.show();
        QTcpServer unavailable;
        QVERIFY(unavailable.listen(QHostAddress::LocalHost));
        const auto port = unavailable.serverPort();
        unavailable.close();
        auto *view = window.addTab(QUrl(QString("http://127.0.0.1:%1/").arg(port)));
        auto *banner = window.findChild<QWidget *>("errorBanner");
        QTRY_VERIFY_WITH_TIMEOUT(banner->isVisible(), 15000);
        view->load(localUrl());
        QTRY_COMPARE_WITH_TIMEOUT(view->title(), "First page", 15000);
        QVERIFY(!banner->isVisible());
    }

    void httpResponseIsNotConnectionFailure()
    {
        QWebEngineProfile profile;
        BrowserWindow window(&profile); window.show();
        auto *view = window.addTab(localUrl("/status429"));
        QTRY_COMPARE_WITH_TIMEOUT(view->title(), QString("Verification required"), 15000);
        QTRY_VERIFY(!view->page()->isLoading());
        const auto message = view->property("errorMessage").toString();
        QVERIFY2(message.contains("HTTP 429"), qPrintable(message));
        QVERIFY(!message.contains("connection"));
    }

    void stoppedLoadIsNotAnError()
    {
        QWebEngineProfile profile;
        BrowserWindow window(&profile);
        window.show();
        auto *view = window.addTab(localUrl("/slow"));
        QTRY_VERIFY(view->page()->isLoading());
        view->stop();
        QTRY_VERIFY(!view->page()->isLoading());
        QVERIFY(!window.findChild<QWidget *>("errorBanner")->isVisible());
    }

    void shortcuts()
    {
        QWebEngineProfile profile;
        BrowserWindow window(&profile);
        window.show();
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        auto *tabs = window.findChild<QTabWidget *>("tabs");
        auto *address = window.findChild<QLineEdit *>("address");
        QTest::keyClick(&window, Qt::Key_T, Qt::ControlModifier);
        QCOMPARE(tabs->count(), 2);
        address->setText("example.com");
        QTest::keyClick(&window, Qt::Key_L, Qt::ControlModifier);
        QVERIFY(address->hasFocus());
        QCOMPARE(address->selectedText(), "example.com");
        QTest::keyClick(address, Qt::Key_W, Qt::ControlModifier);
        QCOMPARE(tabs->count(), 1);
        QTest::keyClick(&window, Qt::Key_T, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(tabs->count(), 2);
    }

    void themesDoNotReloadWebsites()
    {
        QWebEngineProfile profile;
        BrowserWindow window(&profile);
        window.show();
        auto *view = window.addTab(localUrl());
        QTRY_COMPARE_WITH_TIMEOUT(view->title(), "First page", 15000);
        QSignalSpy loads(view, &QWebEngineView::loadStarted);
        window.findChild<QAction *>("themeDark")->trigger();
        QCOMPARE(window.palette().color(QPalette::Window).lightness() < 128, true);
        window.findChild<QAction *>("themeLight")->trigger();
        QCOMPARE(window.palette().color(QPalette::Window).lightness() > 128, true);
        QCOMPARE(loads.count(), 0);
        window.findChild<QAction *>("themeSystem")->trigger();
    }

    void startPageSearchAndTheme()
    {
        QWebEngineProfile profile;
        BrowserWindow window(&profile);
        window.show();
        auto *tabs = window.findChild<QTabWidget *>("tabs");
        auto *view = qobject_cast<QWebEngineView *>(tabs->currentWidget());
        QTRY_COMPARE_WITH_TIMEOUT(view->title(), "New tab", 15000);
        QTRY_VERIFY(!view->page()->isLoading());
        window.findChild<QAction *>("themeDark")->trigger();
        QString theme;
        view->page()->runJavaScript("document.documentElement.dataset.theme", [&theme](const QVariant &value) {
            theme = value.toString();
        });
        QTRY_COMPARE(theme, "dark");
        QUrl requested;
        connect(view->page(), &QWebEnginePage::navigationRequested, this,
                [&requested](QWebEngineNavigationRequest &request) {
            if (request.url().host() == "www.google.com") {
                requested = request.url();
                request.reject();
            }
        });
        view->page()->runJavaScript("document.querySelector('input').value = 'C++ & Qt';"
                                    "document.querySelector('form').requestSubmit();");
        QTRY_COMPARE_WITH_TIMEOUT(requested.host(), "www.google.com", 10000);
        QCOMPARE(requested.path(), "/search");
        QVERIFY(requested.query().contains("q=C%2B%2B+%26+Qt"));
        window.findChild<QAction *>("themeSystem")->trigger();
    }

    void linksOpenInNewTabs()
    {
        QWebEngineProfile profile;
        BrowserWindow window(&profile);
        window.show();
        auto *view = window.addTab(localUrl());
        QTRY_COMPARE_WITH_TIMEOUT(view->title(), "First page", 15000);
        auto *tabs = window.findChild<QTabWidget *>("tabs");
        view->page()->runJavaScript("document.querySelector('[target]').click()");
        QTRY_COMPARE_WITH_TIMEOUT(tabs->count(), 3, 10000);
        auto *target = qobject_cast<QWebEngineView *>(tabs->widget(2));
        QTRY_COMPARE_WITH_TIMEOUT(target->title(), "Second page", 15000);
        QCOMPARE(target->url(), localUrl("/two"));
    }

    void httpAuthentication()
    {
        QWebEngineProfile profile;
        BrowserWindow window(&profile);
        window.show();
        QTimer answer;
        answer.setInterval(25);
        connect(&answer, &QTimer::timeout, this, [&] {
            auto *dialog = window.findChild<QDialog *>("authenticationDialog");
            if (!dialog) return;
            dialog->findChild<QLineEdit *>("username")->setText("user");
            dialog->findChild<QLineEdit *>("password")->setText("pass");
            dialog->accept();
            answer.stop();
        });
        answer.start();
        auto *view = window.addTab(localUrl("/auth"));
        QTRY_COMPARE_WITH_TIMEOUT(view->title(), "First page", 15000);
        QVERIFY(!answer.isActive());
    }

    void redirectAndSharedCookies()
    {
        QWebEngineProfile profile;
        BrowserWindow window(&profile);
        window.show();
        auto *view = window.addTab(localUrl("/redirect"));
        QTRY_COMPARE(view->title(), "Second page");
        QCOMPARE(view->url(), localUrl("/two"));
        bool set = false;
        view->page()->runJavaScript("document.cookie='prism=shared; path=/';true", [&set](const QVariant &value) { set = value.toBool(); });
        QTRY_VERIFY(set);
        auto *other = window.addTab(localUrl());
        QTRY_COMPARE(other->title(), "First page");
        QString cookies;
        other->page()->runJavaScript("document.cookie", [&cookies](const QVariant &value) { cookies = value.toString(); });
        QTRY_VERIFY(cookies.contains("prism=shared"));
    }

    void permissionNeedsConsent()
    {
        QWebEngineProfile profile;
        BrowserWindow window(&profile);
        window.show();
        auto *view = window.addTab(localUrl());
        QTRY_COMPARE(view->title(), "First page");
        view->page()->runJavaScript("navigator.geolocation.getCurrentPosition(()=>{},()=>{})");
        QTRY_VERIFY(window.findChild<QMessageBox *>("permissionPrompt"));
        auto *prompt = window.findChild<QMessageBox *>("permissionPrompt");
        QVERIFY(prompt->text().contains("127.0.0.1"));
        prompt->done(QMessageBox::No);
        QTRY_VERIFY(!view->property("permissionPending").toBool());
    }

    void sleepingPreservesPageAndOffWakesIt()
    {
        QWebEngineProfile profile;
        BrowserWindow window(&profile);
        window.show();
        auto *first = window.addTab(localUrl());
        QTRY_COMPARE(first->title(), "First page");
        first->page()->runJavaScript("window.prismState = 42");
        auto *second = window.addTab(localUrl("/two"));
        QTRY_COMPARE(second->title(), "Second page");
        QTRY_VERIFY(first->page()->recommendedState() != QWebEnginePage::LifecycleState::Active);
        first->setProperty("inactiveSince", -3600000LL);
        first->setProperty("keepAwake", true);
        window.evaluateSleepingTabs();
        QCOMPARE(first->page()->lifecycleState(), QWebEnginePage::LifecycleState::Active);
        first->setProperty("keepAwake", false);
        QSignalSpy loads(first, &QWebEngineView::loadStarted);
        window.evaluateSleepingTabs();
        QCOMPARE(first->page()->lifecycleState(), QWebEnginePage::LifecycleState::Frozen);
        QCOMPARE(second->page()->lifecycleState(), QWebEnginePage::LifecycleState::Active);
        window.findChild<QAction *>("savings_off")->trigger();
        QCOMPARE(first->page()->lifecycleState(), QWebEnginePage::LifecycleState::Active);
        int value = 0;
        first->page()->runJavaScript("window.prismState", [&value](const QVariant &result) { value = result.toInt(); });
        QTRY_COMPARE(value, 42);
        QCOMPARE(loads.count(), 0);
        window.findChild<QAction *>("savings_maximum")->trigger();
        first->setProperty("inactiveSince", -61000LL);
        window.evaluateSleepingTabs();
        QCOMPARE(first->page()->lifecycleState(), QWebEnginePage::LifecycleState::Frozen);
        window.findChild<QTabWidget *>("tabs")->setCurrentWidget(first);
        QCOMPARE(first->page()->lifecycleState(), QWebEnginePage::LifecycleState::Active);
        QCOMPARE(loads.count(), 0);
    }

    void downloadSaveAndProtection()
    {
        QTemporaryDir directory;
        QWebEngineProfile profile;
        BrowserWindow window(&profile);
        window.show();
        auto *view = window.addTab(localUrl());
        QTRY_COMPARE(view->title(), "First page");
        auto *tabs = window.findChild<QTabWidget *>("tabs");
        QSignalSpy requested(&profile, &QWebEngineProfile::downloadRequested);
        view->page()->download(localUrl(), "test.html");
        QTRY_VERIFY(window.findChild<QFileDialog *>("downloadDialog"));
        const int count = tabs->count();
        window.closeTab(tabs->indexOf(view));
        QCOMPARE(tabs->count(), count);
        auto *dialog = window.findChild<QFileDialog *>("downloadDialog");
        QTRY_VERIFY(dialog->isVisible());
        auto *filename = dialog->findChild<QLineEdit *>("fileNameEdit");
        QVERIFY(filename);
        filename->setText(directory.filePath("saved.html"));
        QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
        QTRY_VERIFY(QFile::exists(directory.filePath("saved.html")));
        QFile saved(directory.filePath("saved.html"));
        QVERIFY(saved.open(QIODevice::ReadOnly));
        QVERIFY(saved.readAll().contains("First page"));
    }

    void maximumSavingsReleasesAndReloadsSafePages()
    {
        using State = QWebEnginePage::LifecycleState;
        QWebEngineProfile profile;
        BrowserWindow window(&profile); window.show();
        window.findChild<QAction *>("savings_maximum")->trigger();
        auto *view = window.addTab(localUrl());
        QTRY_COMPARE(view->title(), "First page");
        view->load(localUrl("/two"));
        QTRY_COMPARE(view->title(), "Second page");
        auto *active = window.addTab(localUrl());
        QTRY_COMPARE(active->title(), "First page");
        QTRY_VERIFY(view->page()->recommendedState() != State::Active);
        view->setProperty("inactiveSince", -3600000LL);
        view->setProperty("keepAwake", true);
        window.evaluateSleepingTabs();
        QCOMPARE(view->page()->lifecycleState(), State::Active);
        view->setProperty("keepAwake", false);
        window.evaluateSleepingTabs();
        QTRY_COMPARE(view->page()->recommendedState(), State::Discarded);
        window.evaluateSleepingTabs();
        QCOMPARE(view->page()->lifecycleState(), State::Discarded);
        QCOMPARE(active->page()->lifecycleState(), State::Active);
        QCOMPARE(view->url(), localUrl("/two"));
        QVERIFY(view->page()->history()->canGoBack());
        QSignalSpy reloaded(view, &QWebEngineView::loadFinished);
        window.findChild<QTabWidget *>("tabs")->setCurrentWidget(view);
        QTRY_VERIFY_WITH_TIMEOUT(!reloaded.isEmpty(), 15000);
        QCOMPARE(view->title(), "Second page");
        QVERIFY(view->property("errorMessage").toString().isEmpty());
        view->back(); QTRY_COMPARE(view->title(), "First page");
        view->forward(); QTRY_COMPARE(view->title(), "Second page");
        auto *tabs = window.findChild<QTabWidget *>("tabs");
        for (int cycle=0;cycle<3;++cycle) {
            tabs->setCurrentWidget(active);
            view->setProperty("inactiveSince", -3600000LL);
            window.evaluateSleepingTabs();
            QTRY_COMPARE(view->page()->recommendedState(), State::Discarded);
            window.evaluateSleepingTabs();
            QCOMPARE(view->page()->lifecycleState(), State::Discarded);
            QSignalSpy restored(view, &QWebEngineView::loadFinished);
            tabs->setCurrentWidget(view);
            QTRY_VERIFY_WITH_TIMEOUT(!restored.isEmpty(),15000);
            QCOMPARE(view->url(),localUrl("/two"));
            QVERIFY(view->property("errorMessage").toString().isEmpty());
        }
        QPointer<QWebEnginePage> releasedPage = view->page();
        window.closeTab(tabs->indexOf(view));
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        QTRY_VERIFY(releasedPage.isNull());
    }

    void editedFormsAndNewTabsAreNotDiscarded()
    {
        using State = QWebEnginePage::LifecycleState;
        QWebEngineProfile profile;
        BrowserWindow window(&profile); window.show();
        window.findChild<QAction *>("savings_maximum")->trigger();
        auto *tabs = window.findChild<QTabWidget *>("tabs");
        auto *start = qobject_cast<QWebEngineView *>(tabs->currentWidget());
        auto *form = window.addTab(localUrl());
        QTRY_COMPARE(form->title(), "First page");
        bool ready = false;
        form->page()->runJavaScript("const input=document.createElement('input');input.id='draft';document.body.append(input);input.focus();true", [&ready](const QVariant &value) {ready=value.toBool();});
        QTRY_VERIFY(ready);
        form->setFocus();
        QVERIFY(form->focusProxy());
        QTest::keyClicks(form->focusProxy(), "unsaved draft", Qt::NoModifier, 5);
        QTest::qWait(100);
        QString draft;
        form->page()->runJavaScript("document.getElementById('draft').value", [&draft](const QVariant &value) {draft=value.toString();});
        QTRY_COMPARE(draft, "unsaved draft");
        auto *active = window.addTab(localUrl("/two"));
        QTRY_COMPARE(active->title(), "Second page");
        QTRY_VERIFY(form->page()->recommendedState() != State::Active);
        // The internal page can finish after the small HTTP fixture under load.
        QTRY_VERIFY_WITH_TIMEOUT(!start->page()->isLoading() && start->page()->recommendedState() != State::Active, 15000);
        form->setProperty("inactiveSince", -3600000LL); start->setProperty("inactiveSince", -3600000LL);
        window.evaluateSleepingTabs(); QTest::qWait(100); window.evaluateSleepingTabs();
        QCOMPARE(form->page()->lifecycleState(), State::Frozen);
        QCOMPARE(start->page()->lifecycleState(), State::Frozen);
        tabs->setCurrentWidget(form);
        draft.clear();
        form->page()->runJavaScript("document.getElementById('draft').value", [&draft](const QVariant &value) {draft=value.toString();});
        QTRY_COMPARE(draft, "unsaved draft");
        window.findChild<QAction *>("savings_balanced")->trigger();
        form->setProperty("inactiveSince", -3600000LL);
        tabs->setCurrentWidget(active);
        form->setProperty("inactiveSince", -3600000LL);
        window.evaluateSleepingTabs();
        QVERIFY(!window.findChild<QTimer *>("sleepTimer")->isActive());
    }

    void longTabLabelsKeepCloseButtonsAligned()
    {
        QWebEngineProfile profile;
        BrowserWindow window(&profile); window.resize(620,440); window.show();
        auto *tabs = window.findChild<QTabWidget *>("tabs");
        window.addTab(QUrl("about:blank")); window.addTab(QUrl("about:blank"));
        auto *bar = tabs->tabBar();
        for (int i=0;i<tabs->count();++i) bar->setTabText(i, QString(180,'W'));
        QTest::qWait(100);
        QCOMPARE(bar->elideMode(), Qt::ElideRight);
        for (int i=0;i<bar->count();++i) {
            const auto tab = bar->tabRect(i);
            auto *close = bar->tabButton(i,QTabBar::RightSide);
            QVERIFY(close);
            QVERIFY(tab.adjusted(-1,-1,1,1).contains(close->geometry()));
            QVERIFY(qAbs(tab.center().y()-close->geometry().center().y())<=2);
        }
    }

    void navigationCachePreservesDocument()
    {
        QWebEngineProfile profile;
        BrowserWindow window(&profile);
        window.show();
        window.findChild<QAction *>("savings_balanced")->trigger();
        auto *view = window.addTab(localUrl());
        QTRY_COMPARE(view->title(), "First page");
        QTRY_VERIFY(!view->page()->isLoading());
        bool set = false;
        view->page()->runJavaScript("window.prismRetained=42;true", [&set](const QVariant &value) { set = value.toBool(); });
        QTRY_VERIFY(set);
        view->load(localUrl("/two"));
        QTRY_COMPARE(view->title(), "Second page");
        QTRY_VERIFY(!view->page()->isLoading());
        view->back();
        QTRY_COMPARE(view->title(), "First page");
        int retained = -1;
        view->page()->runJavaScript("window.prismRetained || 0", [&retained](const QVariant &value) { retained=value.toInt(); });
        QTRY_COMPARE(retained,42);
        window.findChild<QAction *>("savings_maximum")->trigger();
        QVERIFY(!view->settings()->testAttribute(QWebEngineSettings::BackForwardCacheEnabled));
    }

    void accentKeepsStartPageDraft()
    {
        QWebEngineProfile profile;
        BrowserWindow window(&profile);
        window.show();
        auto *tabs = window.findChild<QTabWidget *>("tabs");
        auto *view = qobject_cast<QWebEngineView *>(tabs->currentWidget());
        QTRY_COMPARE(view->title(), "New tab");
        QTRY_VERIFY(!view->page()->isLoading());
        bool ready = false;
        view->page()->runJavaScript("document.querySelector('input').value='keep this draft';true",
            [&ready](const QVariant &value) { ready=value.toBool(); });
        QTRY_VERIFY(ready);
        QSignalSpy loads(view, &QWebEngineView::loadStarted);
        window.findChild<QAction *>("accent_71b8eb")->trigger();
        QString state;
        view->page()->runJavaScript("document.documentElement.style.getPropertyValue('--accent')+'|'+document.querySelector('input').value",
            [&state](const QVariant &value) { state=value.toString(); });
        QTRY_COMPARE(state, "#71b8eb|keep this draft");
        QCOMPARE(loads.count(), 0);
        QCOMPARE(QSettings().value("appearance/accent").toString(), "#71b8eb");
        window.findChild<QAction *>("accent_9b7dff")->trigger();
    }

    void realHttpsWebsite()
    {
        if (!qEnvironmentVariableIsSet("PRISM_NETWORK_TESTS"))
            QSKIP("Set PRISM_NETWORK_TESTS=1 to check public HTTPS websites.");
        QWebEngineProfile profile;
        BrowserWindow window(&profile);
        window.show();
        auto *view = window.addTab(QUrl("https://example.com"));
        QTRY_COMPARE_WITH_TIMEOUT(view->title(), "Example Domain", 45000);
        QString body;
        view->page()->runJavaScript("document.body.innerText", [&body](const QVariant &value) {
            body = value.toString();
        });
        QTRY_VERIFY_WITH_TIMEOUT(body.contains("documentation examples"), 5000);
        qInfo("Real HTTPS page rendered: Example Domain");
    }

    void engineCapabilities()
    {
        QWebEngineProfile profile;
        QWebEngineView view(&profile); view.show();
        view.setHtml("<!doctype html><title>Engine capability probe</title>");
        QTRY_COMPARE(view.title(), "Engine capability probe");
        QVariant result;
        view.page()->runJavaScript(R"((()=>{const canvas=document.createElement('canvas');const gl=canvas.getContext('webgl');const debug=gl&&gl.getExtension('WEBGL_debug_renderer_info');const video=document.createElement('video');return {webgl:!!gl,renderer:debug?gl.getParameter(debug.UNMASKED_RENDERER_WEBGL):'unavailable',h264:video.canPlayType('video/mp4; codecs="avc1.42E01E"'),vp9:video.canPlayType('video/webm; codecs="vp9"'),av1:video.canPlayType('video/webm; codecs="av01.0.05M.08"'),ua:navigator.userAgent};})())", [&result](const QVariant &value){result=value;});
        QTRY_VERIFY(result.isValid());
        qInfo().noquote() << QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact);
    }
};

int main(int argc, char **argv)
{
    QStandardPaths::setTestModeEnabled(true);
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("PrismTests");
    QCoreApplication::setApplicationName("PrismTests");
    QTemporaryDir settings;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    WindowTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "window_tests.moc"
