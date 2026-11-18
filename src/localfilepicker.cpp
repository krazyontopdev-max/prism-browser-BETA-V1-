#include "localfilepicker.h"
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTreeView>
#include <QVBoxLayout>

LocalFilePicker::LocalFilePicker(QWidget *parent, bool save, const QString &suggestedName, const QString &directory)
    : QDialog(parent), save_(save), directory_(new QLineEdit(this)), name_(new QLineEdit(this)),
      files_(new QTreeView(this)), model_(new QFileSystemModel(this))
{
    setWindowTitle(save ? "Save private download" : "Choose local wallpapers");
    setAttribute(Qt::WA_DeleteOnClose); resize(760, 480);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Private chooser: folder history is not saved.", this));
    directory_->setAccessibleName("Folder"); directory_->setObjectName("pickerDirectory"); layout->addWidget(directory_);
    const auto initial = directory.isEmpty() ? QStandardPaths::writableLocation(save ? QStandardPaths::DownloadLocation : QStandardPaths::PicturesLocation) : directory;
    directory_->setText(QDir(initial).exists() ? initial : QDir::homePath());
    model_->setReadOnly(true); model_->setOption(QFileSystemModel::DontUseCustomDirectoryIcons);
    files_->setModel(model_); files_->setRootIndex(model_->setRootPath(directory_->text()));
    files_->setSelectionMode(save ? QAbstractItemView::SingleSelection : QAbstractItemView::ExtendedSelection);
    files_->setColumnWidth(0, 320); layout->addWidget(files_, 1);
    name_->setObjectName("pickerName"); name_->setAccessibleName(save ? "File name" : "File path (optional)");
    name_->setText(suggestedName); name_->setPlaceholderText(save ? "File name" : "Select photographs above, or enter a full file path"); layout->addWidget(name_);
    const auto openDirectory = [this](const QString &path) {
        const QFileInfo folder(path);
        if (folder.isDir()) { directory_->setText(folder.absoluteFilePath()); files_->setRootIndex(model_->setRootPath(folder.absoluteFilePath())); }
    };
    connect(directory_, &QLineEdit::returnPressed, this, [this, openDirectory] { openDirectory(directory_->text()); });
    connect(files_, &QTreeView::doubleClicked, this, [this, openDirectory](const QModelIndex &index) {
        const auto info = model_->fileInfo(index);
        if (info.isDir()) openDirectory(info.absoluteFilePath());
        else { name_->setText(info.absoluteFilePath()); if (!save_) accept(); }
    });
    auto *buttons = new QDialogButtonBox(save ? QDialogButtonBox::Save | QDialogButtonBox::Cancel : QDialogButtonBox::Open | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &LocalFilePicker::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject); layout->addWidget(buttons);
}

void LocalFilePicker::accept()
{
    QStringList paths;
    if (!name_->text().trimmed().isEmpty()) {
        const auto input = QDir::fromNativeSeparators(name_->text().trimmed());
        paths.append(QDir::isAbsolutePath(input) ? QDir::cleanPath(input) : QDir(directory_->text()).absoluteFilePath(input));
    } else if (!save_) {
        for (const auto &index : files_->selectionModel()->selectedRows())
            if (model_->fileInfo(index).isFile()) paths.append(model_->filePath(index));
    }
    if (paths.isEmpty()) return;
    if (save_) {
        const QFileInfo file(paths.first());
        if (!QDir(file.absolutePath()).exists() || file.isDir()) { QMessageBox::warning(this, "Choose a file", "Choose an existing folder and a file name."); return; }
        if (file.exists() && QMessageBox::question(this, "Replace file?", "Replace " + file.fileName() + "?", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        emit pathSelected(file.absoluteFilePath());
    } else {
        for (const auto &path : paths) if (!QFileInfo(path).isFile()) { QMessageBox::warning(this, "File not found", "Choose existing local photographs."); return; }
        emit pathsSelected(paths);
    }
    QDialog::accept();
}
