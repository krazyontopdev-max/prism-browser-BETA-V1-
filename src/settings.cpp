#include "browserwindow.h"
#include "browserdata.h"
#include "shields.h"
#include "localfilepicker.h"
#include <QFileDialog>
#include <QFile>
#include <QSaveFile>
#include <QWebEngineDownloadRequest>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWebEnginePermission>
#include <QWebEngineProfile>
#include <QWebEngineView>

namespace {
void stylePanel(QDialog *dialog)
{
    dialog->setStyleSheet("QDialog,QWidget{background:#101010;color:#f5f5f5;}"
        "QLineEdit,QComboBox,QTreeWidget,QListWidget{background:#181818;color:#f5f5f5;border:1px solid #333;padding:7px;}"
        "QPushButton{background:#242424;color:#f5f5f5;border:1px solid #333;padding:8px 12px;}"
        "QPushButton:hover{background:#303030;} QHeaderView::section{background:#242424;color:#f5f5f5;border:0;padding:6px;}"
        "QListWidget::item{padding:7px;} QListWidget::item:selected{background:#292929;} QScrollArea{border:0;}");
}
}

void BrowserWindow::saveSession()
{
    if (private_ || !tabs_->count()) return;
    QStringList urls;
    int selected = 0;
    for (int i = 0; i < tabs_->count() && urls.size() < 500; ++i) {
        auto *view = qobject_cast<QWebEngineView *>(tabs_->widget(i));
        const auto url = view->url();
        if (url != newTabUrl() && url != QUrl("about:blank")
            && (url.host().isEmpty() || (url.scheme() != "http" && url.scheme() != "https"))) continue;
        if (view == currentView()) selected = urls.size();
        urls.append(url.adjusted(QUrl::RemoveUserInfo).toString(QUrl::FullyEncoded));
    }
    QSettings settings;
    settings.setValue("session/urls", urls); settings.setValue("session/selected", selected);
    QStringList closed; for (const auto &tab : closedTabs_) closed.append(tab.url.toString(QUrl::FullyEncoded));
    settings.setValue("session/closed", closed);
    settings.sync();
    if (settings.status() != QSettings::NoError) setError(currentView(), "The tab session could not be saved.");
}

void BrowserWindow::showSettings()
{
    if (auto *existing = findChild<QDialog *>("settingsDialog")) { existing->show(); existing->raise(); return; }
    auto *dialog = new QDialog(this); dialog->setObjectName("settingsDialog");
    dialog->setWindowTitle("Prism Settings"); dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->resize(940, 680); stylePanel(dialog);
    auto *layout = new QVBoxLayout(dialog);
    auto *search = new QLineEdit(dialog); search->setObjectName("settingsSearch");
    search->setPlaceholderText("Search settings"); search->setAccessibleName("Search settings"); layout->addWidget(search);
    auto *body = new QHBoxLayout; layout->addLayout(body, 1);
    auto *sections = new QListWidget(dialog); sections->setFixedWidth(210); body->addWidget(sections);
    auto *pages = new QStackedWidget(dialog); body->addWidget(pages, 1);
    auto *status = new QLabel(private_ ? "Private changes apply to this window and are not saved." : "", dialog);
    status->setObjectName("settingsStatus"); status->setWordWrap(true);
    const auto addPage = [sections, pages](const QString &name, const QString &terms) {
        sections->addItem(name);
        auto *scroll = new QScrollArea(pages); scroll->setWidgetResizable(true);
        auto *widget = new QWidget(scroll); auto *box = new QVBoxLayout(widget);
        auto *heading = new QLabel(name, widget); auto font = heading->font(); font.setPointSize(18); font.setBold(true); heading->setFont(font); box->addWidget(heading);
        auto *form = new QFormLayout; form->setRowWrapPolicy(QFormLayout::WrapLongRows); box->addLayout(form); box->addStretch();
        widget->setProperty("searchTerms", name + " " + terms); scroll->setWidget(widget); pages->addWidget(scroll); return form;
    };
    const auto link = [dialog](QFormLayout *form, const QString &text, const std::function<void()> &run) {
        auto *button = new QPushButton(text, dialog); form->addRow(button); QObject::connect(button, &QPushButton::clicked, dialog, run); return button;
    };
    auto *general = addPage("General", "homepage home startup session restore");
    auto *home = new QLineEdit(homepage_ == newTabUrl().toString() ? "newtab" : homepage_, dialog);
    home->setObjectName("homepageSetting"); general->addRow("Homepage URL or newtab", home);
    auto *startup = new QComboBox(dialog); startup->addItem("New tab", "newtab"); startup->addItem("Homepage", "home"); startup->addItem("Restore last tab URLs", "session");
    startup->setCurrentIndex(qMax(0, startup->findData(startup_))); general->addRow("On startup", startup);
    auto *appearance = addPage("Appearance", "theme light dark system accent color");
    auto *theme = new QComboBox(dialog); theme->addItems({"System", "Light", "Dark"}); theme->setCurrentText(theme_ == "light" ? "Light" : theme_ == "dark" ? "Dark" : "System");
    appearance->addRow("Theme (applies immediately)", theme);
    connect(theme, &QComboBox::currentTextChanged, dialog, [this](const QString &name) { if (auto *action = findChild<QAction *>("theme" + name)) action->trigger(); });
    link(appearance, "Choose accent color…", [this] { findChild<QAction *>("customAccent")->trigger(); });
    auto *newTab = addPage("New Tab", "wallpapers categories custom rotation photo clock shortcuts");
    link(newTab, "Wallpaper, clock and shortcut controls…", [this] {
        auto *view = currentView()->url() == newTabUrl() ? currentView() : addTab();
        const auto open = [view] { if (view->url() == newTabUrl()) view->page()->runJavaScript("document.getElementById('settings').click()"); };
        if (view->page()->isLoading()) connect(view, &QWebEngineView::loadFinished, view, open, Qt::SingleShotConnection);
        else open();
    });
    auto *tabs = addPage("Tabs", "keep awake recently closed new close reopen session");
    auto *awake = new QCheckBox("Keep the current tab awake", dialog); awake->setChecked(currentView()->property("keepAwake").toBool()); tabs->addRow(awake);
    connect(awake, &QCheckBox::clicked, keepAwake_, &QAction::setChecked);
    link(tabs, "Reopen most recently closed tab", [this] { restoreClosedTab(); });
    auto *performance = addPage("Performance", "sleep balanced maximum savings off memory cache");
    auto *savings = new QComboBox(dialog); savings->addItem("Balanced", "balanced"); savings->addItem("Maximum Savings", "maximum"); savings->addItem("Off", "off");
    savings->setCurrentIndex(qMax(0, savings->findData(savingsMode_))); performance->addRow("Tab resource savings (immediate)", savings);
    connect(savings, &QComboBox::currentIndexChanged, dialog, [this, savings] { setSavingsMode(savings->currentData().toString()); });
    auto *sleepNote = new QLabel("Balanced preserves frozen page state. Maximum Savings may reload eligible untouched tabs. Keep Awake protects work Qt cannot detect. GPU, sandboxing and BFCache follow the existing browser policy.", dialog);
    sleepNote->setWordWrap(true); performance->addRow(sleepNote);
    auto *privacy = addPage("Privacy & Security", "cookies website site data clear history permissions private window");
    link(privacy, "Manage cookies, permissions and browsing data…", [this] { showPrivacy(); });
    link(privacy, "Open a private window", [this] { findChild<QAction *>("privateWindowAction")->trigger(); });
    auto *shields = addPage("Prism Shields", "tracker ad blocking exceptions list update standard strict off");
    auto *mode = new QComboBox(dialog); mode->addItems({"standard", "strict", "off"}); mode->setCurrentText(shields_->mode()); shields->addRow("Mode (immediate)", mode);
    connect(mode, &QComboBox::currentTextChanged, shields_, &Shields::setMode);
    auto *filterStatus = new QLabel(shields_->status(), dialog); filterStatus->setWordWrap(true); shields->addRow(filterStatus);
    connect(shields_, &Shields::changed, dialog, [this, filterStatus] { filterStatus->setText(shields_->status()); });
    auto *exceptions = new QListWidget(dialog); exceptions->addItems(shields_->exceptions()); shields->addRow("Sites with Shields disabled", exceptions);
    link(shields, "Add website exception…", [this, dialog, exceptions, status] {
        bool ok; const auto host = QInputDialog::getText(dialog, "Website exception", "Website host (for example example.com)", QLineEdit::Normal, currentView()->url().host(), &ok).trimmed();
        if (!ok) return; shields_->setException(host, true);
        const auto normalized = QString::fromLatin1(QUrl::toAce(host));
        if (!shields_->exceptions().contains(normalized)) { status->setText("Enter a valid domain name, without a path or scheme."); return; }
        exceptions->clear(); exceptions->addItems(shields_->exceptions());
    });
    link(shields, "Remove selected exception", [this, exceptions] {
        if (auto *item = exceptions->currentItem()) shields_->setException(item->text(), false);
        exceptions->clear(); exceptions->addItems(shields_->exceptions());
    });
    link(shields, "Update domain list", [this] { shields_->updateLists(); })->setEnabled(!private_);
    auto *searchPage = addPage("Search", "search engine google duckduckgo bing address bar");
    auto *engine = new QComboBox(dialog); engine->setObjectName("searchEngineSetting");
    engine->addItem("Google", "google"); engine->addItem("DuckDuckGo", "duckduckgo"); engine->addItem("Bing", "bing"); engine->setCurrentIndex(qMax(0, engine->findData(searchEngine_))); searchPage->addRow("Default search engine", engine);
    auto *downloads = addPage("Downloads", "save folder ask location files");
    auto *folder = new QLineEdit(profile_->downloadPath(), dialog); folder->setObjectName("downloadDirectorySetting"); downloads->addRow("Download folder", folder);
    auto *ask = new QCheckBox("Ask where to save each file", dialog); ask->setChecked(askDownloads_); downloads->addRow(ask);
    auto *permissions = addPage("Site Permissions", "camera microphone notifications location clipboard remember revoke");
    auto *remember = new QCheckBox("Remember permission decisions", dialog); remember->setChecked(profile_->persistentPermissionsPolicy() != QWebEngineProfile::PersistentPermissionsPolicy::AskEveryTime); permissions->addRow(remember);
    link(permissions, "Review or revoke website permissions…", [this] { showPrivacy(); });
    auto *profiles = addPage("Profiles", "local profiles isolated cookies data accounts");
    profiles->addRow(new QLabel("Current local profile: " + QCoreApplication::applicationName(), dialog));
    link(profiles, "Create or open a local profile…", [this] { showProfiles(); })->setEnabled(!private_);
    if (private_) profiles->addRow(new QLabel("Manage ordinary profiles from an ordinary window. Private windows do not create persistent profiles.", dialog));
    auto *history = addPage("History & Bookmarks", "history visits bookmarks favorites manage delete");
    link(history, "Manage history…", [this] { showPrivacy(); }); link(history, "Manage bookmarks…", [this] { showBookmarks(); });
    auto *shortcuts = addPage("Keyboard Shortcuts", "keyboard keys navigation shortcut hotkey");
    auto *keys = new QTreeWidget(dialog); keys->setHeaderLabels({"Action", "Shortcut"}); keys->setRootIsDecorated(false);
    for (auto *action : actions()) if (!action->shortcuts().isEmpty()) {
        QStringList labels; for (const auto &key : action->shortcuts()) labels.append(key.toString(QKeySequence::NativeText));
        new QTreeWidgetItem(keys, {action->text(), labels.join(", ")});
    }
    shortcuts->addRow(keys);
    auto *advanced = addPage("Advanced", "local browser files profile directory defaults");
    link(advanced, "Open local browser data folder", [] { QDesktopServices::openUrl(QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))); });
    link(advanced, "Reset appearance to defaults", [this] { findChild<QAction *>("themeSystem")->trigger(); findChild<QAction *>("accent_9b7dff")->trigger(); });
    auto *about = addPage("About", "version qt licenses offline wallpapers compatibility codecs");
    auto *details = new QLabel("Prism Browser " + QCoreApplication::applicationVersion() + "\nQt " + qVersion()
        + "\nLocal browser data. No Prism telemetry or remote crash uploads."
          "\nMedia compatibility depends on the WebEngine build; see the release notes.", dialog);
    details->setWordWrap(true); about->addRow(details);
    link(about, "Offline wallpaper credits", [this] { addTab(QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + "/wallpapers/CREDITS.html")); });
    link(about, "Qt / Chromium component licenses", [this] { addTab(QUrl("chrome://credits")); });
    connect(sections, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex); sections->setCurrentRow(0);
    connect(search, &QLineEdit::textChanged, dialog, [sections, pages](const QString &text) {
        int first = -1;
        for (int i = 0; i < sections->count(); ++i) {
            auto *scroll = qobject_cast<QScrollArea *>(pages->widget(i));
            const bool match = scroll->widget()->property("searchTerms").toString().contains(text.trimmed(), Qt::CaseInsensitive);
            sections->item(i)->setHidden(!match); if (match && first < 0) first = i;
        }
        if (first >= 0) sections->setCurrentRow(first);
        pages->setVisible(first >= 0);
    });
    layout->addWidget(status);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Close, dialog); layout->addWidget(buttons);
    auto *apply = buttons->button(QDialogButtonBox::Apply); apply->setObjectName("settingsApply");
    connect(apply, &QPushButton::clicked, dialog, [this, home, startup, engine, folder, ask, remember, status] {
        const auto text = home->text().trimmed(); const QUrl url(text == "newtab" || text.isEmpty() ? newTabUrl() : QUrl(text));
        if (url != newTabUrl() && url != QUrl("about:blank") && (!url.isValid() || url.host().isEmpty() || (url.scheme() != "http" && url.scheme() != "https"))) {
            status->setText("Use an HTTP(S) homepage URL, about:blank, or newtab."); return;
        }
        const QFileInfo directory(folder->text().trimmed());
        if (!directory.isDir() || !directory.isWritable()) { status->setText("Choose an existing writable download folder."); return; }
        homepage_ = url.toString(QUrl::FullyEncoded); startup_ = startup->currentData().toString(); searchEngine_ = engine->currentData().toString(); askDownloads_ = ask->isChecked();
        profile_->setDownloadPath(directory.absoluteFilePath());
        profile_->setPersistentPermissionsPolicy(remember->isChecked() ? (profile_->isOffTheRecord() ? QWebEngineProfile::PersistentPermissionsPolicy::StoreInMemory : QWebEngineProfile::PersistentPermissionsPolicy::StoreOnDisk) : QWebEngineProfile::PersistentPermissionsPolicy::AskEveryTime);
        for (int i = 0; i < tabs_->count(); ++i) qobject_cast<QWebEngineView *>(tabs_->widget(i))->page()->setProperty("searchEngine", searchEngine_);
        if (!private_) {
            QSettings settings; settings.setValue("general/homepage", homepage_); settings.setValue("general/startup", startup_); settings.setValue("search/engine", searchEngine_);
            settings.setValue("downloads/directory", directory.absoluteFilePath()); settings.setValue("downloads/ask", askDownloads_); settings.setValue("permissions/remember", remember->isChecked()); settings.sync();
            if (settings.status() != QSettings::NoError) { status->setText("Settings applied for this window, but could not be saved."); return; }
        }
        status->setText(private_ ? "Applied for this private window; not saved." : "Settings saved.");
    });
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    dialog->show();
}

void BrowserWindow::showBookmarks()
{
    if (auto *existing = findChild<QDialog *>("bookmarksDialog")) { existing->show(); existing->raise(); return; }
    auto *dialog = new QDialog(this); dialog->setObjectName("bookmarksDialog"); dialog->setWindowTitle("Bookmarks");
    dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->resize(760, 500); stylePanel(dialog);
    auto *layout = new QVBoxLayout(dialog); auto *search = new QLineEdit(dialog); search->setPlaceholderText("Search bookmarks"); layout->addWidget(search);
    auto *tree = new QTreeWidget(dialog); tree->setHeaderLabels({"Title", "Address", "Folder"}); tree->setRootIsDecorated(false); tree->setSelectionMode(QAbstractItemView::ExtendedSelection); layout->addWidget(tree, 1);
    auto *status = new QLabel(private_ ? "Private bookmarks are temporary." : data_->error(), dialog); status->setWordWrap(true); layout->addWidget(status);
    const auto refresh = [this, tree, search] {
        tree->clear();
        for (const auto &value : data_->bookmarks()) {
            const auto row = value.toObject();
            if (!(row.value("title").toString() + row.value("url").toString() + row.value("folder").toString()).contains(search->text(), Qt::CaseInsensitive)) continue;
            auto *item = new QTreeWidgetItem(tree, {row.value("title").toString(), row.value("url").toString(), row.value("folder").toString()}); item->setData(0, Qt::UserRole, row.value("id").toString());
        }
    };
    refresh(); connect(data_, &BrowserData::changed, dialog, refresh); connect(search, &QLineEdit::textChanged, dialog, refresh);
    connect(tree, &QTreeWidget::itemActivated, dialog, [this](QTreeWidgetItem *item) { addTab(QUrl(item->text(1))); });
    auto *buttons = new QHBoxLayout; layout->addLayout(buttons);
    const auto edit = [this, dialog, tree, status](bool existing) {
        auto *item = existing ? tree->currentItem() : nullptr; if (existing && !item) return;
        auto *editor = new QDialog(dialog); editor->setObjectName("bookmarkEditor"); editor->setWindowTitle(existing ? "Edit bookmark" : "Add bookmark"); editor->setAttribute(Qt::WA_DeleteOnClose);
        auto *form = new QFormLayout(editor); auto *title = new QLineEdit(item ? item->text(0) : currentView()->title(), editor); auto *url = new QLineEdit(item ? item->text(1) : currentView()->url().toString(), editor);
        form->addRow("Title", title); form->addRow("HTTP(S) address", url);
        auto *folder = new QLineEdit(item ? item->text(2) : QString(), editor); form->addRow("Folder (optional)", folder);
        auto *ok = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, editor); form->addRow(ok);
        const auto id = item ? item->data(0, Qt::UserRole).toString() : QString();
        connect(ok, &QDialogButtonBox::accepted, editor, [this, editor, title, url, folder, id, status] {
            const QUrl address(url->text().trimmed());
            if (!data_->setBookmark(id, title->text(), address, folder->text())) { status->setText("Bookmark not saved. Use a valid HTTP(S) address. " + data_->error()); return; }
            editor->accept();
        });
        connect(ok, &QDialogButtonBox::rejected, editor, &QDialog::reject); editor->show();
    };
    for (const auto &label : {"Add…", "Edit…", "Delete selected", "Close"}) {
        auto *button = new QPushButton(QString::fromUtf8(label), dialog); buttons->addWidget(button);
        const QString text = QString::fromUtf8(label);
        connect(button, &QPushButton::clicked, dialog, [this, dialog, tree, edit, text, status] {
            if (text == "Close") dialog->close(); else if (text == "Add…") edit(false); else if (text == "Edit…") edit(true);
            else {
                QStringList ids; for (auto *item : tree->selectedItems()) ids.append(item->data(0, Qt::UserRole).toString());
                if (!data_->removeBookmarks(ids)) status->setText(data_->error());
            }
        });
    }
    for (bool importing : {true, false}) {
        auto *button = new QPushButton(importing ? "Import Prism JSON…" : "Export Prism JSON…", dialog); buttons->addWidget(button);
        connect(button, &QPushButton::clicked, dialog, [this, dialog, status, importing] {
            const auto selected = [this, status, importing](const QString &path) {
                if (importing) {
                    QFile file(path);
                    if (!file.open(QIODevice::ReadOnly) || file.size() > 4 * 1024 * 1024 || !data_->importBookmarks(file.readAll())) status->setText("Import rejected. Existing bookmarks were kept. Use a Prism JSON export, at most 4 MiB / 5000 entries.");
                    else status->setText("Bookmarks imported. Duplicate addresses in the same folder were skipped.");
                } else {
                    QSaveFile file(path); const auto bytes = data_->exportBookmarks();
                    status->setText(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit() ? "Bookmarks exported." : "Export could not be saved.");
                }
            };
            if (private_) {
                auto *picker = new LocalFilePicker(dialog, !importing, importing ? QString() : "prism-bookmarks.json");
                if (importing) connect(picker, &LocalFilePicker::pathsSelected, dialog, [selected](const QStringList &paths) { if (!paths.isEmpty()) selected(paths.first()); });
                else connect(picker, &LocalFilePicker::pathSelected, dialog, selected);
                picker->open();
            } else {
                auto *picker = new QFileDialog(dialog, importing ? "Import bookmarks" : "Export bookmarks"); picker->setAttribute(Qt::WA_DeleteOnClose);
                picker->setAcceptMode(importing ? QFileDialog::AcceptOpen : QFileDialog::AcceptSave); picker->setFileMode(importing ? QFileDialog::ExistingFile : QFileDialog::AnyFile);
                picker->setNameFilter("Prism bookmarks (*.json)"); if (!importing) picker->selectFile("prism-bookmarks.json");
                connect(picker, &QFileDialog::fileSelected, dialog, selected); picker->open();
            }
        });
    }
    dialog->show();
}

void BrowserWindow::showProfiles()
{
    if (private_) return;
    auto *dialog = new QDialog(this); dialog->setWindowTitle("Local profiles"); dialog->setAttribute(Qt::WA_DeleteOnClose); stylePanel(dialog);
    auto *layout = new QVBoxLayout(dialog); auto *list = new QListWidget(dialog); layout->addWidget(list);
    QSettings registry("Prism", "Prism"); auto names = registry.value("profiles/known").toStringList();
    registry.beginGroup("profiles/entries"); names.append(registry.childKeys()); names.removeDuplicates(); names.removeAll("default"); names.prepend("default"); list->addItems(names);
    auto *status = new QLabel("Each ordinary profile has separate local cookies, settings, history and bookmarks.", dialog); status->setWordWrap(true); layout->addWidget(status);
    auto *create = new QPushButton("Create local profile…", dialog); layout->addWidget(create);
    connect(create, &QPushButton::clicked, dialog, [list, status, dialog] {
        bool ok; const auto name = QInputDialog::getText(dialog, "Create profile", "Profile ID: letters, numbers, hyphens or underscores (1–32 characters)", QLineEdit::Normal, {}, &ok).trimmed().toLower();
        if (!ok) return;
        static const QRegularExpression valid("^[a-z0-9][a-z0-9_-]{0,31}$");
        if (!valid.match(name).hasMatch() || name == "default") { status->setText("Choose a unique profile ID using the allowed characters."); return; }
        QSettings settings("Prism", "Prism"); auto names = settings.value("profiles/known").toStringList();
        if (names.contains(name) || settings.contains("profiles/entries/" + name)) { status->setText("That profile already exists."); return; }
        settings.setValue("profiles/entries/" + name, true); settings.sync();
        if (settings.status() != QSettings::NoError) { status->setText("Profile could not be saved."); return; }
        list->addItem(name);
    });
    auto *open = new QPushButton("Open selected profile", dialog); layout->addWidget(open);
    connect(open, &QPushButton::clicked, dialog, [list, status] {
        if (!list->currentItem()) return;
        const auto name = list->currentItem()->text(); const QStringList arguments = name == "default" ? QStringList{} : QStringList{"--profile", name};
        if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), arguments)) status->setText("The profile could not be opened.");
    });
    dialog->show();
}

void BrowserWindow::showDownloads()
{
    auto *dialog = new QDialog(this); dialog->setWindowTitle("Downloads"); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->resize(780, 500); stylePanel(dialog);
    auto *layout = new QVBoxLayout(dialog); auto *tree = new QTreeWidget(dialog); tree->setHeaderLabels({"File", "State", "Path"}); tree->setRootIsDecorated(false); layout->addWidget(tree);
    const auto refresh = [this, tree] {
        tree->clear();
        for (auto *request : downloads_) {
            auto *row = new QTreeWidgetItem(tree, {request->downloadFileName(), request->isPaused() ? "Paused" : QString("%1 KiB received").arg(request->receivedBytes() / 1024), request->downloadDirectory()});
            row->setData(0, Qt::UserRole, request->id()); row->setData(0, Qt::UserRole + 1, true);
        }
        if (!private_) for (const auto &value : QSettings().value("downloads/history").toList()) {
            const auto record = value.toMap(); new QTreeWidgetItem(tree, {record.value("name").toString(), record.value("state").toString(), record.value("path").toString()});
        }
    };
    refresh();
    for (const auto &text : {"Refresh", "Pause / Resume", "Cancel", "Open file", "Show folder", "Clear history", "Close"}) {
        auto *button = new QPushButton(QString::fromUtf8(text), dialog); layout->addWidget(button);
        connect(button, &QPushButton::clicked, dialog, [this, tree, dialog, refresh, text = QString::fromUtf8(text)] {
            if (text == "Close") { dialog->close(); return; }
            if (text == "Refresh") { refresh(); return; }
            if (text == "Clear history") { if (!private_) QSettings().remove("downloads/history"); refresh(); return; }
            auto *row = tree->currentItem(); if (!row) return;
            if (row->data(0, Qt::UserRole + 1).toBool()) {
                const auto id = row->data(0, Qt::UserRole).toUInt();
                for (auto *request : downloads_) if (request->id() == id) {
                    if (text == "Cancel") request->cancel();
                    else if (text == "Pause / Resume") { if (request->isPaused()) request->resume(); else request->pause(); }
                    break;
                }
            } else if (text == "Show folder") QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(row->text(2)).absolutePath()));
            else if (text == "Open file" && QFileInfo(row->text(2)).isFile()) {
                const auto path = row->text(2);
                if (QMessageBox::question(dialog, "Open downloaded file", "Open this file in its associated Windows application? Only open files you trust.\n" + QFileInfo(path).fileName(), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
                    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
            }
            refresh();
        });
    }
    dialog->show();
}
