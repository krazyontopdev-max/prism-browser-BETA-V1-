#include "shields.h"
#include "browserdata.h"
#include "webdata.h"
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QSignalSpy>
#include <QProcess>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include <QTest>
#include <QTemporaryDir>

class PrivacyTests : public QObject {
    Q_OBJECT
private slots:
    void bookmarkImportIsTransactional()
    {
        QTemporaryDir directory; BrowserData data(directory.path(), false);
        const QByteArray valid = "[{\"title\":\"Manual\",\"url\":\"https://example.com/a%20b\",\"folder\":\"Work/Docs\"}]";
        QVERIFY(data.importBookmarks(valid));
        QCOMPARE(data.bookmarks().size(), 1);
        const auto before = data.exportBookmarks();
        const QByteArray invalid = "[{\"title\":\"Bad\",\"url\":\"javascript:alert(1)\"}]";
        QVERIFY(!data.importBookmarks(invalid));
        QCOMPARE(data.exportBookmarks(), before);
        QVERIFY(data.importBookmarks(before)); QCOMPARE(data.bookmarks().size(), 1);
        QCOMPARE(data.bookmarks().first().toObject().value("folder").toString(), QString("Work/Docs"));
    }

    void bookmarksPersistAndRejectUnsafeSchemes()
    {
        QTemporaryDir directory;
        {
            BrowserData data(directory.path(), false);
            QVERIFY(data.setBookmark({}, "Example", QUrl("https://example.com/")));
            QVERIFY(!data.setBookmark({}, "Script", QUrl("javascript:alert(1)")));
            QCOMPARE(data.bookmarks().size(), 1);
        }
        BrowserData stored(directory.path(), false);
        QCOMPARE(stored.bookmarks().size(), 1);
        const auto id = stored.bookmarks().first().toObject().value("id").toString();
        QVERIFY(stored.setBookmark(id, "Edited", QUrl("https://example.org/")));
        QCOMPARE(stored.bookmarks().first().toObject().value("title").toString(), QString("Edited"));
        QVERIFY(stored.removeBookmarks({id})); QVERIFY(stored.bookmarks().isEmpty());
        BrowserData privateData(directory.path(), true);
        QVERIFY(privateData.setBookmark({}, "Private", QUrl("https://private.test")));
        BrowserData unchanged(directory.path(), false); QVERIFY(unchanged.bookmarks().isEmpty());
    }

    void resetRefusesNestedJunctions()
    {
#ifdef Q_OS_WIN
        QTemporaryDir directory, outside;
        const auto storage = directory.path() + "/QtWebEngine/Prism";
        QVERIFY(QDir().mkpath(storage));
        QFile sentinel(outside.path() + "/sentinel"); QVERIFY(sentinel.open(QIODevice::WriteOnly)); sentinel.write("keep"); sentinel.close();
        const auto junction = storage + "/linked";
        QCOMPARE(QProcess::execute("cmd.exe", {"/c", "mklink", "/J", QDir::toNativeSeparators(junction), QDir::toNativeSeparators(outside.path())}), 0);
        QString error;
        const bool reset = resetWebData({storage}, directory.path(), &error);
        const bool kept = QFileInfo::exists(sentinel.fileName());
        if (QFileInfo(junction).isJunction()) RemoveDirectoryW(reinterpret_cast<LPCWSTR>(junction.utf16()));
        QVERIFY(!reset);
        QVERIFY(kept);
#endif
    }

    void historySelectionIsRemovedOnce()
    {
        QTemporaryDir directory;
        BrowserData data(directory.path(), false);
        for (const auto &url : {"https://one.test", "https://two.test", "https://three.test"}) data.visit(QUrl(url), "Visit");
        QSignalSpy changed(&data, &BrowserData::changed);
        data.removeVisits({1, 1, -1, 7});
        QCOMPARE(changed.count(), 1);
        QCOMPARE(data.history().size(), 2);
    }

    void historyFileAlwaysFitsItsReader()
    {
        QTemporaryDir directory;
        {
            BrowserData data(directory.path(), false);
            for (int i = 0; i < 350; ++i)
                data.visit(QUrl("https://example.com/" + QString::number(i) + QString(30000, 'x')), "Visit");
        }
        QVERIFY(QFileInfo(directory.path() + "/history.json").size() <= 8 * 1024 * 1024);
        BrowserData reloaded(directory.path(), false);
        QVERIFY(reloaded.error().isEmpty());
        QVERIFY(!reloaded.history().isEmpty());
    }

    void localHistoryAndPrivateHistory()
    {
        QTemporaryDir directory;
        {
            BrowserData data(directory.path(), false);
            data.visit(QUrl("https://name:secret@example.com/page#fragment"), "Page");
            QCOMPARE(data.history().size(), 1);
            QCOMPARE(data.history().first().toObject().value("url").toString(), QString("https://example.com/page"));
        }
        {
            BrowserData data(directory.path(), true);
            QVERIFY(data.history().isEmpty());
            data.visit(QUrl("https://private.test"), "Private");
            QVERIFY(data.history().isEmpty());
        }
        BrowserData reloaded(directory.path(), false);
        QCOMPARE(reloaded.history().size(), 1);
        reloaded.clearHistory();
        BrowserData cleared(directory.path(), false);
        QVERIFY(cleared.history().isEmpty());
    }

    void websiteResetCannotRemoveUnrelatedFiles()
    {
        QTemporaryDir directory;
        QDir().mkpath(directory.path() + "/QtWebEngine/Prism/IndexedDB");
        QDir().mkpath(directory.path() + "/NewTab");
        QFile keep(directory.path() + "/NewTab/keep.txt"); QVERIFY(keep.open(QIODevice::WriteOnly)); keep.write("keep"); keep.close();
        QString error;
        QVERIFY(!resetWebData({directory.path()}, directory.path(), &error));
        QVERIFY(!resetWebData({directory.path() + "/NewTab"}, directory.path(), &error));
        QVERIFY(resetWebData({directory.path() + "/QtWebEngine/Prism"}, directory.path(), &error));
        QVERIFY(!QFileInfo::exists(directory.path() + "/QtWebEngine/Prism"));
        QVERIFY(QFileInfo::exists(keep.fileName()));
    }

    void bundledListBlocksNamedTrackers()
    {
        QTemporaryDir directory;
        Shields shields(directory.path(), false);
        QVERIFY2(shields.blocks(QUrl("https://www.google-analytics.com/g/collect"), QUrl("https://news.example")), qPrintable(shields.status()));
        QVERIFY(shields.blocks(QUrl("https://analytics.tiktok.com/pixel"), QUrl("https://news.example")));
        QVERIFY(shields.blocks(QUrl("https://bat.bing.com/action"), QUrl("https://news.example")));
        QVERIFY(!shields.blocks(QUrl("https://unrelated.3utilities.com"), QUrl("https://news.example")));
    }

    void blockingBoundariesAndExceptions()
    {
        QTemporaryDir directory;
        Shields shields(directory.path(), false);
        QVERIFY(shields.replaceRules("ads.example.com\ntracker.test\nco.uk\n"));
        QVERIFY(!shields.blocks(QUrl("https://ordinary.co.uk"), QUrl("https://news.test")));
        QVERIFY(shields.blocks(QUrl("https://ads.example.com/banner"), QUrl("https://news.test")));
        QVERIFY(shields.blocks(QUrl("https://sub.tracker.test/pixel"), QUrl("https://news.test")));
        QVERIFY(!shields.blocks(QUrl("https://nottracker.test"), QUrl("https://news.test")));
        QVERIFY(!shields.blocks(QUrl("https://tracker.test"), QUrl("https://news.test"), true));
        QVERIFY(!shields.blocks(QUrl("https://ads.example.com"), QUrl("https://www.example.com")));
        shields.setMode("strict");
        QVERIFY(shields.blocks(QUrl("https://ads.example.com"), QUrl("https://www.example.com")));
        shields.setException("www.example.com", true);
        QVERIFY(!shields.blocks(QUrl("https://ads.example.com"), QUrl("https://www.example.com")));
        shields.setMode("off");
        QVERIFY(!shields.blocks(QUrl("https://tracker.test"), QUrl("https://news.test")));
    }

    void invalidUpdatePreservesRulesAndPrivateChangesStayLocal()
    {
        QTemporaryDir directory;
        Shields shields(directory.path(), false);
        QVERIFY(shields.replaceRules("tracker.test\n"));
        QVERIFY(!shields.replaceRules("<html>proxy error</html>"));
        QVERIFY(!shields.replaceRules("127.0.0.1\n"));
        QVERIFY(!shields.replaceRules("com\n"));
        QVERIFY(!shields.replaceRules("# Number of entries: 2\ntracker.test\n"));
        QVERIFY(shields.blocks(QUrl("https://tracker.test"), QUrl("https://news.test")));
        shields.setMode("strict");
        shields.setException("news.test", true);
        {
            Shields privateShields(directory.path(), true);
            QCOMPARE(privateShields.mode(), QString("strict"));
            privateShields.setMode("off");
            privateShields.setException("other.test", true);
        }
        Shields persisted(directory.path(), false);
        QCOMPARE(persisted.mode(), QString("strict"));
        QVERIFY(!persisted.exceptions().contains("other.test"));
    }
};

QTEST_GUILESS_MAIN(PrivacyTests)
#include "privacy_tests.moc"
