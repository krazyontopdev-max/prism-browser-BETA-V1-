#pragma once

#include <QWebEngineUrlRequestInterceptor>
#include <QSet>
#include <QReadWriteLock>
#include <QNetworkAccessManager>

class Shields : public QWebEngineUrlRequestInterceptor {
    Q_OBJECT
public:
    explicit Shields(const QString &directory, bool privateWindow, QObject *parent = nullptr);
    ~Shields() override;
    void interceptRequest(QWebEngineUrlRequestInfo &info) override;
    bool blocks(const QUrl &url, const QUrl &topLevel, bool mainFrame = false) const;
    bool replaceRules(const QByteArray &bytes, int minimum = 1);
    void setMode(const QString &mode);
    QString mode() const;
    void setException(const QString &host, bool allowed);
    QStringList exceptions() const;
    QString status() const;
    void updateLists();
signals:
    void changed();
private:
    QString directory_;
    bool private_;
    mutable QReadWriteLock lock_;
    QSet<QString> domains_;
    QSet<QString> exceptions_;
    QSet<QString> suffixes_, wildcards_, suffixExceptions_;
    QString mode_ = "standard";
    QString status_;
    QNetworkAccessManager network_;
    bool updating_ = false;
    QString site(const QString &host) const;
    void save();
};
