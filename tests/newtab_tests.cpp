#include "newtabstore.h"
#include "newtabbridge.h"
#include "address.h"

#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QLineEdit>
#include <QImage>
#include <QImageReader>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>
#include <QSignalSpy>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>
#include <QWebChannel>
#include <QEventLoop>
#include <QTimer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QIcon>
#include <QBuffer>

class NewTabTests : public QObject {
    Q_OBJECT
private slots:
    void hiddenPagesAndRepeatedActivationAvoidUpdates()
    {
        QTemporaryDir directory;
        NewTabStore store(directory.path());
        for (int i=0;i<6;++i) store.removeShortcut(0);
        store.setOptions({{"mode","none"}});
        QSignalSpy updates(&store,&NewTabStore::changed);
        store.setActive(true);
        QCOMPARE(updates.count(),1);
        store.setActive(true);
        QCOMPARE(updates.count(),1);
        QWebEngineProfile profile;
        QWebEngineView view;
        QWebEnginePage page(&profile,&view);view.setPage(&page);
        NewTabBridge bridge(&store,&page);
        page.load(newTabUrl());QTRY_COMPARE(page.url(),newTabUrl());
        QSignalSpy delivered(&bridge,&NewTabBridge::stateChanged);
        store.setOptions({{"clock24",false}});
        QCOMPARE(delivered.count(),0);
        view.show();QTRY_VERIFY(page.isVisible());
        store.setOptions({{"clock24",true}});
        QCOMPARE(delivered.count(),1);
        view.hide();QTRY_VERIFY(!page.isVisible());
        store.setOptions({{"clock24",false}});
        QCOMPARE(delivered.count(),1);
        QVERIFY(!bridge.getState().value("options").toMap().value("clock24").toBool());
    }
    void frontendControls()
    {
        QTemporaryDir directory;
        NewTabStore store(directory.path());
        QWebEngineProfile profile;
        QWebEngineView view;
        QWebEnginePage page(&profile, &view);
        view.setPage(&page);
        QWebChannel channel(&page);
        NewTabBridge bridge(&store, &page);
        channel.registerObject("prism", &bridge);
        page.setWebChannel(&channel);
        view.resize(1100, 700);
        view.show();
        page.load(newTabUrl());
        QTRY_VERIFY(!page.isLoading());
        auto read = [&page](const QString &script) {
            auto result = std::make_shared<QVariant>();
            QEventLoop loop;
            QTimer timeout;
            timeout.setSingleShot(true);
            QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
            page.runJavaScript(script, [result, guard = QPointer<QEventLoop>(&loop)](const QVariant &value) {
                *result = value;
                if (guard) guard->quit();
            });
            timeout.start(5000); loop.exec();
            return *result;
        };
        QTRY_COMPARE(read("document.querySelectorAll('#shortcuts a').length").toInt(), 6);
        QTRY_VERIFY(read("document.getElementById('wallpaper').style.backgroundImage.includes('/wallpapers/images/')").toBool());
        read("window.builtinLoaded=false; const builtin=new Image(); builtin.onload=()=>window.builtinLoaded=true; builtin.src=document.getElementById('wallpaper').style.backgroundImage.slice(5,-2)");
        QTRY_VERIFY(read("window.builtinLoaded").toBool());
        QVERIFY(read("document.getElementById('clock').textContent").toString() != "--:--");
        read("document.getElementById('settings').click()");
        QVERIFY(read("document.getElementById('options').open").toBool());
        read("document.getElementById('clock24').click()");
        QTRY_VERIFY(!store.state().value("options").toMap().value("clock24").toBool());
        read("document.querySelector('#categories input').click()");
        QTRY_VERIFY(!store.state().value("options").toMap().value("categories").toStringList().contains("coastlines"));
        read("document.getElementById('addInSettings').click(); document.getElementById('shortcutName').value='Example'; document.getElementById('shortcutUrl').value='https://example.com'; document.getElementById('shortcutForm').requestSubmit()");
        QTRY_COMPARE(store.state().value("shortcuts").toList().size(), 7);
        QTRY_COMPARE(read("document.querySelectorAll('#shortcuts a').length").toInt(), 7);
        QImage fixture(160, 90, QImage::Format_RGB32); fixture.fill(Qt::green);
        const auto path = directory.filePath("fixture.png");
        QVERIFY(fixture.save(path));
        QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
        read("document.getElementById('import').click()");
        QTRY_VERIFY(view.findChild<QFileDialog *>("wallpaperImportDialog"));
        auto *picker = view.findChild<QFileDialog *>("wallpaperImportDialog");
        auto *filename = picker->findChild<QLineEdit *>("fileNameEdit");
        QVERIFY(filename);
        filename->setText(path);
        QCOMPARE(picker->selectedFiles(), QStringList{path});
        QSignalSpy selected(picker, &QFileDialog::filesSelected);
        QVERIFY(QMetaObject::invokeMethod(picker, "accept"));
        QCOMPARE(selected.count(), 1);
        QTRY_COMPARE(store.state().value("custom").toList().size(), 1);
        read("document.getElementById('closeSettings').click()");
        store.setOptions({{"mode", "customFixed"}});
        QTRY_VERIFY(read("document.getElementById('wallpaper').style.backgroundImage.includes('/custom/')").toBool());
        read("window.photoLoaded=false; const p=new Image(); p.onload=()=>window.photoLoaded=true; p.src=document.getElementById('wallpaper').style.backgroundImage.slice(5,-2)");
        QTRY_VERIFY(read("window.photoLoaded").toBool());
        view.resize(480, 600);
        QVERIFY(read("document.documentElement.scrollWidth<=innerWidth").toBool());
        view.resize(420, 300);
        QTRY_VERIFY(read("document.querySelector('main').getBoundingClientRect().bottom<=document.querySelector('footer').getBoundingClientRect().top").toBool());
        QVERIFY(read("getComputedStyle(document.getElementById('wallpaper')).backgroundSize==='cover'").toBool());
        QSignalSpy fullscreen(&bridge, &NewTabBridge::fullscreenRequested);
        read("document.getElementById('fullscreen').click()");
        QTRY_COMPARE(fullscreen.count(), 1);
    }
    void bundledWallpapersWorkOffline()
    {
        QTemporaryDir directory;
        NewTabStore store(directory.path());
        const auto state = store.state();
        QVERIFY(!state.value("assetsPending").toBool());
        QCOMPARE(state.value("builtinCount").toInt(), 40);
        const auto catalog = state.value("catalog").toList();
        QCOMPARE(catalog.size(), 40);
        QMap<QString, int> counts;
        QSet<QString> hashes;
        for (const auto &value : catalog) {
            const auto photo = value.toMap();
            ++counts[photo.value("category").toString()];
            const auto sha = photo.value("sha256").toString();
            hashes.insert(sha);
            store.setOptions({{"mode", "builtinFixed"}, {"fixedId", photo.value("id")}});
            const auto selected = store.state().value("wallpaper").toMap();
            QCOMPARE(selected.value("id"), photo.value("id"));
            const QUrl url(selected.value("image").toString());
            QVERIFY(url.isLocalFile());
            QFile file(url.toLocalFile());
            QVERIFY(file.open(QIODevice::ReadOnly));
            QVERIFY(file.size() <= 2000000);
            QCOMPARE(QString::fromLatin1(QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex()), sha);
            QImageReader reader(url.toLocalFile());
            QCOMPARE(reader.format(), QByteArray("webp"));
            QCOMPARE(reader.size(), QSize(2560, 1440));
            QVERIFY(!reader.read().isNull());
            QVERIFY(!photo.value("photographer").toString().isEmpty());
            QVERIFY(!photo.value("changes").toString().isEmpty());
            const auto license = QCoreApplication::applicationDirPath() + "/wallpapers/licenses/" + photo.value("license").toString() + ".html";
            QVERIFY(QFile::exists(license));
        }
        QCOMPARE(hashes.size(), 40);
        for (const auto &category : NewTabStore::categories()) {
            QCOMPARE(counts.value(category), 10);
            store.setOptions({{"mode", "builtinRotation"}, {"categories", QStringList{category}}});
            QSet<QString> selectedIds;
            for (int i = 0; i < 10; ++i) {
                const auto photo = store.state().value("wallpaper").toMap();
                QCOMPARE(photo.value("category").toString(), category);
                QVERIFY(QUrl(photo.value("image").toString()).isLocalFile());
                selectedIds.insert(photo.value("id").toString());
                store.shuffle();
            }
            QCOMPARE(selectedIds.size(), 10);
        }
        store.setOptions({{"mode", "none"}});
        QVERIFY(store.state().value("wallpaper").toMap().isEmpty());
        store.setOptions({{"mode", "builtinRotation"}});
        QCOMPARE(state.value("options").toMap().value("mode").toString(), "builtinRotation");
        QCOMPARE(state.value("shortcuts").toList().size(), 6);
    }
    void validateCatalog()
    {
        QJsonObject photo{{"id", "coast-1"}, {"category", "coastlines"}, {"url", "https://photos.example/a.webp"},
            {"sha256", QString(64, 'a')}, {"license", "CC0-1.0"}, {"licenseUrl", "https://creativecommons.org/publicdomain/zero/1.0/"},
            {"sourceUrl", "https://source.example/photo"}, {"photographer", "Test fixture author"}, {"location", "Test fixture"}};
        auto bytes = [&photo] { return QJsonDocument(QJsonObject{{"version", 1}, {"photos", QJsonArray{photo}}}).toJson(); };
        QString error;
        QCOMPARE(NewTabStore::validateCatalog(bytes(), QUrl("https://photos.example/catalog.json"), &error).size(), 1);
        QVERIFY(error.isEmpty());
        photo["bundled"] = true;
        photo["image"] = "file:///untrusted.webp";
        const auto sanitized = NewTabStore::validateCatalog(bytes(), QUrl("https://photos.example/catalog.json"), &error).first().toObject();
        QVERIFY(!sanitized.contains("bundled"));
        QVERIFY(!sanitized.contains("image"));
        photo["url"] = "https://unapproved.example/a.webp";
        QVERIFY(NewTabStore::validateCatalog(bytes(), QUrl("https://photos.example/catalog.json"), &error).isEmpty());
        QVERIFY(!error.isEmpty());
        photo["url"] = "https://photos.example/a.webp";
        photo["license"] = "unknown";
        QVERIFY(NewTabStore::validateCatalog(bytes(), QUrl("https://photos.example/catalog.json"), &error).isEmpty());
    }
    void localImagesAndSharedRotation()
    {
        QTemporaryDir directory;
        QImage image(100, 60, QImage::Format_RGB32);
        image.fill(Qt::red);
        const auto red = directory.filePath("red-fixture.png");
        QVERIFY(image.save(red));
        image.fill(Qt::blue);
        const auto blue = directory.filePath("blue-fixture.png");
        QVERIFY(image.save(blue));
        const QString root = directory.filePath("state");
        QString firstId;
        {
            NewTabStore store(root);
            store.importImages({red, blue});
            QTRY_COMPARE_WITH_TIMEOUT(store.state().value("custom").toList().size(), 2, 15000);
            store.setOptions({{"mode", "customRotation"}, {"clock24", false}});
            const auto first = store.state().value("wallpaper").toMap();
            QVERIFY(!first.value("image").toString().isEmpty());
            QCOMPARE(store.state().value("wallpaper"), store.state().value("wallpaper"));
            firstId = first.value("id").toString();
            store.shuffle();
            QVERIFY(store.state().value("wallpaper").toMap().value("id").toString() != firstId);
            store.setOptions({{"mode", "customFixed"}, {"fixedId", firstId}});
            QCOMPARE(store.state().value("wallpaper").toMap().value("id").toString(), firstId);
            store.setOptions({{"mode", "mixed"}, {"categories", QStringList{"coastlines"}}});
            QSet<QString> mixed;
            for (int i = 0; i < 12; ++i) {
                const auto photo = store.state().value("wallpaper").toMap();
                QVERIFY(photo.value("custom").toBool() || photo.value("category").toString() == "coastlines");
                mixed.insert(photo.value("id").toString());
                store.shuffle();
            }
            QCOMPARE(mixed.size(), 12);
            store.setOptions({{"mode", "none"}});
            QVERIFY(store.state().value("wallpaper").toMap().isEmpty());
        }
        NewTabStore reloaded(root);
        QCOMPARE(reloaded.state().value("custom").toList().size(), 2);
        QVERIFY(!reloaded.state().value("options").toMap().value("clock24").toBool());
        reloaded.removeCustom(firstId);
        QCOMPARE(reloaded.state().value("custom").toList().size(), 1);
        QVERIFY(QFile::exists(red));
        QVERIFY(QFile::exists(blue));
    }
    void shortcutsRejectExecutableUrls()
    {
        QTemporaryDir directory;
        NewTabStore store(directory.path());
        const auto count = store.state().value("shortcuts").toList().size();
        store.addShortcut("Unsafe", "javascript:alert(1)");
        QCOMPARE(store.state().value("shortcuts").toList().size(), count);
        store.addShortcut("Example", "https://example.com");
        QCOMPARE(store.state().value("shortcuts").toList().size(), count + 1);
        store.removeShortcut(count);
        QCOMPARE(store.state().value("shortcuts").toList().size(), count);
    }
    void faviconDiscoveryAndCache()
    {
        const auto urls = NewTabStore::faviconUrls("<link href='/brand.png' rel='shortcut icon'><link rel=icon href='https://cdn.example/icon.png'><link rel=icon href='file:///private.png'><link rel=icon href='javascript:bad'><link rel=icon href='http://other.example/icon.png'>", QUrl("https://site.example/landing"));
        QCOMPARE(urls, QList<QUrl>({QUrl("https://site.example/brand.png"),QUrl("https://cdn.example/icon.png")}));
        QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost));
        QImage icon(64,64,QImage::Format_ARGB32); icon.fill(Qt::black);
        QByteArray png; QBuffer buffer(&png); buffer.open(QIODevice::WriteOnly); QVERIFY(icon.save(&buffer,"PNG"));
        int requests=0;
        connect(&server,&QTcpServer::newConnection,this,[&]{while(auto *socket=server.nextPendingConnection()) {
            connect(socket,&QTcpSocket::readyRead,socket,[&,socket]{
                const auto data=socket->readAll(); if(!data.contains("\r\n\r\n"))return; ++requests;
                const auto path=data.split(' ').value(1);
                const bool missing=path=="/favicon.ico";
                const QByteArray body=path=="/brand.png"?png:QByteArray("<link href='/brand.png' rel='icon'>");
                socket->write((missing?QByteArray("HTTP/1.1 404 Not Found\r\n"):QByteArray("HTTP/1.1 200 OK\r\n"))+"Content-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);socket->disconnectFromHost();
            });
            connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        }});
        QTemporaryDir directory; NewTabStore store(directory.path());
        for(int i=0;i<6;++i)store.removeShortcut(0);
        store.addShortcut("Actual site",QString("http://127.0.0.1:%1/").arg(server.serverPort()));
        auto iconUrl=[&]{return QUrl(store.state().value("shortcuts").toList().first().toMap().value("icon").toString());};
        QTRY_VERIFY(!iconUrl().isEmpty());
        QCOMPARE(requests,3);
        QImage cached(iconUrl().toLocalFile()); QVERIFY(!cached.isNull());
        QCOMPARE(cached.pixelColor(0,0),QColor(Qt::white));
        QCOMPARE(cached.pixelColor(cached.width()/2,cached.height()/2),QColor(Qt::black));
        store.setActive(true);store.setActive(true);QTest::qWait(100);
        QCOMPARE(requests,3);
    }
    void bridgeRejectsRemotePages()
    {
        QTemporaryDir directory;
        NewTabStore store(directory.path());
        QWebEngineProfile profile;
        QWebEnginePage page(&profile);
        NewTabBridge bridge(&store, &page);
        QVERIFY(bridge.getState().isEmpty());
        bridge.setOptions({{"mode", "none"}});
        QCOMPARE(store.state().value("options").toMap().value("mode").toString(), "builtinRotation");
        page.load(newTabUrl());
        QTRY_COMPARE(page.url(), newTabUrl());
        QVERIFY(!bridge.getState().isEmpty());
        page.load(QUrl("about:blank"));
        QTRY_COMPARE(page.url(), QUrl("about:blank"));
        QVERIFY(bridge.getState().isEmpty());
    }
};

QTEST_MAIN(NewTabTests)
#include "newtab_tests.moc"
