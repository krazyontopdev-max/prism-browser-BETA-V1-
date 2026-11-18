#include "browserdata.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <algorithm>
#include <QUuid>
#include <QSet>

BrowserData::BrowserData(const QString &directory, bool privateWindow, QObject *parent)
    : QObject(parent), directory_(directory), private_(privateWindow)
{
    if (!private_) {
        QFile file(directory_ + "/history.json");
        if (file.exists()) {
            if (file.open(QIODevice::ReadOnly) && file.size() <= 8 * 1024 * 1024) {
                QJsonParseError parse;
                const auto document = QJsonDocument::fromJson(file.readAll(), &parse);
                if (parse.error == QJsonParseError::NoError && document.isArray() && document.array().size() <= 5000) {
                    for (const auto &value : document.array()) {
                        const auto row = value.toObject();
                        const QUrl url(row.value("url").toString());
                        if ((url.scheme() == "https" || url.scheme() == "http") && url.isValid() && !url.host().isEmpty())
                            history_.append(row);
                    }
                } else error_ = "History could not be read; the existing file will not be overwritten.";
            } else error_ = "History could not be opened; the existing file will not be overwritten.";
        }
    }
    readOnly_ = !error_.isEmpty();
    if (!private_) {
        QFile file(directory_ + "/bookmarks.json");
        if (file.exists()) {
            QJsonParseError parse;
            QJsonDocument document;
            if (file.open(QIODevice::ReadOnly) && file.size() <= 4 * 1024 * 1024)
                document = QJsonDocument::fromJson(file.readAll(), &parse);
            if (!document.isArray() || parse.error != QJsonParseError::NoError || document.array().size() > 5000) bookmarksReadOnly_ = true;
            else {
                for (const auto &value : document.array()) {
                    const auto row = value.toObject(); const QUrl url(row.value("url").toString());
                    if (!url.isValid() || url.host().isEmpty() || (url.scheme() != "http" && url.scheme() != "https")
                        || row.value("id").toString().isEmpty() || row.value("title").toString().size() > 1024 || url.toString().size() > 32768) bookmarksReadOnly_ = true;
                }
                if (!bookmarksReadOnly_) bookmarks_ = document.array();
            }
            if (bookmarksReadOnly_) bookmarkError_ = "Bookmarks could not be read; the existing file will not be overwritten.";
        }
    }
    timer_.setSingleShot(true); timer_.setInterval(1000); timer_.setTimerType(Qt::CoarseTimer);
    connect(&timer_, &QTimer::timeout, this, &BrowserData::save);
}
BrowserData::~BrowserData() { if (timer_.isActive()) save(); }
void BrowserData::visit(const QUrl &url, const QString &title)
{
    if (private_ || !url.isValid() || url.host().isEmpty() || (url.scheme() != "http" && url.scheme() != "https")) return;
    const auto safe = url.adjusted(QUrl::RemoveUserInfo | QUrl::RemoveFragment).toString(QUrl::FullyEncoded);
    if (safe.size() > 32768) return;
    if (!history_.isEmpty() && history_.last().toObject().value("url").toString() == safe) history_.removeLast();
    history_.append(QJsonObject{{"url", safe}, {"title", title.left(1024)}, {"time", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}});
    while (history_.size() > 5000) history_.removeFirst();
    timer_.start(); emit changed();
}
QJsonArray BrowserData::history() const { return history_; }
void BrowserData::removeVisits(QList<int> indexes)
{
    std::sort(indexes.begin(), indexes.end(), std::greater<int>());
    indexes.erase(std::unique(indexes.begin(), indexes.end()), indexes.end());
    for (int index : indexes) if (index >= 0 && index < history_.size()) history_.removeAt(index);
    timer_.start(); emit changed();
}
void BrowserData::clearHistory()
{
    history_ = {}; error_.clear(); readOnly_ = false; timer_.stop(); save(); emit changed();
}
QString BrowserData::error() const { return bookmarkError_.isEmpty() ? error_ : bookmarkError_; }
QJsonArray BrowserData::bookmarks() const { return bookmarks_; }
bool BrowserData::setBookmark(const QString &id, const QString &title, const QUrl &url, const QString &folder)
{
    if (bookmarksReadOnly_ || !url.isValid() || url.host().isEmpty() || (url.scheme() != "http" && url.scheme() != "https")
        || url.toString().size() > 32768 || title.size() > 1024 || folder.size() > 256 || folder.contains('\n') || folder.contains('\r') || (id.isEmpty() && bookmarks_.size() >= 5000)) return false;
    const auto previous = bookmarks_;
    int index = -1;
    for (int i = 0; i < bookmarks_.size(); ++i) if (bookmarks_[i].toObject().value("id").toString() == id) { index = i; break; }
    if (!id.isEmpty() && index < 0) return false;
    const QJsonObject row{{"id", id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : id},
        {"title", title.trimmed().isEmpty() ? url.host() : title.trimmed()}, {"url", url.adjusted(QUrl::RemoveUserInfo).toString(QUrl::FullyEncoded)}, {"folder", folder.trimmed()}};
    if (index >= 0) bookmarks_[index] = row; else bookmarks_.append(row);
    if (!saveBookmarks()) { bookmarks_ = previous; return false; }
    emit changed(); return true;
}
bool BrowserData::removeBookmarks(const QStringList &ids)
{
    if (bookmarksReadOnly_) return false;
    const auto previous = bookmarks_;
    for (int i = bookmarks_.size() - 1; i >= 0; --i)
        if (ids.contains(bookmarks_[i].toObject().value("id").toString())) bookmarks_.removeAt(i);
    if (!saveBookmarks()) { bookmarks_ = previous; return false; }
    emit changed(); return true;
}
bool BrowserData::saveBookmarks()
{
    if (private_) return true;
    QDir().mkpath(directory_);
    const auto bytes = QJsonDocument(bookmarks_).toJson(QJsonDocument::Compact);
    QSaveFile file(directory_ + "/bookmarks.json");
    if (bytes.size() > 4 * 1024 * 1024 || !file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        bookmarkError_ = "Bookmarks could not be saved (limit: 5000 entries / 4 MiB)."; return false;
    }
    bookmarkError_.clear(); return true;
}

QByteArray BrowserData::exportBookmarks() const { return QJsonDocument(bookmarks_).toJson(); }
bool BrowserData::importBookmarks(const QByteArray &bytes)
{
    if (bookmarksReadOnly_ || bytes.size() > 4 * 1024 * 1024) return false;
    QJsonParseError parse; const auto document = QJsonDocument::fromJson(bytes, &parse);
    if (parse.error != QJsonParseError::NoError || !document.isArray() || document.array().size() + bookmarks_.size() > 5000) return false;
    auto combined = bookmarks_;
    QSet<QString> keys;
    for (const auto &value : combined) { const auto row = value.toObject(); keys.insert(row.value("url").toString() + '\n' + row.value("folder").toString()); }
    for (const auto &value : document.array()) {
        if (!value.isObject()) return false;
        auto row = value.toObject(); const QUrl url(row.value("url").toString());
        const auto folder = row.value("folder").toString();
        if (!row.value("title").isString() || !row.value("url").isString() || (row.contains("folder") && !row.value("folder").isString()) || !url.isValid() || url.host().isEmpty()
            || !url.userInfo().isEmpty() || (url.scheme() != "https" && url.scheme() != "http") || url.toString().size() > 32768
            || row.value("title").toString().size() > 1024 || folder.size() > 256 || folder.contains('\n') || folder.contains('\r')) return false;
        const auto key = url.toString(QUrl::FullyEncoded) + '\n' + folder;
        if (keys.contains(key)) continue;
        keys.insert(key);
        row["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces); row["url"] = url.toString(QUrl::FullyEncoded); combined.append(row);
    }
    const auto previous = bookmarks_; bookmarks_ = combined;
    if (!saveBookmarks()) { bookmarks_ = previous; return false; }
    emit changed(); return true;
}
void BrowserData::save()
{
    if (private_ || readOnly_) return;
    QDir().mkpath(directory_);
    QSaveFile file(directory_ + "/history.json");
    auto bytes = QJsonDocument(history_).toJson(QJsonDocument::Compact);
    bool pruned = false;
    while (bytes.size() > 8 * 1024 * 1024 && history_.size() > 1) {
        const auto remove = qMax(qsizetype(1), history_.size() / 4);
        for (qsizetype i = 0; i < remove; ++i) history_.removeFirst();
        bytes = QJsonDocument(history_).toJson(QJsonDocument::Compact); pruned = true;
    }
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        error_ = "History could not be saved.";
    else error_.clear();
    if (pruned || !error_.isEmpty()) emit changed();
}
