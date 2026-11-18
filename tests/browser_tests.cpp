#include "address.h"

#include <QTest>
#include <QUrlQuery>

class BrowserTests : public QObject {
    Q_OBJECT

private slots:
    void configuredSearchEngine()
    {
        QCOMPARE(resolveAddress("cats & dogs", "duckduckgo").url, QUrl("https://duckduckgo.com/?q=cats%20%26%20dogs"));
        QCOMPARE(resolveAddress("cats", "bing").url, QUrl("https://www.bing.com/search?q=cats"));
        QCOMPARE(resolveAddress("example.com", "bing").url, QUrl("https://example.com"));
    }

    void addresses_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");
        QTest::newRow("https") << "https://example.com/a?b=2" << "https://example.com/a?b=2";
        QTest::newRow("hostname") << "example.com" << "https://example.com";
        QTest::newRow("hostname-port") << "example.com:8080/path" << "https://example.com:8080/path";
        QTest::newRow("trim") << "  example.com/path  " << "https://example.com/path";
        QTest::newRow("localhost") << "localhost:8080/test" << "http://localhost:8080/test";
        QTest::newRow("ipv4") << "127.0.0.1:9000" << "http://127.0.0.1:9000";
        QTest::newRow("ipv6") << "[::1]:8080" << "http://[::1]:8080";
        QTest::newRow("blank") << "about:blank" << "about:blank";
        QTest::newRow("file") << "C:\\Users\\user\\page.html" << "file:///C:/Users/user/page.html";
    }

    void addresses()
    {
        QFETCH(QString, input);
        QFETCH(QString, expected);
        const auto result = resolveAddress(input);
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.url, QUrl(expected));
    }

    void queries_data()
    {
        QTest::addColumn<QString>("input");
        QTest::newRow("words") << "light through a prism";
        QTest::newRow("punctuation") << "C++ & Qt #widgets";
        QTest::newRow("unicode") << QString::fromUtf8("café 日本語");
        QTest::newRow("one-word") << "prism";
    }

    void queries()
    {
        QFETCH(QString, input);
        const auto result = resolveAddress(input);
        QVERIFY(result.error.isEmpty());
        QCOMPARE(result.url.host(), "www.google.com");
        QCOMPARE(result.url.path(), "/search");
        QCOMPARE(QUrlQuery(result.url).queryItemValue("q", QUrl::FullyDecoded), input);
    }

    void rejectsInvalidAddresses_data()
    {
        QTest::addColumn<QString>("input");
        QTest::newRow("empty") << "   ";
        QTest::newRow("missing-host") << "https://";
        QTest::newRow("bad-host") << "https://bad host.com";
        QTest::newRow("bad-port") << "https://example.com:99999";
        QTest::newRow("script") << "javascript:alert(1)";
        QTest::newRow("data") << "data:text/html,hello";
        QTest::newRow("unsupported") << "ftp://example.com";
    }

    void rejectsInvalidAddresses()
    {
        QFETCH(QString, input);
        const auto result = resolveAddress(input);
        QVERIFY(!result.error.isEmpty());
        QVERIFY(result.url.isEmpty());
    }
};

QTEST_MAIN(BrowserTests)
#include "browser_tests.moc"
