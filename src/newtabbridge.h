#pragma once

#include <QObject>
#include <QPointer>
#include <QVariantMap>

class NewTabStore;
class QWebEnginePage;

class NewTabBridge : public QObject {
    Q_OBJECT
public:
    NewTabBridge(NewTabStore *store, QWebEnginePage *page, QObject *parent = nullptr);
    Q_INVOKABLE QVariantMap getState();
    Q_INVOKABLE void setOptions(const QVariantMap &options);
    Q_INVOKABLE void addShortcut(const QString &name, const QString &url, int replace);
    Q_INVOKABLE void removeShortcut(int index);
    Q_INVOKABLE void removeCustom(const QString &id);
    Q_INVOKABLE void chooseImages();
    Q_INVOKABLE void refreshCatalog();
    Q_INVOKABLE void shuffle();
    Q_INVOKABLE void toggleFullscreen();
    Q_INVOKABLE void search(const QString &text);
    Q_INVOKABLE void openBrowserSettings();
signals:
    void stateChanged(const QVariantMap &state);
    void fullscreenRequested();
    void settingsRequested();
private:
    bool allowed() const;
    NewTabStore *store_;
    QPointer<QWebEnginePage> page_;
};
