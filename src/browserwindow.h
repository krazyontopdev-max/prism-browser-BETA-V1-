#pragma once

#include "address.h"

#include <QList>
#include <QMainWindow>
#include <QElapsedTimer>
#include <QPointer>
#include <QSet>
#include <memory>

class QAction;
class QCloseEvent;
class QLabel;
class QLineEdit;
class QProgressBar;
class QTabWidget;
class QToolButton;
class QWebEngineProfile;
class QWebEngineView;
class QTimer;
class QWebEngineDownloadRequest;
class NewTabStore;
class Shields;
class BrowserData;
class QTemporaryDir;

class BrowserWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit BrowserWindow(QWebEngineProfile *profile = nullptr, bool privateWindow = false, bool restoreSession = false);
    ~BrowserWindow() override;

    QWebEngineView *addTab(const QUrl &url = newTabUrl(), bool activate = true);
    void closeTab(int index);
    void restoreClosedTab();

public slots:
    void evaluateSleepingTabs();

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    struct ClosedTab {
        QUrl url;
        int index;
    };

    QWebEngineView *currentView() const;
    void activateCurrentTab();
    void navigateFromAddress();
    void updateControls(bool updateAddress = true);
    void applyTheme();
    void updateStartPage(QWebEngineView *view);
    void setError(QWebEngineView *view, const QString &message);
    void setSavingsMode(const QString &mode);
    void configureNavigationCache(QWebEngineView *view);
    void scheduleSleepingTabs();
    void refreshDownloads();
    void showPrivacy();
    void showSettings();
    void showBookmarks();
    void showProfiles();
    void showDownloads();
    void saveSession();

    QWebEngineProfile *profile_;
    bool private_ = false;
    std::unique_ptr<QTemporaryDir> privateDirectory_;
    Shields *shields_ = nullptr;
    BrowserData *data_ = nullptr;
    QString homepage_;
    QString searchEngine_ = "google";
    QString startup_ = "newtab";
    bool askDownloads_ = true;
    QTimer *sessionTimer_ = nullptr;
    NewTabStore *newTabStore_ = nullptr;
    QTabWidget *tabs_;
    QLineEdit *address_ = nullptr;
    QToolButton *back_ = nullptr;
    QToolButton *forward_ = nullptr;
    QToolButton *reload_ = nullptr;
    QProgressBar *progress_ = nullptr;
    QWidget *errorBanner_ = nullptr;
    QLabel *errorText_ = nullptr;
    QAction *restoreAction_ = nullptr;
    QList<QWebEngineView *> recentTabs_;
    QList<ClosedTab> closedTabs_;
    QString theme_ = "system";
    QString accent_ = "#9b7dff";
    bool dark_ = false;
    QIcon reloadIcon_;
    QIcon stopIcon_;
    QElapsedTimer clock_;
    QTimer *sleepTimer_ = nullptr;
    QString savingsMode_ = "balanced";
    bool evaluatingSleep_ = false;
    QPointer<QWebEngineView> lastActive_;
    QAction *keepAwake_ = nullptr;
    QSet<QWebEngineDownloadRequest *> downloads_;
    QWidget *downloadsBar_ = nullptr;
    QLabel *downloadsText_ = nullptr;
};
