#include "newtabbridge.h"
#include "newtabstore.h"
#include "address.h"
#include "localfilepicker.h"

#include <QFileDialog>
#include <QWebEnginePage>
#include <QWebEngineView>

NewTabBridge::NewTabBridge(NewTabStore *store, QWebEnginePage *page, QObject *parent)
    : QObject(parent), store_(store), page_(page)
{
    connect(store, &NewTabStore::changed, this, [this] {
        if (allowed() && page_->isVisible()) emit stateChanged(store_->state());
    });
}
bool NewTabBridge::allowed() const { return page_ && page_->url() == newTabUrl(); }
QVariantMap NewTabBridge::getState() { return allowed() ? store_->state() : QVariantMap(); }
void NewTabBridge::setOptions(const QVariantMap &options) { if (allowed()) store_->setOptions(options); }
void NewTabBridge::addShortcut(const QString &name, const QString &url, int replace) { if (allowed()) store_->addShortcut(name, url, replace); }
void NewTabBridge::removeShortcut(int index) { if (allowed()) store_->removeShortcut(index); }
void NewTabBridge::removeCustom(const QString &id) { if (allowed()) store_->removeCustom(id); }
void NewTabBridge::refreshCatalog() { if (allowed()) store_->refreshCatalog(); }
void NewTabBridge::shuffle() { if (allowed()) store_->shuffle(); }
void NewTabBridge::toggleFullscreen() { if (allowed()) emit fullscreenRequested(); }
void NewTabBridge::openBrowserSettings() { if (allowed()) emit settingsRequested(); }
void NewTabBridge::search(const QString &text)
{
    if (!allowed()) return;
    auto url = searchUrl(text.trimmed(), page_->property("searchEngine").toString());
    if (!url.isEmpty()) {
        url.setQuery(url.query(QUrl::FullyEncoded).replace("%20", "+"));
        page_->load(url);
    }
}
void NewTabBridge::chooseImages()
{
    if (!allowed()) return;
    if (page_->property("privateBrowsing").toBool()) {
        auto *dialog = new LocalFilePicker(QWebEngineView::forPage(page_), false);
        dialog->setObjectName("wallpaperImportDialog");
        connect(dialog, &LocalFilePicker::pathsSelected, this, [this](const QStringList &paths) { if (allowed()) store_->importImages(paths); });
        connect(page_, &QObject::destroyed, dialog, &QWidget::close);
        dialog->open(); return;
    }
    auto *dialog = new QFileDialog(QWebEngineView::forPage(page_), "Choose local wallpapers");
    dialog->setObjectName("wallpaperImportDialog");
    dialog->setFileMode(QFileDialog::ExistingFiles);
    dialog->setNameFilter("Photographs (*.jpg *.jpeg *.png *.webp)");
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &QFileDialog::filesSelected, this, [this](const QStringList &paths) {
        if (allowed()) store_->importImages(paths);
    });
    connect(page_, &QObject::destroyed, dialog, &QWidget::close);
    dialog->open();
}
