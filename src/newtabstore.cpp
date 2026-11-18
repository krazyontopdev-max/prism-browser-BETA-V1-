#include "newtabstore.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QImageReader>
#include <QIcon>
#include <QPainter>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QtConcurrent>
#include <QWebEngineProfile>

namespace {
QString digest(const QByteArray &bytes) { return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()); }
bool webUrl(const QUrl &url) { return url.isValid() && !url.host().isEmpty() && url.userInfo().isEmpty() && (url.scheme() == "https" || url.scheme() == "http"); }
bool secureUrl(const QUrl &url) { return webUrl(url) && url.scheme() == "https"; }
bool sameOrigin(const QUrl &a, const QUrl &b) { return a.scheme() == b.scheme() && a.host() == b.host() && a.port(a.scheme() == "https" ? 443 : 80) == b.port(b.scheme() == "https" ? 443 : 80); }
QString iconKey(const QUrl &url) { return digest("v04:" + url.toEncoded()); }
bool writeFile(const QString &path, const QByteArray &bytes) {
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}
QByteArray readFile(const QString &path) { QFile file(path); if (!file.open(QIODevice::ReadOnly) || file.size() > 512 * 1024) return {}; return file.readAll(); }
bool imageHeader(QImageReader &reader, int maxWidth, int maxHeight) {
    const auto size = reader.size();
    return size.isValid() && size.width() <= maxWidth && size.height() <= maxHeight;
}
QImage faviconImage(const QByteArray &bytes) {
    QBuffer input; input.setData(bytes); input.open(QIODevice::ReadOnly); QImageReader reader(&input);
    // SVG references are handled by Chromium's favicon decoder, not the native fallback.
    if (!QList<QByteArray>{"png", "jpeg", "ico", "gif", "webp", "bmp"}.contains(reader.format()) || !imageHeader(reader, 1024, 1024)) return {};
    reader.setScaledSize(reader.size().scaled(32, 32, Qt::KeepAspectRatio));
    return reader.read();
}
}

QStringList NewTabStore::categories() { return {"coastlines", "nature", "cities", "mountains"}; }

NewTabStore::NewTabStore(const QString &directory, QObject *parent)
    : QObject(parent), root_(directory.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/NewTab" : directory)
{
    builtinRoot_ = QCoreApplication::applicationDirPath() + "/wallpapers";
    const auto bundled = QJsonDocument::fromJson(readFile(builtinRoot_ + "/catalog.json")).object();
    if (bundled.value("version").toInt() == 1 && bundled.value("photos").toArray().size() <= 100) {
        for (const auto &value : bundled.value("photos").toArray()) {
            auto photo = value.toObject();
            const auto sha = photo.value("sha256").toString();
            if (!QRegularExpression("^[a-f0-9]{64}$").match(sha).hasMatch()
                || !categories().contains(photo.value("category").toString())
                || !secureUrl(QUrl(photo.value("sourceUrl").toString()))
                || !secureUrl(QUrl(photo.value("licenseUrl").toString()))
                || !QStringList{"CC0-1.0", "CC-BY-SA-4.0", "CC-BY-SA-3.0", "CC-BY-4.0", "CC-BY-3.0", "CC-BY-2.0"}.contains(photo.value("license").toString())
                || photo.value("photographer").toString().isEmpty()) continue;
            photo["bundled"] = true;
            builtins_.append(photo);
        }
    }
    QDir().mkpath(root_ + "/wallpapers");
    QDir().mkpath(root_ + "/custom");
    QDir().mkpath(root_ + "/icons");
    const auto saved = QJsonDocument::fromJson(readFile(root_ + "/settings.json")).object();
    settings_ = saved.value("options").toObject();
    if (!settings_.contains("mode")) settings_["mode"] = "builtinRotation";
    if (!settings_.contains("categories")) settings_["categories"] = QJsonArray::fromStringList(categories());
    if (!settings_.contains("clock24")) settings_["clock24"] = true;
    custom_ = saved.value("custom").toArray();
    shortcuts_ = saved.value("shortcuts").toArray();
    for (int i = shortcuts_.size() - 1; i >= 0; --i)
        if (!webUrl(QUrl(shortcuts_[i].toObject().value("url").toString())) || i >= 20) shortcuts_.removeAt(i);
    if (!saved.contains("shortcuts")) {
        for (const auto &site : QList<QPair<QString, QString>>{{"YouTube", "https://www.youtube.com"}, {"Discord", "https://discord.com"}, {"Spotify", "https://open.spotify.com"}, {"GitHub", "https://github.com"}, {"Reddit", "https://www.reddit.com"}, {"Notion", "https://www.notion.so"}})
            shortcuts_.append(QJsonObject{{"name", site.first}, {"url", site.second}});
    }
    QString ignored;
    catalog_ = validateCatalog(readFile(root_ + "/catalog.json"), QUrl(settings_.value("catalogUrl").toString()), &ignored);
    timer_.setSingleShot(true);
    timer_.setTimerType(Qt::PreciseTimer);
    connect(&timer_, &QTimer::timeout, this, [this] { emit changed(); schedule(); });
}

QString NewTabStore::imagePath(const QString &id, bool custom) const
{
    static const QRegularExpression hash("^[a-f0-9]{64}$");
    if (!hash.match(id).hasMatch()) return {};
    return root_ + (custom ? "/custom/" : "/wallpapers/") + id + ".webp";
}

NewTabStore::~NewTabStore()
{
    for (auto *reply : network_.findChildren<QNetworkReply *>()) { reply->disconnect(this); reply->abort(); }
    // Finish local writes before a private window removes its temporary directory.
    for (auto *watcher : findChildren<QFutureWatcherBase *>()) watcher->waitForFinished();
}

void NewTabStore::save()
{
    if (!writeFile(root_ + "/settings.json", QJsonDocument(QJsonObject{{"options", settings_}, {"custom", custom_}, {"shortcuts", shortcuts_}}).toJson()))
        status_ = "Settings could not be saved.";
    emit changed();
}

QJsonArray NewTabStore::validateCatalog(const QByteArray &bytes, const QUrl &origin, QString *error)
{
    const auto document = QJsonDocument::fromJson(bytes);
    const auto root = document.object();
    QJsonArray result;
    QSet<QString> ids;
    if (root.value("version").toInt() != 1 || !root.value("photos").isArray() || root.value("photos").toArray().size() > 100) {
        *error = "Catalog must have version 1 and at most 100 photographs.";
        return {};
    }
    for (const auto &value : root.value("photos").toArray()) {
        auto photo = value.toObject();
        photo.remove("bundled");
        photo.remove("custom");
        photo.remove("image");
        if (!photo.value("enabled").toBool(true)) continue;
        const auto id = photo.value("id").toString();
        const QUrl url(photo.value("url").toString());
        const QString license = photo.value("license").toString();
        const auto sha = photo.value("sha256").toString();
        if (id.isEmpty() || id.size() > 80 || ids.contains(id) || !categories().contains(photo.value("category").toString())
            || !secureUrl(url) || !sameOrigin(url, origin) || !QRegularExpression("^[a-f0-9]{64}$").match(sha).hasMatch()
            || !QStringList{"CC0-1.0", "CC-BY-4.0", "CC-BY-SA-4.0"}.contains(license)
            || !secureUrl(QUrl(photo.value("licenseUrl").toString())) || !secureUrl(QUrl(photo.value("sourceUrl").toString()))
            || photo.value("photographer").toString().trimmed().isEmpty() || photo.value("location").toString().trimmed().isEmpty()) {
            *error = "Catalog contains an invalid URL, duplicate ID, hash, category, or missing license/credit. Images must share the catalog's HTTPS origin.";
            return {};
        }
        ids.insert(id);
        result.append(photo);
    }
    error->clear();
    return result;
}

QJsonArray NewTabStore::availablePhotos() const
{
    QJsonArray photos = builtins_;
    QSet<QString> ids, hashes;
    for (const auto &entry : photos) {
        ids.insert(entry.toObject().value("id").toString());
        hashes.insert(entry.toObject().value("sha256").toString());
    }
    for (const auto &entry : catalog_) {
        const auto photo = entry.toObject();
        if (!ids.contains(photo.value("id").toString()) && !hashes.contains(photo.value("sha256").toString())) {
            photos.append(photo);
            ids.insert(photo.value("id").toString());
            hashes.insert(photo.value("sha256").toString());
        }
    }
    return photos;
}

QJsonObject NewTabStore::selection() const
{
    const auto mode = settings_.value("mode").toString();
    if (mode == "none") return {};
    QJsonArray pool;
    if (mode == "builtinFixed" || mode == "builtinRotation" || mode == "mixed") {
        const auto chosen = settings_.value("categories").toArray();
        for (const auto &photo : availablePhotos()) if (chosen.contains(photo.toObject().value("category"))) pool.append(photo);
    }
    if (mode == "customFixed" || mode == "customRotation" || mode == "mixed") {
        for (const auto &photo : custom_) { auto entry = photo.toObject(); entry["custom"] = true; pool.append(entry); }
    }
    if (pool.isEmpty()) return {};
    if (mode == "builtinFixed" || mode == "customFixed") {
        for (const auto &entry : pool) if (entry.toObject().value("id") == settings_.value("fixedId")) return entry.toObject();
        return pool.first().toObject();
    }
    const qint64 hour = QDateTime::currentSecsSinceEpoch() / 3600;
    const quint64 index = quint64(hour) + quint64(settings_.value("shuffle").toDouble());
    return pool.at(int(index % quint64(pool.size()))).toObject();
}

QVariantMap NewTabStore::state()
{
    const auto photos = availablePhotos();
    QJsonObject state{{"options", settings_}, {"catalog", photos}, {"custom", custom_}, {"importing", importing_}, {"builtinCount", builtins_.size()}};
    QJsonArray links;
    for (const auto &entry : shortcuts_) {
        auto link = entry.toObject();
        const auto path = root_ + "/icons/" + iconKey(QUrl(link.value("url").toString())) + ".png";
        const QFileInfo file(path);
        QUrl icon = QUrl::fromLocalFile(path);
        icon.setQuery("v=" + QString::number(file.lastModified().toMSecsSinceEpoch()));
        link["icon"] = file.exists() ? icon.toString() : QString();
        links.append(link);
    }
    state["shortcuts"] = links;
    auto selected = selection();
    const bool custom = selected.value("custom").toBool();
    const bool bundled = selected.value("bundled").toBool();
    const auto path = bundled ? builtinRoot_ + "/images/" + selected.value("sha256").toString() + ".webp"
                              : imagePath(selected.value(custom ? "id" : "sha256").toString(), custom);
    if (!path.isEmpty() && QFile::exists(path)) selected["image"] = QUrl::fromLocalFile(path).toString();
    else if (!selected.isEmpty() && !custom && !bundled) ensureWallpaper(selected);
    else if (bundled) status_ = "A bundled wallpaper is missing. Restore the wallpapers folder beside Prism.exe.";
    state["wallpaper"] = selected;
    state["status"] = status_;
    state["assetsPending"] = photos.isEmpty();
    return state.toVariantMap();
}

void NewTabStore::setOptions(const QVariantMap &options)
{
    const QString mode = options.value("mode").toString();
    if (QStringList{"none", "builtinFixed", "builtinRotation", "customFixed", "customRotation", "mixed"}.contains(mode)) settings_["mode"] = mode;
    if (options.contains("clock24")) settings_["clock24"] = options.value("clock24").toBool();
    if (options.contains("fixedId")) settings_["fixedId"] = options.value("fixedId").toString().left(80);
    if (options.contains("categories")) {
        QJsonArray valid;
        for (const auto &category : options.value("categories").toStringList()) if (categories().contains(category) && !valid.contains(category)) valid.append(category);
        settings_["categories"] = valid;
    }
    if (options.contains("catalogUrl")) {
        const auto input = options.value("catalogUrl").toString().trimmed();
        const QUrl url(input);
        if (!input.isEmpty() && !secureUrl(url)) { status_ = "Catalog URL must be HTTPS without embedded credentials."; emit changed(); return; }
        if (settings_.value("catalogUrl").toString() != input) {
            settings_["catalogUrl"] = input;
            settings_["catalogChecked"] = 0;
            catalog_ = {};
            QFile::remove(root_ + "/catalog.json");
        }
    }
    status_.clear();
    save(); schedule();
}

void NewTabStore::shuffle()
{
    const auto mode = settings_.value("mode").toString();
    if (mode == "none") { status_ = "Choose a wallpaper mode in Settings first."; emit changed(); return; }
    if (mode == "builtinFixed" || mode == "customFixed") {
        QJsonArray pool;
        if (mode == "customFixed") pool = custom_;
        else for (const auto &entry : availablePhotos()) if (settings_.value("categories").toArray().contains(entry.toObject().value("category"))) pool.append(entry);
        if (pool.isEmpty()) { status_ = "No eligible wallpapers are available."; emit changed(); return; }
        int index = -1;
        for (int i = 0; i < pool.size(); ++i) if (pool[i].toObject().value("id") == selection().value("id")) index = i;
        settings_["fixedId"] = pool[(index + 1) % pool.size()].toObject().value("id");
    } else settings_["shuffle"] = settings_.value("shuffle").toDouble() + 1;
    status_.clear(); save();
}
void NewTabStore::setActive(bool active)
{
    if (active_ == active) return;
    active_ = active;
    if (!active) { timer_.stop(); return; }
    ensureFavicons();
    if (!settings_.value("catalogUrl").toString().isEmpty() && QDateTime::currentSecsSinceEpoch() - settings_.value("catalogChecked").toDouble() >= 86400) refreshCatalog();
    emit changed(); schedule();
}
void NewTabStore::schedule()
{
    timer_.stop();
    const auto mode = settings_.value("mode").toString();
    if (active_ && (mode == "builtinRotation" || mode == "customRotation" || mode == "mixed"))
        timer_.start(int(3600000 - QDateTime::currentMSecsSinceEpoch() % 3600000));
}

void NewTabStore::addShortcut(const QString &name, const QString &input, int replace)
{
    const QUrl url = QUrl::fromUserInput(input.trimmed());
    if (!webUrl(url) || name.trimmed().isEmpty() || name.size() > 60 || input.size() > 2048 || (replace < 0 && shortcuts_.size() >= 20)) {
        status_ = "Enter a name and an HTTP/HTTPS website. Up to 20 shortcuts are supported."; emit changed(); return;
    }
    QJsonObject link{{"name", name.trimmed()}, {"url", url.toString()}};
    if (replace >= 0 && replace < shortcuts_.size()) shortcuts_.replace(replace, link);
    else if (replace == -1) shortcuts_.append(link);
    status_.clear(); save(); ensureFavicons();
}
void NewTabStore::removeShortcut(int index) { if (index >= 0 && index < shortcuts_.size()) { shortcuts_.removeAt(index); save(); } }
void NewTabStore::removeCustom(const QString &id)
{
    for (int i = 0; i < custom_.size(); ++i) if (custom_[i].toObject().value("id").toString() == id) {
        custom_.removeAt(i); QFile::remove(imagePath(id, true)); save(); return;
    }
}

void NewTabStore::fetch(const QUrl &url, qint64 limit, bool restrictOrigin, const std::function<void(QByteArray, QString, QUrl)> &done)
{
    QNetworkRequest request(url);
    request.setTransferTimeout(20000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, restrictOrigin ? QNetworkRequest::SameOriginRedirectPolicy : QNetworkRequest::NoLessSafeRedirectPolicy);
    auto *reply = network_.get(request);
    reply->setReadBufferSize(64 * 1024);
    auto data = std::make_shared<QByteArray>();
    auto exceeded = std::make_shared<bool>(false);
    connect(reply, &QIODevice::readyRead, this, [reply, data, exceeded, limit] {
        data->append(reply->readAll());
        if (data->size() > limit) { *exceeded = true; data->clear(); reply->abort(); }
    });
    connect(reply, &QNetworkReply::finished, this, [reply, data, exceeded, limit, done] {
        if (!*exceeded) data->append(reply->readAll());
        const auto error = *exceeded || data->size() > limit ? "Download exceeds the size limit." : reply->error() != QNetworkReply::NoError ? reply->errorString() : QString();
        done(error.isEmpty() ? *data : QByteArray(), error, reply->url());
        reply->deleteLater();
    });
}

void NewTabStore::refreshCatalog()
{
    const QUrl url(settings_.value("catalogUrl").toString());
    if (!secureUrl(url)) { status_ = "No published wallpaper catalog configured. Import a local image or enter a catalog URL."; emit changed(); return; }
    if (pending_.contains("catalog")) return;
    pending_.insert("catalog");
    settings_["catalogChecked"] = double(QDateTime::currentSecsSinceEpoch());
    fetch(url, 256 * 1024, true, [this, url](const QByteArray &bytes, QString error, const QUrl &) {
        pending_.remove("catalog");
        if (url.toString() != settings_.value("catalogUrl").toString()) return;
        if (error.isEmpty()) {
            const auto photos = validateCatalog(bytes, url, &error);
            if (error.isEmpty() && writeFile(root_ + "/catalog.json", bytes)) {
                catalog_ = photos;
                for (const auto &photo : photos) pending_.remove("failed:" + photo.toObject().value("sha256").toString());
            }
            else if (error.isEmpty()) error = "Catalog could not be cached.";
        }
        status_ = error.isEmpty() ? "Wallpaper catalog updated." : error;
        save();
    });
}

void NewTabStore::ensureWallpaper(const QJsonObject &entry)
{
    if (!active_) return;
    const QString sha = entry.value("sha256").toString();
    if (pending_.contains(sha) || pending_.contains("failed:" + sha)) return;
    pending_.insert(sha);
    status_ = "Downloading selected wallpaper…";
    fetch(QUrl(entry.value("url").toString()), 2 * 1024 * 1024, true, [this, sha](const QByteArray &bytes, QString error, const QUrl &) {
        if (error.isEmpty() && digest(bytes) != sha) error = "Wallpaper checksum did not match the catalog.";
        if (error.isEmpty()) {
            QBuffer buffer; buffer.setData(bytes); buffer.open(QIODevice::ReadOnly);
            QImageReader reader(&buffer);
            if (reader.format() != "webp" || !imageHeader(reader, 3840, 2160)) error = "Wallpaper must be WebP, at most 3840 × 2160 and 2 MiB.";
        }
        if (error.isEmpty() && !writeFile(imagePath(sha, false), bytes)) error = "Wallpaper could not be cached.";
        pending_.remove(sha);
        if (!error.isEmpty()) pending_.insert("failed:" + sha);
        status_ = error;
        // Only files in this app-owned cache directory are pruned.
        QDir directory(root_ + "/wallpapers");
        auto files = directory.entryInfoList({"*.webp"}, QDir::Files, QDir::Time | QDir::Reversed);
        qint64 total = 0; for (const auto &file : files) total += file.size();
        for (const auto &file : files) if (total > 128 * 1024 * 1024 && file.baseName() != sha) { total -= file.size(); QFile::remove(file.absoluteFilePath()); }
        emit changed();
    });
}

void NewTabStore::ensureFavicons()
{
    for (const auto &entry : shortcuts_) {
        const QUrl site(entry.toObject().value("url").toString());
        if (!webUrl(site)) continue;
        const QString key = iconKey(site);
        const QString path = root_ + "/icons/" + key + ".png";
        const QFileInfo failure(root_ + "/icons/" + key + ".failed");
        if (QFile::exists(path) || attemptedIcons_.contains(key) || (failure.exists() && failure.lastModified().secsTo(QDateTime::currentDateTime()) < 86400)) continue;
        attemptedIcons_.insert(key);
        if (iconProfile_) {
            iconProfile_->requestIconForPageURL(site, 32, [guard = QPointer<NewTabStore>(this), site](const QIcon &icon, const QUrl &, const QUrl &) {
                if (!guard) return;
                if (!icon.isNull() && guard->saveFavicon(site, icon.pixmap(32, 32).toImage())) return;
                guard->fetchFavicon(site);
            });
        } else fetchFavicon(site);
    }
}

void NewTabStore::setIconProfile(QWebEngineProfile *profile) { iconProfile_ = profile; }

bool NewTabStore::saveFavicon(const QUrl &site, const QImage &source)
{
    if (source.isNull()) return false;
    auto image = source.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_ARGB32);
    qint64 brightness = 0, alpha = 0;
    for (int y = 0; y < image.height(); ++y) for (int x = 0; x < image.width(); ++x) {
        const QRgb pixel = image.pixel(x, y);
        brightness += qGray(pixel) * qAlpha(pixel); alpha += qAlpha(pixel);
    }
    if (!alpha) return false;
    if (brightness / alpha < 80) {
        QImage plate(36, 36, QImage::Format_ARGB32_Premultiplied); plate.fill(Qt::white);
        QPainter painter(&plate); painter.drawImage((36-image.width())/2, (36-image.height())/2, image); painter.end();
        image = plate;
    }
    QByteArray png; QBuffer output(&png); output.open(QIODevice::WriteOnly);
    if (!image.save(&output, "PNG")) return false;
    const auto path = root_ + "/icons/" + iconKey(site) + ".png";
    if (readFile(path) == png) return true;
    if (!writeFile(path, png)) return false;
    emit changed();
    return true;
}

void NewTabStore::rememberFavicon(const QUrl &pageUrl, const QIcon &icon)
{
    if (icon.isNull() || !webUrl(pageUrl)) return;
    for (const auto &entry : shortcuts_) {
        const QUrl site(entry.toObject().value("url").toString());
        if (sameOrigin(site, pageUrl)) saveFavicon(site, icon.pixmap(32, 32).toImage());
    }
}

QList<QUrl> NewTabStore::faviconUrls(const QByteArray &bytes, const QUrl &baseUrl)
{
    QList<QUrl> urls;
    static const QRegularExpression links("<link\\b[^>]*>", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression attributes("\\b(rel|href)\\s*=\\s*(?:\"([^\"]*)\"|'([^']*)'|([^\\s>]+))", QRegularExpression::CaseInsensitiveOption);
    auto tags = links.globalMatch(QString::fromUtf8(bytes));
    while (tags.hasNext() && urls.size() < 4) {
        auto values = attributes.globalMatch(tags.next().captured()); QString rel, href;
        while (values.hasNext()) {
            const auto match = values.next();
            const auto value = !match.captured(2).isNull() ? match.captured(2) : !match.captured(3).isNull() ? match.captured(3) : match.captured(4);
            if (match.captured(1).compare("rel", Qt::CaseInsensitive) == 0) rel = value.toLower(); else href = value;
        }
        if (href.isEmpty() || (!rel.simplified().split(' ').contains("icon") && rel != "apple-touch-icon")) continue;
        href.replace("&amp;", "&");
        const auto url = baseUrl.resolved(QUrl(href));
        if (webUrl(url) && (url.scheme() == "https" || sameOrigin(url, baseUrl)) && !urls.contains(url)) urls.append(url);
    }
    return urls;
}

void NewTabStore::fetchFavicon(const QUrl &site)
{
    QUrl url = site; url.setPath("/favicon.ico"); url.setQuery(QString()); url.setFragment(QString());
    fetch(url, 256 * 1024, false, [this, site](const QByteArray &bytes, const QString &error, const QUrl &) {
        if (error.isEmpty() && saveFavicon(site, faviconImage(bytes))) return;
        QUrl home = site; home.setPath("/"); home.setQuery(QString()); home.setFragment(QString());
        fetch(home, 512 * 1024, false, [this, site](const QByteArray &html, const QString &, const QUrl &effectiveUrl) {
            tryFaviconUrls(site, faviconUrls(html, effectiveUrl));
        });
    });
}

void NewTabStore::tryFaviconUrls(const QUrl &site, const QList<QUrl> &urls, int index)
{
    if (index >= urls.size()) { writeFile(root_ + "/icons/" + iconKey(site) + ".failed", "unavailable"); emit changed(); return; }
    fetch(urls[index], 256 * 1024, false, [this, site, urls, index](const QByteArray &bytes, const QString &error, const QUrl &) {
        if (error.isEmpty() && saveFavicon(site, faviconImage(bytes))) return;
        tryFaviconUrls(site, urls, index + 1);
    });
}

void NewTabStore::importImages(const QStringList &paths)
{
    if (importing_) return;
    if (custom_.size() + paths.size() > 100) { status_ = "Up to 100 local images are supported."; emit changed(); return; }
    importing_ = true; status_ = "Importing local images…"; emit changed();
    auto *watcher = new QFutureWatcher<QJsonObject>(this);
    const QString root = root_;
    connect(watcher, &QFutureWatcher<QJsonObject>::finished, this, [this, watcher] {
        const auto result = watcher->result();
        for (const auto &entry : result.value("images").toArray()) {
            bool exists = false;
            for (const auto &old : custom_) if (old.toObject().value("id") == entry.toObject().value("id")) exists = true;
            if (!exists) custom_.append(entry);
        }
        importing_ = false; status_ = result.value("message").toString(); save(); watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([root, paths] {
        QJsonArray added; int failed = 0;
        for (const auto &path : paths) {
            if (QFileInfo(path).size() > 20 * 1024 * 1024) { ++failed; continue; }
            QImageReader reader(path); reader.setAutoTransform(true);
            if (!imageHeader(reader, 20000, 20000) || qint64(reader.size().width()) * reader.size().height() > 100000000) { ++failed; continue; }
            if (reader.size().width() > 2560 || reader.size().height() > 1440) reader.setScaledSize(reader.size().scaled(2560, 1440, Qt::KeepAspectRatio));
            auto image = reader.read(); if (image.isNull()) { ++failed; continue; }
            QByteArray bytes; QBuffer output(&bytes); output.open(QIODevice::WriteOnly);
            if (!image.save(&output, "WEBP", 85) || bytes.size() > 2 * 1024 * 1024) { ++failed; continue; }
            const QString id = digest(bytes);
            if (!writeFile(root + "/custom/" + id + ".webp", bytes)) { ++failed; continue; }
            added.append(QJsonObject{{"id", id}, {"name", QFileInfo(path).fileName()}, {"location", "Custom image"}, {"photographer", "Stored locally"}});
        }
        return QJsonObject{{"images", added}, {"message", QString("Imported %1 image(s). %2 file(s) could not be imported.").arg(added.size()).arg(failed)}};
    }));
}
