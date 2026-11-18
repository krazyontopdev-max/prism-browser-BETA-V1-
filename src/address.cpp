#include "address.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

AddressResult resolveAddress(const QString &text, const QString &engine)
{
    const QString input = text.trimmed();
    if (input.isEmpty())
        return {{}, "Enter a website address or something to search for."};
    if (input.size() > 32768)
        return {{}, "This address is too long."};

    static const QRegularExpression windowsPath(R"(^[a-zA-Z]:[\\/])");
    if (windowsPath.match(input).hasMatch() || input.startsWith('/') || input.startsWith("\\\\"))
        return {QUrl::fromLocalFile(QDir::fromNativeSeparators(input)), {}};

    const QString firstPart = input.section('/', 0, 0).section('?', 0, 0).section('#', 0, 0);
    const bool loopback = firstPart == "localhost" || firstPart.startsWith("localhost:")
        || firstPart.startsWith("127.") || firstPart.startsWith("[::1]");
    static const QRegularExpression scheme(R"(^[a-zA-Z][a-zA-Z0-9+.-]*:)");
    static const QRegularExpression hostPort(R"(^[^\s/:]+\.[^\s/:]+:[0-9]+$)");
    const bool explicitScheme = !loopback && !hostPort.match(firstPart).hasMatch()
        && scheme.match(input).hasMatch();
    const bool host = !input.contains(QRegularExpression(R"(\s)"))
        && (firstPart.contains('.') || firstPart.startsWith('[') || loopback);

    if (explicitScheme || host) {
        QUrl url(explicitScheme ? input : (loopback ? "http://" : "https://") + input,
                 QUrl::StrictMode);
        const auto protocol = url.scheme().toLower();
        if (protocol == "about" && url == QUrl("about:blank"))
            return {url, {}};
        if (protocol == "file" && url.isValid() && !url.path().isEmpty())
            return {url, {}};
        if (protocol != "http" && protocol != "https")
            return {{}, "Use an http, https, or local file address."};
        if (!url.isValid() || url.host().isEmpty()
            || (explicitScheme && !input.startsWith(protocol + "://", Qt::CaseInsensitive)))
            return {{}, "That address is not valid. Check the website name and try again."};
        return {url, {}};
    }

    return {searchUrl(input, engine), {}};
}

QUrl searchUrl(const QString &text, const QString &engine)
{
    if (text.trimmed().isEmpty() || text.size() > 32768) return {};
    QUrl search(engine == "duckduckgo" ? "https://duckduckgo.com/"
        : engine == "bing" ? "https://www.bing.com/search" : "https://www.google.com/search");
    search.setQuery("q=" + QString::fromLatin1(QUrl::toPercentEncoding(text)));
    return search;
}

QUrl newTabUrl()
{
    return QUrl("qrc:/start/index.html");
}
