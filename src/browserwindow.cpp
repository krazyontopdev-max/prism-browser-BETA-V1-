#include "browserwindow.h"
#include "newtabstore.h"
#include "newtabbridge.h"
#include "shields.h"
#include "browserdata.h"
#include "localfilepicker.h"
#include <QWebChannel>
#include <QWebEngineNavigationRequest>

#include <QAction>
#include <QActionGroup>
#include <QAuthenticator>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QApplication>
#include <QCloseEvent>
#include <QFrame>
#include <QColorDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QWebEngineCookieStore>
#include <QNetworkCookie>
#include <QTreeWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QJsonObject>
#include <QMetaEnum>
#include <QHostAddress>
#include <QDir>
#include <QFile>
#include <algorithm>
#include <QTimer>
#include <QWebEngineDownloadRequest>
#include <QWebEngineDesktopMediaRequest>
#include <QWebEngineFullScreenRequest>
#include <QWebEnginePermission>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QItemSelectionModel>
#include <QMenu>
#include <QPainter>
#include <QProgressBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QStyleHints>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWebEngineHistory>
#include <QWebEngineLoadingInfo>
#include <QWebEngineNewWindowRequest>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>

#include <functional>

namespace {
void requestCredentials(QWidget *parent, const QString &site, QAuthenticator *authenticator)
{
    QDialog dialog(parent);
    dialog.setObjectName("authenticationDialog");
    dialog.setWindowTitle("Website sign in");
    auto *layout = new QFormLayout(&dialog);
    auto *origin = new QLabel(site, &dialog);
    origin->setTextFormat(Qt::PlainText);
    layout->addRow(origin);
    auto *username = new QLineEdit(&dialog);
    username->setObjectName("username");
    auto *password = new QLineEdit(&dialog);
    password->setObjectName("password");
    password->setEchoMode(QLineEdit::Password);
    layout->addRow("Username", username);
    layout->addRow("Password", password);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    // Qt requires the authenticator to be filled before this signal returns.
    if (dialog.exec() == QDialog::Accepted) {
        authenticator->setUser(username->text());
        authenticator->setPassword(password->text());
    }
}

QIcon symbolIcon(const QString &name, const QColor &color)
{
    QPixmap image(48, 48);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(2, 2);
    painter.setPen(QPen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (name == "back" || name == "forward") {
        if (name == "forward") {
            painter.translate(24, 0);
            painter.scale(-1, 1);
        }
        painter.drawLine(QPointF(19, 12), QPointF(5, 12));
        painter.drawPolyline(QPolygonF{QPointF(11, 6), QPointF(5, 12), QPointF(11, 18)});
    } else if (name == "reload") {
        painter.drawArc(QRectF(5, 5, 14, 14), 40 * 16, 290 * 16);
        painter.drawPolyline(QPolygonF{QPointF(19, 4), QPointF(19, 9), QPointF(14, 9)});
    } else if (name == "home") {
        painter.drawPolyline(QPolygonF{QPointF(3, 11), QPointF(12, 4), QPointF(21, 11)});
        painter.drawPolyline(QPolygonF{QPointF(6, 9), QPointF(6, 20), QPointF(10, 20),
            QPointF(10, 14), QPointF(14, 14), QPointF(14, 20), QPointF(18, 20), QPointF(18, 9)});
    } else if (name == "newTab") {
        painter.drawLine(QPointF(6, 12), QPointF(18, 12));
        painter.drawLine(QPointF(12, 6), QPointF(12, 18));
    } else if (name == "stop") {
        painter.drawLine(QPointF(7, 7), QPointF(17, 17));
        painter.drawLine(QPointF(17, 7), QPointF(7, 17));
    } else if (name == "menu") {
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        for (int x : {5, 12, 19})
            painter.drawEllipse(QPointF(x, 12), 1.4, 1.4);
    } else if (name == "prism") {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#b0b0b0"));
        painter.drawPolygon(QPolygonF{QPointF(12, 2), QPointF(11.6, 14), QPointF(2, 21)});
        painter.setBrush(QColor("#f5f5f5"));
        painter.drawPolygon(QPolygonF{QPointF(12, 2), QPointF(22, 21), QPointF(11.6, 14)});
        painter.setBrush(QColor("#808080"));
        painter.drawPolygon(QPolygonF{QPointF(2, 21), QPointF(11.6, 14), QPointF(22, 21)});
    }
    return QIcon(image);
}
}

BrowserWindow::BrowserWindow(QWebEngineProfile *profile, bool privateWindow, bool restoreSession)
    : profile_(profile ? profile : privateWindow ? new QWebEngineProfile(this) : new QWebEngineProfile("Prism", this)),
      private_(privateWindow),
      tabs_(new QTabWidget(this))
{
    clock_.start();
    QSettings preferences;
    homepage_ = preferences.value("general/homepage", newTabUrl().toString()).toString();
    const QUrl homeUrl(homepage_);
    if (homeUrl != newTabUrl() && homeUrl != QUrl("about:blank")
        && (homeUrl.host().isEmpty() || (homeUrl.scheme() != "http" && homeUrl.scheme() != "https"))) homepage_ = newTabUrl().toString();
    searchEngine_ = preferences.value("search/engine", "google").toString();
    if (searchEngine_ != "duckduckgo" && searchEngine_ != "bing") searchEngine_ = "google";
    startup_ = preferences.value("general/startup", "newtab").toString();
    askDownloads_ = preferences.value("downloads/ask", true).toBool();
    const auto downloadPath = preferences.value("downloads/directory").toString();
    if (!downloadPath.isEmpty() && QDir(downloadPath).exists()) profile_->setDownloadPath(downloadPath);
    if (preferences.contains("permissions/remember"))
        profile_->setPersistentPermissionsPolicy(preferences.value("permissions/remember").toBool()
            ? (profile_->isOffTheRecord() ? QWebEngineProfile::PersistentPermissionsPolicy::StoreInMemory : QWebEngineProfile::PersistentPermissionsPolicy::StoreOnDisk)
            : QWebEngineProfile::PersistentPermissionsPolicy::AskEveryTime);
    sessionTimer_ = new QTimer(this); sessionTimer_->setSingleShot(true);
    sessionTimer_->setInterval(1000); sessionTimer_->setTimerType(Qt::CoarseTimer);
    connect(sessionTimer_, &QTimer::timeout, this, &BrowserWindow::saveSession);
    qApp->installEventFilter(this);
    if (private_) {
        setObjectName("privateBrowser");
        privateDirectory_ = std::make_unique<QTemporaryDir>();
        if (!privateDirectory_->isValid()) qFatal("Cannot create isolated private-window storage.");
    }
    newTabStore_ = new NewTabStore(private_ ? privateDirectory_->path() : QString(), this);
    newTabStore_->setIconProfile(profile_);
    shields_ = profile_->findChild<Shields *>("prismShields");
    if (!shields_) {
        shields_ = new Shields(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/Shields", private_, profile_);
        shields_->setObjectName("prismShields");
    }
    profile_->setUrlRequestInterceptor(shields_);
    data_ = new BrowserData(private_ ? privateDirectory_->path() : QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/BrowserData", private_, this);
    sleepTimer_ = new QTimer(this);
    sleepTimer_->setObjectName("sleepTimer");
    sleepTimer_->setTimerType(Qt::CoarseTimer);
    sleepTimer_->setSingleShot(true);
    connect(sleepTimer_, &QTimer::timeout, this, &BrowserWindow::evaluateSleepingTabs);
    resize(1180, 800);
    setMinimumSize(620, 440);
    setWindowTitle("Prism Browser");
    setWindowIcon(symbolIcon("prism", {}));
    auto *root = new QWidget(this);
    auto *layout = new QVBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto *toolbar = new QFrame(root);
    toolbar->setObjectName("toolbar");
    auto *tools = new QHBoxLayout(toolbar);
    tools->setContentsMargins(14, 10, 14, 10);
    tools->setSpacing(6);
    auto *mark = new QLabel(toolbar);
    mark->setPixmap(windowIcon().pixmap(24, 24));
    tools->addWidget(mark);
    auto *brand = new QLabel("PRISM", toolbar);
    brand->setObjectName("brand");
    tools->addWidget(brand);
    tools->addSpacing(16);

    auto button = [tools, toolbar](const QString &name, const QString &text, const QString &tip) {
        auto *control = new QToolButton(toolbar);
        control->setObjectName(name);
        control->setProperty("symbol", name);
        control->setText(text);
        control->setToolTip(tip);
        control->setAccessibleName(tip);
        control->setFocusPolicy(Qt::StrongFocus);
        control->setFixedSize(32, 32);
        tools->addWidget(control);
        return control;
    };
    back_ = button("back", "←", "Back (Alt+Left)");
    forward_ = button("forward", "→", "Forward (Alt+Right)");
    reload_ = button("reload", "↻", "Reload (Ctrl+R)");
    auto *home = button("home", "⌂", "Home (Alt+Home)");
    tools->addSpacing(8);
    address_ = new QLineEdit(toolbar);
    address_->setObjectName("address");
    address_->setAccessibleName("Address or web search");
    address_->setPlaceholderText("Search the web or enter a website");
    address_->setClearButtonEnabled(true);
    address_->setMinimumHeight(36);
    tools->addWidget(address_, 1);
    tools->addSpacing(6);
    auto *menuButton = button("menu", "⋯", "Prism menu");
    auto *menu = new QMenu(menuButton);
    menuButton->setMenu(menu);
    menuButton->setPopupMode(QToolButton::InstantPopup);

    tabs_->setObjectName("tabs");
    tabs_->setDocumentMode(true);
    tabs_->setMovable(true);
    tabs_->setTabsClosable(false);
    tabs_->setUsesScrollButtons(true);
    tabs_->setElideMode(Qt::ElideRight);
    tabs_->tabBar()->setExpanding(false);
    auto *newTabButton = new QToolButton(tabs_);
    newTabButton->setObjectName("newTab");
    newTabButton->setProperty("symbol", "newTab");
    newTabButton->setText("+");
    newTabButton->setToolTip("New tab (Ctrl+T)");
    newTabButton->setAccessibleName("New tab");
    newTabButton->setFixedSize(38, 34);
    tabs_->setCornerWidget(newTabButton, Qt::TopRightCorner);

    errorBanner_ = new QFrame(root);
    errorBanner_->setObjectName("errorBanner");
    auto *errorLayout = new QHBoxLayout(errorBanner_);
    errorLayout->setContentsMargins(18, 8, 12, 8);
    errorText_ = new QLabel(errorBanner_);
    errorText_->setTextFormat(Qt::PlainText);
    errorText_->setWordWrap(true);
    errorLayout->addWidget(errorText_, 1);
    auto *retry = new QToolButton(errorBanner_);
    retry->setText("Retry");
    retry->setObjectName("retry");
    errorLayout->addWidget(retry);
    auto *dismiss = new QToolButton(errorBanner_);
    dismiss->setText("×");
    dismiss->setAccessibleName("Dismiss error");
    errorLayout->addWidget(dismiss);
    errorBanner_->hide();

    progress_ = new QProgressBar(root);
    progress_->setRange(0, 100);
    progress_->setTextVisible(false);
    progress_->setFixedHeight(2);
    progress_->hide();
    layout->addWidget(toolbar);
    layout->addWidget(errorBanner_);
    layout->addWidget(progress_);
    layout->addWidget(tabs_, 1);
    downloadsBar_ = new QFrame(root);
    auto *downloadLayout = new QHBoxLayout(downloadsBar_);
    downloadsText_ = new QLabel(downloadsBar_);
    downloadsText_->setTextFormat(Qt::PlainText);
    downloadLayout->addWidget(downloadsText_, 1);
    auto *cancelDownloads = new QToolButton(downloadsBar_);
    cancelDownloads->setText("Cancel downloads");
    cancelDownloads->setObjectName("cancelDownloads");
    downloadLayout->addWidget(cancelDownloads);
    connect(cancelDownloads, &QToolButton::clicked, this, [this] {
        const auto pending = downloads_;
        for (auto *download : pending) download->cancel();
    });
    downloadsBar_->hide();
    layout->addWidget(downloadsBar_);
    setCentralWidget(root);

    auto shortcut = [this](const QString &name, const QString &text,
                           const QList<QKeySequence> &keys, const std::function<void()> &run) {
        auto *action = new QAction(text, this);
        action->setObjectName(name);
        action->setShortcuts(keys);
        addAction(action);
        connect(action, &QAction::triggered, this, run);
        return action;
    };
    auto *newAction = shortcut("newTabAction", "New tab", {QKeySequence("Ctrl+T")}, [this] {
        addTab();
        address_->setFocus();
    });
    shortcut("closeTabAction", "Close tab", {QKeySequence("Ctrl+W")}, [this] {
        closeTab(tabs_->currentIndex());
    });
    restoreAction_ = shortcut("restoreTabAction", "Reopen closed tab", {QKeySequence("Ctrl+Shift+T")},
                              [this] { restoreClosedTab(); });
    restoreAction_->setEnabled(false);
    shortcut("focusAddress", "Focus address", {QKeySequence("Ctrl+L"), QKeySequence("Alt+D"), QKeySequence("F6")},
             [this] { address_->setFocus(); address_->selectAll(); });
    shortcut("reloadAction", "Reload", {QKeySequence("Ctrl+R"), QKeySequence("F5")},
             [this] { currentView()->setFocus(); currentView()->reload(); });
    shortcut("stopAction", "Stop", {QKeySequence("Esc")}, [this] {
        if (isFullScreen()) { showNormal(); currentView()->triggerPageAction(QWebEnginePage::ExitFullScreen); }
        else currentView()->stop();
    });
    shortcut("backAction", "Back", {QKeySequence("Alt+Left")}, [this] {
        currentView()->setFocus(); currentView()->back();
    });
    shortcut("forwardAction", "Forward", {QKeySequence("Alt+Right")}, [this] {
        currentView()->setFocus(); currentView()->forward();
    });
    shortcut("homeAction", "Home", {QKeySequence("Alt+Home")}, [this] {
        currentView()->setFocus(); currentView()->load(QUrl(homepage_));
    });
    shortcut("nextTab", "Next tab", {QKeySequence("Ctrl+Tab")}, [this] {
        tabs_->setCurrentIndex((tabs_->currentIndex() + 1) % tabs_->count());
    });
    shortcut("previousTab", "Previous tab", {QKeySequence("Ctrl+Shift+Tab")}, [this] {
        tabs_->setCurrentIndex((tabs_->currentIndex() + tabs_->count() - 1) % tabs_->count());
    });

    connect(newTabButton, &QToolButton::clicked, newAction, &QAction::trigger);
    connect(back_, &QToolButton::clicked, this, [this] { currentView()->back(); });
    connect(forward_, &QToolButton::clicked, this, [this] { currentView()->forward(); });
    connect(reload_, &QToolButton::clicked, this, [this] {
        if (currentView()->page()->isLoading())
            currentView()->stop();
        else
            currentView()->reload();
    });
    connect(home, &QToolButton::clicked, this, [this] { currentView()->load(QUrl(homepage_)); });
    connect(address_, &QLineEdit::returnPressed, this, [this] { navigateFromAddress(); });
    connect(retry, &QToolButton::clicked, this, [this] { currentView()->reload(); });
    connect(dismiss, &QToolButton::clicked, this, [this] { setError(currentView(), {}); });
    connect(tabs_, &QTabWidget::currentChanged, this, [this] { activateCurrentTab(); });
    connect(tabs_, &QTabWidget::tabCloseRequested, this, &BrowserWindow::closeTab);
    connect(tabs_->tabBar(), &QTabBar::tabBarDoubleClicked, this, [this](int index) {
        if (index == -1)
            addTab();
    });

    menu->addAction(newAction);
    menu->addAction(shortcut("settingsAction", "Settings…", {QKeySequence("Ctrl+,")}, [this] { showSettings(); }));
    menu->addAction(shortcut("bookmarksAction", "Bookmarks…", {QKeySequence("Ctrl+Shift+O")}, [this] { showBookmarks(); }));
    menu->addAction(shortcut("bookmarkPageAction", "Bookmark this page", {QKeySequence("Ctrl+D")}, [this] {
        if (data_->setBookmark({}, currentView()->title(), currentView()->url())) showBookmarks();
        else setError(currentView(), "Only HTTP(S) pages can be bookmarked. " + data_->error());
    }));
    menu->addAction(shortcut("historyAction", "History and site data…", {QKeySequence("Ctrl+H")}, [this] { showPrivacy(); }));
    menu->addAction(shortcut("downloadsAction", "Downloads…", {QKeySequence("Ctrl+J")}, [this] { showDownloads(); }));
    tabs_->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tabs_->tabBar(), &QTabBar::customContextMenuRequested, this, [this](const QPoint &point) {
        auto *view = qobject_cast<QWebEngineView *>(tabs_->widget(tabs_->tabBar()->tabAt(point))); if (!view) return;
        auto *popup = new QMenu(this); popup->setAttribute(Qt::WA_DeleteOnClose);
        const QPointer<QWebEngineView> guard(view);
        popup->addAction("Duplicate tab", this, [this, guard] { if (guard) addTab(guard->url()); });
        auto *mute = popup->addAction("Mute tab"); mute->setCheckable(true); mute->setChecked(view->page()->isAudioMuted());
        connect(mute, &QAction::triggered, this, [guard](bool value) { if (guard) guard->page()->setAudioMuted(value); });
        auto *awake = popup->addAction("Keep awake"); awake->setCheckable(true); awake->setChecked(view->property("keepAwake").toBool());
        connect(awake, &QAction::triggered, this, [this, guard](bool value) { if (guard) { guard->setProperty("keepAwake", value); scheduleSleepingTabs(); } });
        popup->addAction("Close tab", this, [this, guard] { if (guard) closeTab(tabs_->indexOf(guard)); });
        popup->addAction("Close other tabs", this, [this, guard] { if (guard) for (int i = tabs_->count() - 1; i >= 0; --i) if (tabs_->widget(i) != guard) closeTab(i); });
        popup->addAction("Reopen closed tab", this, &BrowserWindow::restoreClosedTab);
        popup->popup(tabs_->tabBar()->mapToGlobal(point));
    });
    auto *privateAction = shortcut("privateWindowAction", "New private window", {QKeySequence("Ctrl+Shift+N")}, [] {
        auto *window = new BrowserWindow(nullptr, true);
        window->setAttribute(Qt::WA_DeleteOnClose);
        window->show();
    });
    menu->addAction(privateAction);
    auto *shieldsMenu = menu->addMenu("Prism Shields");
    auto *shieldGroup = new QActionGroup(this);
    for (const auto &mode : QStringList{"standard", "strict", "off"}) {
        auto *action = shieldsMenu->addAction(mode == "standard" ? "Standard (default)" : mode == "strict" ? "Strict" : "Off");
        action->setObjectName("shields_" + mode);
        action->setCheckable(true); action->setChecked(shields_->mode() == mode);
        shieldGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, mode] { shields_->setMode(mode); });
    }
    auto *siteException = shieldsMenu->addAction("Turn off for this site");
    siteException->setCheckable(true);
    connect(shieldsMenu, &QMenu::aboutToShow, this, [this, siteException, shieldGroup] {
        const auto host = currentView()->url().host();
        QHostAddress address;
        siteException->setEnabled(host.contains('.') && !address.setAddress(host));
        siteException->setChecked(shields_->exceptions().contains(QString::fromLatin1(QUrl::toAce(host))));
        for (auto *action : shieldGroup->actions()) action->setChecked(action->objectName() == "shields_" + shields_->mode());
    });
    connect(siteException, &QAction::triggered, this, [this](bool excepted) {
        shields_->setException(currentView()->url().host(), excepted);
        setError(currentView(), "Shields changed. Reload this page to apply the new rules.");
    });
    auto *updateFilters = shieldsMenu->addAction("Update domain list");
    updateFilters->setEnabled(!private_);
    connect(updateFilters, &QAction::triggered, shields_, &Shields::updateLists);
    shieldsMenu->addAction("Privacy and site data…", this, &BrowserWindow::showPrivacy);
    menu->addAction(restoreAction_);
    menu->addSeparator();
    auto *themes = menu->addMenu("Appearance");
    auto *themeGroup = new QActionGroup(this);
    theme_ = QSettings().value("appearance/theme", "system").toString();
    if (theme_ != "light" && theme_ != "dark")
        theme_ = "system";
    const QList<QPair<QString, QString>> choices = {{"system", "System"}, {"light", "Light"}, {"dark", "Dark"}};
    for (const auto &choice : choices) {
        auto *action = themes->addAction(choice.second);
        action->setObjectName("theme" + choice.second);
        action->setCheckable(true);
        action->setChecked(theme_ == choice.first);
        themeGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, mode = choice.first] {
            theme_ = mode;
            if (!private_) QSettings().setValue("appearance/theme", mode);
            if (!private_) qApp->styleHints()->setColorScheme(mode == "system" ? Qt::ColorScheme::Unknown
                : mode == "dark" ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
            applyTheme();
        });
    }
    accent_ = QSettings().value("appearance/accent", "#9b7dff").toString();
    if (!QColor(accent_).isValid()) accent_ = "#9b7dff";
    accent_ = QColor(accent_).name();
    auto *accents = themes->addMenu("Accent color");
    auto *accentGroup = new QActionGroup(this);
    for (const auto &choice : QList<QPair<QString, QString>>{{"#9b7dff", "Prism violet"},
             {"#df82c6", "Orchid"}, {"#71b8eb", "Glacier"}, {"#77c8b2", "Mint"}, {"#e9b476", "Amber"}}) {
        auto *action = accents->addAction(choice.second);
        action->setObjectName("accent_" + choice.first.mid(1));
        action->setCheckable(true);
        action->setChecked(accent_ == choice.first);
        accentGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, color = choice.first] {
            accent_ = color;
            if (!private_) QSettings().setValue("appearance/accent", accent_);
            applyTheme();
        });
    }
    auto *customAccent = accents->addAction("Custom color…");
    customAccent->setObjectName("customAccent");
    connect(customAccent, &QAction::triggered, this, [this, accentGroup] {
        auto *dialog = new QColorDialog(QColor(accent_), this);
        dialog->setWindowTitle("Prism accent color");
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog, &QColorDialog::colorSelected, this, [this, accentGroup](const QColor &color) {
            if (!color.isValid()) return;
            accent_ = color.name();
            if (!private_) QSettings().setValue("appearance/accent", accent_);
            for (auto *action : accentGroup->actions()) action->setChecked(action->objectName() == "accent_" + accent_.mid(1));
            applyTheme();
        });
        dialog->open();
    });
    menu->addSeparator();
    auto *savings = menu->addMenu("Tab resource savings");
    auto *savingsGroup = new QActionGroup(this);
    savingsMode_ = QSettings().value("performance/mode", "balanced").toString();
    if (savingsMode_ != "maximum" && savingsMode_ != "off") savingsMode_ = "balanced";
    for (const auto &choice : QList<QPair<QString, QString>>{{"balanced", "Balanced"}, {"maximum", "Maximum Savings"}, {"off", "Off"}}) {
        auto *action = savings->addAction(choice.second);
        action->setObjectName("savings_" + choice.first);
        action->setCheckable(true);
        action->setChecked(savingsMode_ == choice.first);
        if (choice.first == "maximum") action->setToolTip("Freeze after one minute; release untouched eligible web pages after ten minutes. Released pages reload on selection. Tabs with user input stay frozen. Use Keep Awake for ongoing work.");
        savingsGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, mode = choice.first] { setSavingsMode(mode); });
    }
    keepAwake_ = menu->addAction("Keep this tab awake");
    keepAwake_->setObjectName("keepAwake");
    keepAwake_->setCheckable(true);
    connect(keepAwake_, &QAction::toggled, this, [this](bool keep) {
        if (auto *view = currentView()) view->setProperty("keepAwake", keep);
        scheduleSleepingTabs();
    });
    connect(profile_, &QWebEngineProfile::downloadRequested, this, [this](QWebEngineDownloadRequest *download) {
        if (download->isSavePageDownload()) { download->cancel(); return; }
        downloads_.insert(download);
        scheduleSleepingTabs();
        refreshDownloads();
        connect(download, &QWebEngineDownloadRequest::receivedBytesChanged, this, &BrowserWindow::refreshDownloads);
        connect(download, &QWebEngineDownloadRequest::isFinishedChanged, this, [this, download] {
            if (!download->isFinished()) return;
            if (!private_) {
                QSettings settings; auto records = settings.value("downloads/history").toList();
                records.prepend(QVariantMap{{"name", download->downloadFileName()}, {"path", QDir(download->downloadDirectory()).filePath(download->downloadFileName())},
                    {"state", download->state() == QWebEngineDownloadRequest::DownloadCompleted ? "Completed" : "Stopped"}});
                while (records.size() > 200) records.removeLast(); settings.setValue("downloads/history", records);
            }
            downloads_.remove(download);
            downloadsText_->setText(download->state() == QWebEngineDownloadRequest::DownloadCompleted
                ? "Saved: " + download->downloadFileName() : "Download ended: " + download->interruptReasonString());
            if (!downloads_.isEmpty()) refreshDownloads();
            scheduleSleepingTabs();
            download->deleteLater();
        });
        connect(download, &QObject::destroyed, this, [this, download] { downloads_.remove(download); scheduleSleepingTabs(); });
        if (!askDownloads_) {
            const QDir folder(profile_->downloadPath());
            const QFileInfo suggested(QFileInfo(download->suggestedFileName()).fileName());
            for (int i = 0; !suggested.fileName().isEmpty() && i < 10000; ++i) {
                const auto name = i == 0 ? suggested.fileName() : suggested.completeBaseName() + QString(" (%1)").arg(i)
                    + (suggested.suffix().isEmpty() ? QString() : "." + suggested.suffix());
                QFile reserve(folder.filePath(name));
                if (reserve.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
                    // Reserve atomically so concurrent automatic downloads cannot overwrite a file.
                    reserve.close(); download->setDownloadDirectory(folder.absolutePath()); download->setDownloadFileName(name); download->accept(); return;
                }
                if (!folder.exists(name)) break;
            }
        }
        const QPointer<QWebEngineDownloadRequest> guard(download);
        const auto selected = [guard](const QString &path) {
            if (!guard || guard->isFinished()) return;
            const QFileInfo target(path);
            guard->setDownloadDirectory(target.absolutePath());
            guard->setDownloadFileName(target.fileName());
            guard->accept();
        };
        QDialog *dialog;
        if (private_) {
            auto *picker = new LocalFilePicker(this, true, QFileInfo(download->suggestedFileName()).fileName(), profile_->downloadPath());
            connect(picker, &LocalFilePicker::pathSelected, this, selected); dialog = picker;
        } else {
            auto *picker = new QFileDialog(this, "Save download", profile_->downloadPath());
            picker->setAcceptMode(QFileDialog::AcceptSave); picker->setFileMode(QFileDialog::AnyFile);
            picker->selectFile(QFileInfo(download->suggestedFileName()).fileName());
            connect(picker, &QFileDialog::fileSelected, this, selected); dialog = picker;
        }
        dialog->setObjectName("downloadDialog"); dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog, &QDialog::rejected, this, [guard] { if (guard) guard->cancel(); });
        connect(download, &QObject::destroyed, dialog, &QWidget::close);
        dialog->open();
    });
    auto *version = menu->addAction("Prism " + QCoreApplication::applicationVersion());
    version->setEnabled(false);
    menu->addAction("Quit", QKeySequence("Alt+F4"), this, &QWidget::close);
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (theme_ == "system")
            applyTheme();
    });
    if (!private_) qApp->styleHints()->setColorScheme(theme_ == "system" ? Qt::ColorScheme::Unknown
        : theme_ == "dark" ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
    applyTheme();
    const QByteArray geometry = QSettings().value("window/geometry").toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);
    if (!private_ && (restoreSession || startup_ == "session")) {
        const auto urls = preferences.value("session/urls").toStringList();
        for (const auto &input : urls.mid(0, 500)) {
            const QUrl url(input);
            if (url == newTabUrl() || url == QUrl("about:blank") || (url.isValid() && !url.host().isEmpty()
                && (url.scheme() == "http" || url.scheme() == "https"))) addTab(url, false);
        }
        if (tabs_->count()) tabs_->setCurrentIndex(qBound(0, preferences.value("session/selected", 0).toInt(), tabs_->count() - 1));
    }
    if (!tabs_->count()) addTab(!private_ && startup_ == "home" ? QUrl(homepage_) : newTabUrl());
    if (!private_) for (const auto &input : preferences.value("session/closed").toStringList().mid(0, 20)) {
        const QUrl url(input); if (url.isValid() && (url == newTabUrl() || url.scheme() == "https" || url.scheme() == "http")) closedTabs_.append({url, tabs_->count()});
    }
    restoreAction_->setEnabled(!closedTabs_.isEmpty());
}

BrowserWindow::~BrowserWindow()
{
    qApp->removeEventFilter(this);
    evaluatingSleep_ = true;
    sleepTimer_->stop();
    sessionTimer_->stop();
    for (auto *dialog : findChildren<QDialog *>("desktopCaptureDialog")) {
        dialog->reject(); delete dialog;
    }
    // Pages must be destroyed before their shared profile.
    delete takeCentralWidget();
    if (private_) { delete newTabStore_; newTabStore_ = nullptr; }
}

bool BrowserWindow::eventFilter(QObject *object, QEvent *event)
{
    const auto type = event->type();
    if (type == QEvent::KeyPress || type == QEvent::MouseButtonPress || type == QEvent::InputMethod
        || type == QEvent::Drop || type == QEvent::TouchBegin) {
        auto *widget = qobject_cast<QWidget *>(object);
        while (widget) {
            if (auto *view = qobject_cast<QWebEngineView *>(widget)) {
                // Keep this protection across history restores; Qt can miss edits outside HTML forms.
                if (tabs_->indexOf(view) >= 0) view->setProperty("userInteracted", true);
                break;
            }
            widget = widget->parentWidget();
        }
    }
    return QMainWindow::eventFilter(object, event);
}

QWebEngineView *BrowserWindow::addTab(const QUrl &url, bool activate)
{
    auto *view = new QWebEngineView(profile_, tabs_);
    view->page()->setProperty("privateBrowsing", private_);
    view->page()->setProperty("searchEngine", searchEngine_);
    auto *channel = new QWebChannel(view->page());
    auto *bridge = new NewTabBridge(newTabStore_, view->page(), view);
    channel->registerObject("prism", bridge);
    connect(bridge, &NewTabBridge::fullscreenRequested, this, [this] {
        if (isFullScreen()) showNormal(); else showFullScreen();
    });
    connect(bridge, &NewTabBridge::settingsRequested, this, &BrowserWindow::showSettings);
    connect(view->page(), &QWebEnginePage::navigationRequested, this,
            [view, page = view->page(), channel](QWebEngineNavigationRequest &request) {
        if (request.isMainFrame()) {
            page->setWebChannel(request.url() == newTabUrl() ? channel : nullptr);
            if (request.navigationType() == QWebEnginePage::NavigationTypeFormSubmitted) view->setProperty("formNavigation", true);
            else if (request.navigationType() == QWebEnginePage::NavigationTypeTyped || request.navigationType() == QWebEnginePage::NavigationTypeLinkClicked)
                view->setProperty("formNavigation", false);
        }
    });
    view->setProperty("inactiveSince", clock_.elapsed());
    view->settings()->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
    view->settings()->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, PRISM_ENABLE_SCREEN_SHARING != 0);
    connect(view->page(), &QWebEnginePage::desktopMediaRequested, this,
            [this, view](const QWebEngineDesktopMediaRequest &request) {
        if (view->property("permissionPending").toBool()) { request.cancel(); return; }
        view->setProperty("permissionPending", true);
        auto *dialog = new QDialog(this);
        dialog->setObjectName("desktopCaptureDialog");
        dialog->setProperty("capturePage", QVariant::fromValue<QObject *>(view->page()));
        dialog->setWindowTitle("Choose what to share");
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->resize(540, 400);
        auto *layout = new QVBoxLayout(dialog);
        auto *label = new QLabel("Sharing requested from the tab at " + view->url().adjusted(QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment | QUrl::RemoveUserInfo).toDisplayString()
            + ". Choose a source. Stop using the website controls or close this tab.", dialog);
        label->setTextFormat(Qt::PlainText); label->setWordWrap(true); layout->addWidget(label);
        auto *sources = new QTabWidget(dialog); layout->addWidget(sources);
        auto *screens = new QListView(sources); screens->setModel(request.screensModel());
        auto *windows = new QListView(sources); windows->setModel(request.windowsModel());
        screens->setObjectName("captureScreens"); windows->setObjectName("captureWindows");
        sources->addTab(screens, "Screens"); sources->addTab(windows, "Windows");
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
        auto *share = buttons->button(QDialogButtonBox::Ok); share->setText("Share"); share->setEnabled(false);
        share->setAutoDefault(false);
        buttons->button(QDialogButtonBox::Cancel)->setDefault(true);
        buttons->button(QDialogButtonBox::Cancel)->setFocus();
        layout->addWidget(buttons);
        const auto updateSelection = [sources, screens, windows, share] {
            share->setEnabled((sources->currentIndex() == 0 ? screens : windows)->currentIndex().isValid());
        };
        for (auto *list : {screens, windows})
            connect(list->selectionModel(), &QItemSelectionModel::currentChanged, dialog, updateSelection);
        connect(sources, &QTabWidget::currentChanged, dialog, updateSelection);
        connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
        connect(view, &QWebEngineView::urlChanged, dialog, &QDialog::reject);
        const QPointer<QWebEngineView> guard(view);
        const auto resolved = std::make_shared<bool>(false);
        connect(dialog, &QDialog::finished, this, [this, guard, request, sources, screens, windows, resolved](int result) {
            if (*resolved) return;
            *resolved = true;
            if (guard) guard->setProperty("permissionPending", false);
            const bool screen = sources->currentIndex() == 0;
            const auto index = (screen ? screens : windows)->currentIndex();
            if (result == QDialog::Accepted && guard && index.isValid()) {
                if (screen) request.selectScreen(index); else request.selectWindow(index);
            } else request.cancel();
            scheduleSleepingTabs();
        });
        // Qt defaults to the primary screen if a request is abandoned without cancellation.
        connect(dialog, &QObject::destroyed, this, [request, resolved] {
            if (!*resolved) { *resolved = true; request.cancel(); }
        });
        dialog->open(); scheduleSleepingTabs();
    });
    connect(view->page(), &QWebEnginePage::authenticationRequired, this,
            [this](const QUrl &url, QAuthenticator *authenticator) {
        requestCredentials(this, url.adjusted(QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment | QUrl::RemoveUserInfo).toDisplayString(), authenticator);
    });
    connect(view->page(), &QWebEnginePage::proxyAuthenticationRequired, this,
            [this](const QUrl &, QAuthenticator *authenticator, const QString &host) {
        requestCredentials(this, "Proxy: " + host, authenticator);
    });
    connect(view->page(), &QWebEnginePage::fullScreenRequested, this, [this](QWebEngineFullScreenRequest request) {
        request.accept();
        if (request.toggleOn()) showFullScreen(); else showNormal();
    });
    connect(view->page(), &QWebEnginePage::permissionRequested, this, [this, view](QWebEnginePermission permission) {
        QString capability;
        using Type = QWebEnginePermission::PermissionType;
        switch (permission.permissionType()) {
        case Type::MediaAudioCapture: capability = "use your microphone"; break;
        case Type::MediaVideoCapture: capability = "use your camera"; break;
        case Type::MediaAudioVideoCapture: capability = "use your camera and microphone"; break;
        case Type::DesktopVideoCapture: capability = "share the screen or window you selected"; break;
        case Type::DesktopAudioVideoCapture: capability = "share the screen or window you selected and its audio"; break;
        case Type::Geolocation: capability = "access your location"; break;
        case Type::Notifications: capability = "show notifications"; break;
        case Type::ClipboardReadWrite: capability = "read and write your clipboard"; break;
        default: permission.deny(); return;
        }
        view->setProperty("permissionPending", true);
        auto *prompt = new QMessageBox(QMessageBox::Question, "Website permission", {}, QMessageBox::Yes | QMessageBox::No, this);
        prompt->setObjectName("permissionPrompt");
        prompt->setTextFormat(Qt::PlainText);
        prompt->setText(permission.origin().toDisplayString() + " wants to " + capability + ". Allow?");
        prompt->setDefaultButton(QMessageBox::No);
        prompt->setAttribute(Qt::WA_DeleteOnClose);
        const QPointer<QWebEngineView> guard(view);
        connect(view, &QWebEngineView::urlChanged, prompt, &QDialog::reject);
        connect(view, &QObject::destroyed, prompt, &QDialog::reject);
        connect(prompt, &QDialog::finished, this, [this, guard, permission](int result) {
            if (guard) guard->setProperty("permissionPending", false);
            if (result == QMessageBox::Yes && guard && permission.isValid()) {
                guard->setProperty("permissionGranted", true);
                permission.grant();
            } else permission.deny();
            scheduleSleepingTabs();
        });
        prompt->open();
    });
    connect(view->page(), &QWebEnginePage::recommendedStateChanged, this, [this, view](QWebEnginePage::LifecycleState state) {
        if (state == QWebEnginePage::LifecycleState::Active)
            view->page()->setLifecycleState(state);
        scheduleSleepingTabs();
    });
    connect(view->page(), &QWebEnginePage::recentlyAudibleChanged, this, [this, view](bool audible) {
        if (audible) view->page()->setLifecycleState(QWebEnginePage::LifecycleState::Active);
        scheduleSleepingTabs();
    });
    view->settings()->setUnknownUrlSchemePolicy(QWebEngineSettings::DisallowUnknownUrlSchemes);
    view->setProperty("progress", 0);
    const int index = tabs_->addTab(view, "New tab");
    tabs_->setTabToolTip(index, "New tab");
    auto *closeButton = new QToolButton(tabs_->tabBar());
    closeButton->setObjectName("tabClose");
    closeButton->setProperty("symbol", "stop");
    closeButton->setIcon(symbolIcon("stop", QColor("#f5f5f5")));
    closeButton->setIconSize(QSize(12, 12));
    closeButton->setFixedSize(22, 22);
    closeButton->setAccessibleName("Close tab");
    closeButton->setToolTip("Close tab (Ctrl+W)");
    tabs_->tabBar()->setTabButton(index, QTabBar::RightSide, closeButton);
    connect(closeButton, &QToolButton::clicked, this, [this, view] { closeTab(tabs_->indexOf(view)); });
    connect(view, &QWebEngineView::titleChanged, this, [this, view](const QString &title) {
        const int tab = tabs_->indexOf(view);
        if (tab < 0)
            return;
        const QString label = view->url() == newTabUrl() ? "New tab"
            : title.isEmpty() ? view->url().host() : title;
        tabs_->setTabText(tab, label.isEmpty() ? "New tab" : label);
        tabs_->setTabToolTip(tab, label);
        if (view == currentView())
            setWindowTitle(label + (private_ ? " — Prism Browser · Private" : " — Prism Browser"));
    });
    connect(view, &QWebEngineView::iconChanged, this, [this, view](const QIcon &icon) {
        const int tab = tabs_->indexOf(view);
        if (tab >= 0)
            tabs_->setTabIcon(tab, icon);
        newTabStore_->rememberFavicon(view->url(), icon);
        if (view->page()->requestedUrl() != view->url()) newTabStore_->rememberFavicon(view->page()->requestedUrl(), icon);
    });
    connect(view, &QWebEngineView::urlChanged, this, [this, view] {
        if (!private_) sessionTimer_->start();
        if (view == currentView()) {
            newTabStore_->setActive(view->url() == newTabUrl());
            updateControls(!address_->hasFocus());
        }
    });
    connect(view, &QWebEngineView::loadStarted, this, [this, view] {
        scheduleSleepingTabs();
        view->setProperty("progress", 0);
        setError(view, {});
        if (view == currentView())
            updateControls(false);
    });
    connect(view, &QWebEngineView::loadProgress, this, [this, view](int progress) {
        view->setProperty("progress", progress);
        if (view == currentView())
            updateControls(false);
    });
    connect(view, &QWebEngineView::loadFinished, this, [this, view](bool success) {
        if (success) data_->visit(view->url(), view->title());
        configureNavigationCache(view);
        updateStartPage(view);
        scheduleSleepingTabs();
        if (view == currentView())
            updateControls(!address_->hasFocus());
    });
    connect(view->page(), &QWebEnginePage::loadingChanged, this,
            [this, view](const QWebEngineLoadingInfo &info) {
        if (info.status() == QWebEngineLoadingInfo::LoadFailedStatus && !info.isDownload()) {
            if (info.errorDomain() == QWebEngineLoadingInfo::HttpStatusCodeDomain)
                setError(view, QString("The website returned HTTP %1. Follow any instructions shown on the page.").arg(info.errorCode()));
            else
                setError(view, "Couldn't open this page. Check the address or your connection, then retry. "
                         + info.errorString());
        }
        if (view == currentView())
            updateControls(!address_->hasFocus());
    });
    connect(view, &QWebEngineView::renderProcessTerminated, this, [this, view](QWebEnginePage::RenderProcessTerminationStatus status, int) {
        if (status != QWebEnginePage::NormalTerminationStatus && view->page()->lifecycleState() != QWebEnginePage::LifecycleState::Discarded)
            setError(view, "This page stopped responding. Reload it to try again.");
    });
    connect(view->page(), &QWebEnginePage::newWindowRequested, this,
            [this](QWebEngineNewWindowRequest &request) {
        auto *target = addTab({}, request.destination() != QWebEngineNewWindowRequest::InNewBackgroundTab);
        request.openIn(target->page());
    });
    connect(view->page(), &QWebEnginePage::windowCloseRequested, this, [this, view] {
        closeTab(tabs_->indexOf(view));
    });
    if (activate)
        tabs_->setCurrentWidget(view);
    if (!url.isEmpty())
        view->load(url);
    if (activate && (recentTabs_.isEmpty() || recentTabs_.first() != view)) activateCurrentTab();
    scheduleSleepingTabs();
    return view;
}

void BrowserWindow::closeTab(int index)
{
    auto *view = qobject_cast<QWebEngineView *>(tabs_->widget(index));
    if (!view)
        return;
    for (auto *download : downloads_) {
        if (download->page() == view->page()) {
            setError(view, "Finish or cancel this tab's downloads before closing it.");
            return;
        }
    }
    // Resolve capture callbacks while their page is still alive.
    for (auto *dialog : findChildren<QDialog *>("desktopCaptureDialog")) {
        if (dialog->property("capturePage").value<QObject *>() == view->page()) {
            dialog->reject(); delete dialog;
        }
    }
    QUrl url = view->url();
    if (url.isEmpty())
        url = view->page()->requestedUrl();
    closedTabs_.append({url.isEmpty() ? newTabUrl() : url, index});
    if (closedTabs_.size() > 20)
        closedTabs_.removeFirst();
    restoreAction_->setEnabled(true);
    auto *selected = currentView();
    recentTabs_.removeAll(view);
    {
        const QSignalBlocker blocked(tabs_);
        tabs_->removeTab(index);
        if (selected != view)
            tabs_->setCurrentWidget(selected);
        else if (!recentTabs_.isEmpty())
            tabs_->setCurrentWidget(recentTabs_.first());
    }
    view->disconnect(this);
    view->page()->disconnect(this);
    view->stop();
    view->deleteLater();
    if (tabs_->count() == 0)
        addTab();
    else
        activateCurrentTab();
    if (tabs_->count() < 2) sleepTimer_->stop();
}

void BrowserWindow::restoreClosedTab()
{
    if (closedTabs_.isEmpty())
        return;
    const auto closed = closedTabs_.takeLast();
    auto *view = addTab(closed.url);
    tabs_->tabBar()->moveTab(tabs_->indexOf(view), qMin(closed.index, tabs_->count() - 1));
    restoreAction_->setEnabled(!closedTabs_.isEmpty());
}

QWebEngineView *BrowserWindow::currentView() const
{
    return qobject_cast<QWebEngineView *>(tabs_->currentWidget());
}

void BrowserWindow::activateCurrentTab()
{
    if (!private_) sessionTimer_->start();
    if (auto *view = currentView()) {
        newTabStore_->setActive(view->url() == newTabUrl());
        if (lastActive_ && lastActive_ != view) lastActive_->setProperty("inactiveSince", clock_.elapsed());
        lastActive_ = view;
        view->page()->setLifecycleState(QWebEnginePage::LifecycleState::Active);
        tabs_->setTabToolTip(tabs_->currentIndex(), tabs_->tabText(tabs_->currentIndex()));
        if (keepAwake_) {
            const QSignalBlocker blocked(keepAwake_);
            keepAwake_->setChecked(view->property("keepAwake").toBool());
        }
        recentTabs_.removeAll(view);
        recentTabs_.prepend(view);
        updateControls();
        setWindowTitle(tabs_->tabText(tabs_->currentIndex()) + (private_ ? " — Prism Browser · Private" : " — Prism Browser"));
        scheduleSleepingTabs();
    }
}

void BrowserWindow::navigateFromAddress()
{
    const auto result = resolveAddress(address_->text(), searchEngine_);
    if (!result.error.isEmpty()) {
        setError(currentView(), result.error);
        return;
    }
    currentView()->load(result.url);
    currentView()->setFocus();
}

void BrowserWindow::updateControls(bool updateAddress)
{
    auto *view = currentView();
    if (!view)
        return;
    if (updateAddress) {
        const auto url = view->url();
        address_->setText(url == newTabUrl() || url.isEmpty() ? QString() : url.toDisplayString());
    }
    back_->setEnabled(view->history()->canGoBack());
    forward_->setEnabled(view->history()->canGoForward());
    const bool loading = view->page()->isLoading();
    if (!reload_->property("loadingState").isValid() || reload_->property("loadingState").toBool() != loading) {
        reload_->setIcon(loading ? stopIcon_ : reloadIcon_);
        reload_->setProperty("loadingState", loading);
    }
    reload_->setToolTip(loading ? "Stop loading (Esc)" : "Reload (Ctrl+R)");
    reload_->setAccessibleName(loading ? "Stop loading" : "Reload");
    progress_->setValue(view->property("progress").toInt());
    progress_->setVisible(loading);
    errorText_->setText(view->property("errorMessage").toString());
    errorBanner_->setVisible(!errorText_->text().isEmpty());
}

void BrowserWindow::setError(QWebEngineView *view, const QString &message)
{
    view->setProperty("errorMessage", message);
    if (view == currentView())
        updateControls(false);
}

void BrowserWindow::applyTheme()
{
    dark_ = theme_ == "dark" || (theme_ == "system" && qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark);
    QPalette colors;
    colors.setColor(QPalette::Window, QColor(dark_ ? "#101010" : "#fafafa"));
    colors.setColor(QPalette::WindowText, QColor(dark_ ? "#f5f5f5" : "#202020"));
    colors.setColor(QPalette::Base, QColor(dark_ ? "#242424" : "#ffffff"));
    colors.setColor(QPalette::Text, colors.color(QPalette::WindowText));
    colors.setColor(QPalette::Button, colors.color(QPalette::Window));
    colors.setColor(QPalette::ButtonText, colors.color(QPalette::WindowText));
    colors.setColor(QPalette::Highlight, QColor(accent_));
    colors.setColor(QPalette::HighlightedText, qGray(QColor(accent_).rgb()) > 150 ? QColor("#101010") : QColor(Qt::white));
    colors.setColor(QPalette::Disabled, QPalette::Text, QColor(dark_ ? "#707070" : "#a0a0a0"));
    colors.setColor(QPalette::Disabled, QPalette::ButtonText, colors.color(QPalette::Disabled, QPalette::Text));
    setPalette(colors);
    reloadIcon_ = symbolIcon("reload", QColor("#f5f5f5"));
    stopIcon_ = symbolIcon("stop", QColor("#f5f5f5"));
    reload_->setProperty("loadingState", QVariant());
    const QString background = dark_ ? "#101010" : "#fafafa";
    const QString surface = dark_ ? "#242424" : "#ffffff";
    const QString text = dark_ ? "#f5f5f5" : "#202020";
    const QString muted = dark_ ? "#aaaaaa" : "#686868";
    const QString border = dark_ ? "#333333" : "#d4d4d4";
    const QString hover = dark_ ? "#292929" : "#e8e8e8";
    const QString toolbar = "#000000";
    const QString tabbar = "#000000";
    const QString activeTab = "#1c1c1c";
    const QString accent = accent_;
    setStyleSheet(QString(R"(
        QMainWindow { background: %1; }
        QWidget { font-family: 'Segoe UI'; font-size: 12px; color: %3; }
        QFrame#toolbar, QTabBar { color: #f5f5f5; }
        QFrame#toolbar QLabel, QFrame#toolbar QToolButton { color: #f5f5f5; }
        QFrame#toolbar { background: %8; border-bottom: 1px solid %5; }
        QLabel#brand { font-size: 11px; font-weight: 700; letter-spacing: 2px; padding-left: 3px; }
        QToolButton { border: 1px solid transparent; border-radius: 4px; padding: 4px; background: transparent; }
        QToolButton:hover { background: %6; }
        QToolButton:pressed { background: %5; }
        QToolButton:focus { border: 1px solid %7; }
        QToolButton::menu-indicator { image: none; width: 0; }
        QLineEdit#address { background: #101010; color: #f5f5f5; border: 1px solid #282828; border-radius: 6px;
                           padding: 3px 16px; font-size: 13px; }
        QLineEdit#address:focus { border: 1px solid %7; }
        QTabWidget#tabs::pane { border: 0; border-top: 1px solid %5; }
        QTabWidget#tabs { background: %9; }
        QTabBar { background: %9; qproperty-drawBase: 0; }
        QTabBar::tab { background: transparent; border: 1px solid transparent;
                      border-radius: 4px; min-width: 96px; max-width: 210px; height: 28px;
                      padding: 2px 9px; margin: 3px 2px; color: #aaaaaa; }
        QToolButton#tabClose { padding: 0; margin: 0; }
        QTabBar::tab:selected { background: %10; border-color: #282828; border-top-color: %7; color: #f5f5f5; }
        QTabBar::tab:!selected:hover { background: #202020; color: #f5f5f5; }
        QTabBar::close-button { subcontrol-position: right; }
        QToolButton#newTab { margin: 3px 5px; }
        QProgressBar { border: 0; background: %1; }
        QProgressBar::chunk { background: %7; }
        QFrame#errorBanner { background: %6; border-bottom: 1px solid %5; }
        QMenu { background: %2; border: 1px solid %5; border-radius: 4px; padding: 6px; }
        QMenu::item { padding: 8px 28px; border-radius: 5px; }
        QMenu::item:selected { background: %6; }
        QMenu::item:disabled { color: %4; }
        QMenu::separator { height: 1px; background: %5; margin: 5px 8px; }
    )").arg(background, surface, text, muted, border, hover, accent, toolbar, tabbar, activeTab));
    for (auto *button : findChildren<QToolButton *>()) {
        const QString name = button->property("symbol").toString();
        if (!name.isEmpty()) {
            button->setIcon(symbolIcon(name, QColor("#f5f5f5")));
            button->setIconSize(button->objectName() == "tabClose" ? QSize(12, 12) : QSize(18, 18));
        }
    }
    for (int i = 0; i < tabs_->count(); ++i)
        updateStartPage(qobject_cast<QWebEngineView *>(tabs_->widget(i)));
    updateControls(false);
}

void BrowserWindow::updateStartPage(QWebEngineView *view)
{
    if (view->url() == newTabUrl()) {
        if (view == currentView()) newTabStore_->setActive(true);
        view->page()->setBackgroundColor(QColor(dark_ ? "#101010" : "#fafafa"));
        const QString ink = qGray(QColor(accent_).rgb()) > 150 ? "#101010" : "#ffffff";
        view->page()->runJavaScript(QString("document.documentElement.dataset.theme='%1';"
            "document.documentElement.style.setProperty('--accent','%2');"
            "document.documentElement.style.setProperty('--accent-ink','%3');")
            .arg(dark_ ? "dark" : "light", accent_, ink));
    }
}

void BrowserWindow::closeEvent(QCloseEvent *event)
{
    if (!downloads_.isEmpty()) {
        setError(currentView(), "Downloads are active. Finish or cancel them before quitting.");
        event->ignore();
        return;
    }
    if (!private_) QSettings().setValue("window/geometry", saveGeometry());
    saveSession();
    QMainWindow::closeEvent(event);
}

void BrowserWindow::setSavingsMode(const QString &mode)
{
    savingsMode_ = mode;
    if (!private_) QSettings().setValue("performance/mode", mode);
    for (int i = 0; i < tabs_->count(); ++i)
        configureNavigationCache(qobject_cast<QWebEngineView *>(tabs_->widget(i)));
    if (mode == "off") {
        sleepTimer_->stop();
        for (int i = 0; i < tabs_->count(); ++i) {
            auto *page = qobject_cast<QWebEngineView *>(tabs_->widget(i))->page();
            if (page->lifecycleState() == QWebEnginePage::LifecycleState::Frozen)
                page->setLifecycleState(QWebEnginePage::LifecycleState::Active);
        }
    }
    scheduleSleepingTabs();
}

void BrowserWindow::configureNavigationCache(QWebEngineView *view)
{
    const auto scheme = view->url().scheme();
    // Enable after loading so a departing new-tab page is not retained in history's renderer cache.
    const bool enabled = savingsMode_ != "maximum" && (scheme == "http" || scheme == "https");
    view->settings()->setAttribute(QWebEngineSettings::BackForwardCacheEnabled, enabled);
}

void BrowserWindow::evaluateSleepingTabs()
{
    using State = QWebEnginePage::LifecycleState;
    if (savingsMode_ == "off") return;
    evaluatingSleep_ = true;
    const qint64 delay = savingsMode_ == "maximum" ? 60000 : 300000;
    for (int i = 0; i < tabs_->count(); ++i) {
        auto *view = qobject_cast<QWebEngineView *>(tabs_->widget(i));
        auto *page = view->page();
        if (page->lifecycleState() == State::Discarded || view == currentView() || page->isVisible() || page->isLoading() || page->recentlyAudible()
            || page->devToolsPage() || !downloads_.isEmpty() || view->property("keepAwake").toBool()
            || view->property("permissionPending").toBool() || view->property("permissionGranted").toBool()
            || page->recommendedState() == State::Active) continue;
        if (clock_.elapsed() - view->property("inactiveSince").toLongLong() >= delay) {
            if (page->lifecycleState() == State::Active) {
                page->setLifecycleState(State::Frozen);
                tabs_->setTabToolTip(i, tabs_->tabText(i) + " (sleeping; select to resume)");
            }
            const auto scheme = view->url().scheme();
            if (savingsMode_ == "maximum" && clock_.elapsed() - view->property("inactiveSince").toLongLong() >= 600000
                && page->recommendedState() == State::Discarded && (scheme == "http" || scheme == "https")
                && !view->property("formNavigation").toBool() && !view->property("userInteracted").toBool()) {
                page->setLifecycleState(State::Discarded);
                tabs_->setTabToolTip(i, tabs_->tabText(i) + " (memory released; select to reload)");
            }
        }
    }
    evaluatingSleep_ = false;
    scheduleSleepingTabs();
}

void BrowserWindow::scheduleSleepingTabs()
{
    if (evaluatingSleep_) return;
    sleepTimer_->stop();
    if (savingsMode_ == "off" || tabs_->count() < 2 || !downloads_.isEmpty()) return;
    using State = QWebEnginePage::LifecycleState;
    qint64 next = -1;
    for (int i = 0; i < tabs_->count(); ++i) {
        auto *view = qobject_cast<QWebEngineView *>(tabs_->widget(i));
        auto *page = view->page();
        if (view == currentView() || page->isVisible() || page->isLoading() || page->recentlyAudible()
            || page->devToolsPage() || view->property("keepAwake").toBool() || view->property("permissionPending").toBool()
            || view->property("permissionGranted").toBool() || page->recommendedState() == State::Active) continue;
        qint64 deadline = -1;
        if (page->lifecycleState() == State::Active) deadline = savingsMode_ == "maximum" ? 60000 : 300000;
        else if (savingsMode_ == "maximum" && page->lifecycleState() == State::Frozen && page->recommendedState() == State::Discarded
                 && (view->url().scheme() == "http" || view->url().scheme() == "https")
                 && !view->property("formNavigation").toBool() && !view->property("userInteracted").toBool()) deadline = 600000;
        if (deadline < 0) continue;
        const qint64 remaining = qMax(1000LL, deadline - (clock_.elapsed() - view->property("inactiveSince").toLongLong()));
        if (next < 0 || remaining < next) next = remaining;
    }
    if (next >= 0) sleepTimer_->start(int(qMin(next, 600000LL)));
}

void BrowserWindow::showPrivacy()
{
    if (auto *existing = findChild<QDialog *>("privacyDialog")) { existing->show(); existing->raise(); return; }
    auto *dialog = new QDialog(this);
    dialog->setWindowTitle("Privacy and site data"); dialog->setObjectName("privacyDialog");
    dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->resize(760, 520);
    auto *layout = new QVBoxLayout(dialog);
    auto *notice = new QLabel(private_ ? "Private window: browsing records and native new-tab files are temporary. Downloads you save remain on disk."
        : "Browsing data stays on this computer. Deleting cookies can sign you out. Cookies and cache are not all website storage.", dialog);
    notice->setWordWrap(true); layout->addWidget(notice);
    auto *status = new QLabel(shields_->status(), dialog); status->setWordWrap(true); layout->addWidget(status);
    connect(shields_, &Shields::changed, dialog, [this, status] { status->setText(shields_->status()); });
    if (!data_->error().isEmpty()) status->setText(data_->error());
    connect(data_, &BrowserData::changed, dialog, [this, status] { if (!data_->error().isEmpty()) status->setText(data_->error()); });
    auto *pages = new QTabWidget(dialog); layout->addWidget(pages, 1);
    auto page = [pages](const QString &name, QTreeWidget **tree, const QStringList &headers) {
        auto *widget = new QWidget(pages); auto *box = new QVBoxLayout(widget);
        *tree = new QTreeWidget(widget); (*tree)->setHeaderLabels(headers); (*tree)->setRootIsDecorated(false);
        (*tree)->setSelectionMode(QAbstractItemView::ExtendedSelection); box->addWidget(*tree);
        pages->addTab(widget, name); return box;
    };
    QTreeWidget *cookies;
    auto *cookieLayout = page("Cookies", &cookies, {"Domain", "Name", "Path"}); cookies->setObjectName("cookieList");
    auto *removeCookie = new QPushButton("Delete selected cookies", dialog); cookieLayout->addWidget(removeCookie);
    auto *store = profile_->cookieStore();
    connect(store, &QWebEngineCookieStore::cookieAdded, dialog, [cookies](const QNetworkCookie &cookie) {
        for (int i = 0; i < cookies->topLevelItemCount(); ++i) {
            auto *item = cookies->topLevelItem(i);
            if (item->text(0) == cookie.domain() && item->text(1) == QString::fromUtf8(cookie.name()) && item->text(2) == cookie.path()) {
                item->setData(0, Qt::UserRole, cookie.toRawForm()); return;
            }
        }
        auto *item = new QTreeWidgetItem(cookies, {cookie.domain(), QString::fromUtf8(cookie.name()), cookie.path()});
        item->setData(0, Qt::UserRole, cookie.toRawForm());
    });
    connect(store, &QWebEngineCookieStore::cookieRemoved, dialog, [cookies](const QNetworkCookie &cookie) {
        for (int i = cookies->topLevelItemCount() - 1; i >= 0; --i) {
            auto *item = cookies->topLevelItem(i);
            if (item->text(0) == cookie.domain() && item->text(1) == QString::fromUtf8(cookie.name()) && item->text(2) == cookie.path()) delete item;
        }
    });
    connect(removeCookie, &QPushButton::clicked, dialog, [cookies, store] {
        for (auto *item : cookies->selectedItems()) {
            const auto parsed = QNetworkCookie::parseCookies(item->data(0, Qt::UserRole).toByteArray());
            if (!parsed.isEmpty()) store->deleteCookie(parsed.first());
        }
    });
    store->loadAllCookies();
    QTreeWidget *permissions;
    auto *permissionLayout = page("Permissions", &permissions, {"Website", "Capability", "Decision"});
    const auto refreshPermissions = [this, permissions] {
        permissions->clear();
        for (const auto &permission : profile_->listAllPermissions()) {
            const auto key = QMetaEnum::fromType<QWebEnginePermission::PermissionType>().valueToKey(int(permission.permissionType()));
            auto *item = new QTreeWidgetItem(permissions, {permission.origin().toDisplayString(), QString::fromLatin1(key ? key : "Permission"),
                permission.state() == QWebEnginePermission::State::Granted ? "Allowed" : "Blocked"});
            item->setData(0, Qt::UserRole, int(permission.permissionType()));
        }
    };
    refreshPermissions();
    auto *resetPermission = new QPushButton("Reset selected permissions (ask again)", dialog); permissionLayout->addWidget(resetPermission);
    connect(resetPermission, &QPushButton::clicked, dialog, [this, permissions, refreshPermissions] {
        for (auto *item : permissions->selectedItems())
            profile_->queryPermission(QUrl(item->text(0)), QWebEnginePermission::PermissionType(item->data(0, Qt::UserRole).toInt())).reset();
        refreshPermissions();
        setError(currentView(), "Permissions reset. Reload affected pages to end their existing access.");
    });
    QTreeWidget *history;
    auto *historyLayout = page("History", &history, {"Title", "Address", "Visited (UTC)"}); history->setObjectName("historyList");
    auto *searchHistory = new QLineEdit(dialog); searchHistory->setPlaceholderText("Search history"); historyLayout->insertWidget(0, searchHistory);
    connect(searchHistory, &QLineEdit::textChanged, dialog, [history](const QString &text) {
        for (int i = 0; i < history->topLevelItemCount(); ++i) { auto *item = history->topLevelItem(i); item->setHidden(!(item->text(0) + item->text(1)).contains(text, Qt::CaseInsensitive)); }
    });
    const auto refreshHistory = [this, history] {
        history->clear(); const auto records = data_->history();
        for (int i = records.size() - 1; i >= 0; --i) {
            const auto record = records[i].toObject();
            auto *item = new QTreeWidgetItem(history, {record.value("title").toString(), record.value("url").toString(), record.value("time").toString()});
            item->setData(0, Qt::UserRole, i);
        }
    };
    refreshHistory(); connect(data_, &BrowserData::changed, dialog, refreshHistory);
    connect(history, &QTreeWidget::itemActivated, dialog, [this](QTreeWidgetItem *item) { addTab(QUrl(item->text(1))); });
    auto *removeVisit = new QPushButton("Delete selected visits", dialog); historyLayout->addWidget(removeVisit);
    connect(removeVisit, &QPushButton::clicked, dialog, [this, history] {
        QList<int> indexes;
        for (auto *item : history->selectedItems()) indexes.append(item->data(0, Qt::UserRole).toInt());
        data_->removeVisits(indexes);
    });
    auto *clearHistory = new QPushButton("Clear history", dialog); historyLayout->addWidget(clearHistory);
    connect(clearHistory, &QPushButton::clicked, dialog, [this, dialog] {
        if (QMessageBox::question(dialog, "Clear history", "Delete all Prism visit history and visited-link markers? This cannot be undone.") != QMessageBox::Yes) return;
        data_->clearHistory(); profile_->clearAllVisitedLinks();
    });
    auto *clear = new QPushButton("Clear cookies and HTTP cache", dialog); layout->addWidget(clear);
    connect(clear, &QPushButton::clicked, dialog, [this, dialog, status] {
        if (QMessageBox::question(dialog, "Clear cookies and cache", "Delete this window profile's cookies and HTTP cache? This can sign you out. IndexedDB, local storage and service workers are retained.") != QMessageBox::Yes) return;
        profile_->cookieStore()->deleteAllCookies(); profile_->clearHttpCache();
        status->setText("Cookie/cache clearing requested. Other website storage is retained.");
    });
    if (!private_ && !profile_->isOffTheRecord()) {
        auto *reset = new QPushButton("Clear ALL website storage on next launch…", dialog); layout->addWidget(reset);
        connect(reset, &QPushButton::clicked, dialog, [this, dialog, status] {
            if (QMessageBox::warning(dialog, "Clear all website data", "On Prism's next launch, delete all cookies, local storage, IndexedDB, service workers and HTTP cache for the default profile. You will be signed out. Bookmarks, photos and downloaded files are kept. Close every Prism window before launching again. This cannot be undone.", QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
            QSettings settings;
            settings.setValue("privacy/resetPaths", QStringList{profile_->persistentStoragePath(), profile_->cachePath()}); settings.sync();
            status->setText(settings.status() == QSettings::NoError ? "Website-data reset scheduled. Close all Prism windows, then launch again." : "The reset could not be scheduled.");
        });
    }
    auto *close = new QDialogButtonBox(QDialogButtonBox::Close, dialog); layout->addWidget(close);
    connect(close, &QDialogButtonBox::rejected, dialog, &QDialog::close); dialog->show();
}

void BrowserWindow::refreshDownloads()
{
    qint64 received = 0;
    for (auto *download : downloads_) received += download->receivedBytes();
    downloadsText_->setText(QString("%1 download(s) — %2 KiB received").arg(downloads_.size()).arg(received / 1024));
    downloadsBar_->show();
}
