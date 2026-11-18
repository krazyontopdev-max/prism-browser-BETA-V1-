#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QVariantMap>
#include <functional>

class QWebEngineProfile;
class QIcon;
class QImage;

class NewTabStore : public QObject {
    Q_OBJECT
public:
    explicit NewTabStore(const QString &directory = {}, QObject *parent = nullptr);
    ~NewTabStore() override;
    QVariantMap state();
    void setOptions(const QVariantMap &options);
    void addShortcut(const QString &name, const QString &url, int replace = -1);
    void removeShortcut(int index);
    void removeCustom(const QString &id);
    void importImages(const QStringList &paths);
    void refreshCatalog();
    void shuffle();
    void setActive(bool active);
    void setIconProfile(QWebEngineProfile *profile);
    void rememberFavicon(const QUrl &pageUrl, const QIcon &icon);
    static QList<QUrl> faviconUrls(const QByteArray &html, const QUrl &baseUrl);
    static QStringList categories();
    static QJsonArray validateCatalog(const QByteArray &bytes, const QUrl &origin, QString *error);

signals:
    void changed();

private:
    void save();
    void schedule();
    void ensureWallpaper(const QJsonObject &entry);
    void ensureFavicons();
    void fetchFavicon(const QUrl &site);
    void tryFaviconUrls(const QUrl &site, const QList<QUrl> &urls, int index = 0);
    bool saveFavicon(const QUrl &site, const QImage &image);
    void fetch(const QUrl &url, qint64 limit, bool sameOrigin,
               const std::function<void(QByteArray, QString, QUrl)> &done);
    QJsonObject selection() const;
    QJsonArray availablePhotos() const;
    QString imagePath(const QString &id, bool custom) const;
    QString root_;
    QString builtinRoot_;
    QJsonArray builtins_;
    QJsonObject settings_;
    QJsonArray catalog_;
    QJsonArray shortcuts_;
    QJsonArray custom_;
    QNetworkAccessManager network_;
    QPointer<QWebEngineProfile> iconProfile_;
    QTimer timer_;
    QSet<QString> pending_;
    QSet<QString> attemptedIcons_;
    QString status_;
    bool active_ = false;
    bool importing_ = false;
};
