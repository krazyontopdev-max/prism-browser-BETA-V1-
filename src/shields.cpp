#include "shields.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QWebEngineUrlRequestInfo>
#include <memory>

namespace {
constexpr int maximumBytes = 4 * 1024 * 1024;
const QUrl listUrl("https://raw.githubusercontent.com/hagezi/dns-blocklists/main/wildcard/light-onlydomains.txt");
QByteArray readList(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) && file.size() <= maximumBytes ? file.readAll() : QByteArray();
}
QString hostName(QString host)
{
    if (host.endsWith('.')) host.chop(1);
    return QString::fromLatin1(QUrl::toAce(host.toLower()));
}
bool domainName(const QString &host)
{
    static const QRegularExpression pattern(R"(^(?:[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\.)+[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$)");
    QHostAddress address;
    return host.size() <= 253 && pattern.match(host).hasMatch() && !address.setAddress(host);
}
bool listed(QString host, const QSet<QString> &domains)
{
    while (!host.isEmpty()) {
        if (domains.contains(host)) return true;
        const auto dot = host.indexOf('.');
        if (dot < 0) break;
        host.remove(0, dot + 1);
    }
    return false;
}
}

Shields::Shields(const QString &directory, bool privateWindow, QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent), directory_(directory), private_(privateWindow)
{
    const QString bundled = QCoreApplication::applicationDirPath() + "/filters/";
    for (auto line : readList(bundled + "public_suffix_list.dat").split('\n')) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith("//")) continue;
        if (line.startsWith('!')) suffixExceptions_.insert(hostName(QString::fromUtf8(line.mid(1))));
        else if (line.startsWith("*.")) wildcards_.insert(hostName(QString::fromUtf8(line.mid(2))));
        else suffixes_.insert(hostName(QString::fromUtf8(line)));
    }
    QSettings settings(directory_ + "/settings.ini", QSettings::IniFormat);
    const auto savedMode = settings.value("mode", "standard").toString();
    if (savedMode == "strict" || savedMode == "off") mode_ = savedMode;
    for (const auto &entry : settings.value("exceptions").toStringList()) {
        const auto host = hostName(entry);
        if (domainName(host)) exceptions_.insert(host);
    }
    if (!replaceRules(readList(directory_ + "/light.txt"), 1000)
        && !replaceRules(readList(bundled + "light.txt"), 1000))
        status_ = "Shields is unavailable: valid filter or public-suffix data is missing. Restore the filters folder.";
    if (!private_ && !QStandardPaths::isTestModeEnabled() && settings.value("lastCheck").toLongLong() + 86400 < QDateTime::currentSecsSinceEpoch())
        QTimer::singleShot(5000, this, &Shields::updateLists);
}

Shields::~Shields()
{
    for (auto *reply : network_.findChildren<QNetworkReply *>()) { reply->disconnect(this); reply->abort(); }
}

QString Shields::site(const QString &host) const
{
    const auto labels = host.split('.');
    int suffix = 1;
    for (int i = 0; i < labels.size(); ++i) {
        const auto tail = labels.mid(i).join('.');
        if (suffixExceptions_.contains(tail)) { suffix = labels.size() - i - 1; break; }
        if (suffixes_.contains(tail)) suffix = qMax(suffix, int(labels.size()) - i);
        if (i + 1 < labels.size() && wildcards_.contains(labels.mid(i + 1).join('.')))
            suffix = qMax(suffix, int(labels.size()) - i);
    }
    return labels.size() > suffix ? labels.mid(labels.size() - suffix - 1).join('.') : QString();
}

bool Shields::replaceRules(const QByteArray &bytes, int minimum)
{
    if (bytes.isEmpty() || bytes.size() > maximumBytes || suffixes_.size() < 1000) return false;
    QSet<QString> parsed;
    int entries = 0, declared = -1;
    for (auto line : bytes.split('\n')) {
        line = line.trimmed();
        if (line.startsWith("# Number of entries:")) {
            bool ok = false; declared = line.mid(20).trimmed().toInt(&ok);
            if (!ok || declared < 1 || declared > 250000) return false;
        }
        if (line.isEmpty() || line.startsWith('#')) continue;
        if (++entries > 250000) return false;
        const auto host = QString::fromLatin1(line);
        if (!domainName(host)) return false;
        // Upstream includes a few public suffixes; never block an entire shared-hosting suffix.
        if (!suffixExceptions_.contains(host) && (suffixes_.contains(host)
            || wildcards_.contains(host.mid(host.indexOf('.') + 1)))) continue;
        parsed.insert(host);
        if (parsed.size() > 250000) return false;
    }
    if (parsed.size() < minimum || (declared >= 0 && entries != declared) || (minimum >= 1000 && declared < 0)) return false;
    {
        QWriteLocker guard(&lock_);
        domains_ = std::move(parsed);
    }
    status_ = QString("%1 domain rules loaded. No cosmetic or video-ad filtering.").arg(domains_.size());
    emit changed();
    return true;
}

bool Shields::blocks(const QUrl &url, const QUrl &topLevel, bool mainFrame) const
{
    if (mainFrame || (url.scheme() != "https" && url.scheme() != "http")) return false;
    const auto host = hostName(url.host());
    const auto first = hostName(topLevel.host());
    if (first.isEmpty() || (topLevel.scheme() != "https" && topLevel.scheme() != "http")) return false;
    QReadLocker guard(&lock_);
    if (mode_ == "off" || exceptions_.contains(first) || !listed(host, domains_)) return false;
    if (mode_ == "strict") return true;
    const auto firstSite = site(first);
    return !firstSite.isEmpty() && firstSite != site(host);
}

void Shields::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    info.block(blocks(info.requestUrl(), info.firstPartyUrl(),
        info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeMainFrame));
}

void Shields::save()
{
    if (private_) return;
    QDir().mkpath(directory_);
    QSettings settings(directory_ + "/settings.ini", QSettings::IniFormat);
    settings.setValue("mode", mode());
    settings.setValue("exceptions", exceptions());
    settings.sync();
    if (settings.status() != QSettings::NoError) status_ = "Shields preferences could not be saved.";
}

void Shields::setMode(const QString &mode)
{
    if (mode != "standard" && mode != "strict" && mode != "off") return;
    { QWriteLocker guard(&lock_); mode_ = mode; }
    save(); emit changed();
}
QString Shields::mode() const { QReadLocker guard(&lock_); return mode_; }
void Shields::setException(const QString &name, bool allowed)
{
    const auto host = hostName(name);
    if (!domainName(host)) return;
    { QWriteLocker guard(&lock_); if (allowed) exceptions_.insert(host); else exceptions_.remove(host); }
    save(); emit changed();
}
QStringList Shields::exceptions() const { QReadLocker guard(&lock_); return exceptions_.values(); }
QString Shields::status() const { return status_; }

void Shields::updateLists()
{
    if (private_ || updating_) return;
    updating_ = true;
    QDir().mkpath(directory_);
    QSettings(directory_ + "/settings.ini", QSettings::IniFormat).setValue("lastCheck", QDateTime::currentSecsSinceEpoch());
    QNetworkRequest request(listUrl);
    request.setTransferTimeout(15000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    auto *reply = network_.get(request);
    auto *deadline = new QTimer(reply); deadline->setSingleShot(true);
    connect(deadline, &QTimer::timeout, reply, &QNetworkReply::abort); deadline->start(15000);
    reply->setReadBufferSize(maximumBytes + 1);
    auto buffer = std::make_shared<QByteArray>();
    connect(reply, &QNetworkReply::readyRead, this, [reply, buffer] {
        buffer->append(reply->readAll());
        if (buffer->size() > maximumBytes) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, buffer] {
        buffer->append(reply->readAll());
        const auto previous = [this] { QReadLocker guard(&lock_); return domains_; }();
        bool valid = reply->error() == QNetworkReply::NoError && reply->url() == listUrl
            && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200
            && buffer->contains("# Homepage: https://github.com/hagezi/dns-blocklists")
            && buffer->contains("# License: https://github.com/hagezi/dns-blocklists/blob/main/LICENSE")
            && replaceRules(*buffer, 1000);
        if (valid) {
            QSaveFile file(directory_ + "/light.txt");
            valid = file.open(QIODevice::WriteOnly) && file.write(*buffer) == buffer->size() && file.commit();
        }
        if (!valid) {
            { QWriteLocker guard(&lock_); domains_ = previous; }
            status_ = "Update failed; keeping the previous domain list. Retry later.";
        }
        updating_ = false;
        reply->deleteLater();
        emit changed();
    });
    status_ = "Updating the HaGeZi domain list…";
    emit changed();
}
