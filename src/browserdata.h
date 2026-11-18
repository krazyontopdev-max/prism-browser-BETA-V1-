#pragma once

#include <QObject>
#include <QJsonArray>
#include <QTimer>
#include <QUrl>
#include <QStringList>

class BrowserData : public QObject {
    Q_OBJECT
public:
    BrowserData(const QString &directory, bool privateWindow, QObject *parent = nullptr);
    ~BrowserData() override;
    void visit(const QUrl &url, const QString &title);
    QJsonArray history() const;
    void removeVisits(QList<int> indexes);
    void clearHistory();
    QString error() const;
    QJsonArray bookmarks() const;
    bool setBookmark(const QString &id, const QString &title, const QUrl &url, const QString &folder = {});
    bool removeBookmarks(const QStringList &ids);
    QByteArray exportBookmarks() const;
    bool importBookmarks(const QByteArray &bytes);
signals:
    void changed();
private:
    void save();
    QString directory_;
    bool private_;
    QJsonArray history_;
    QTimer timer_;
    QString error_;
    bool readOnly_ = false;
    QJsonArray bookmarks_;
    bool bookmarksReadOnly_ = false;
    QString bookmarkError_;
    bool saveBookmarks();
};
