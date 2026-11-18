#pragma once
#include <QDialog>
#include <QStringList>
class QFileSystemModel;
class QLineEdit;
class QTreeView;

// QFileDialog persists its navigation history; private windows use this local chooser.
class LocalFilePicker : public QDialog {
    Q_OBJECT
public:
    LocalFilePicker(QWidget *parent, bool save, const QString &suggestedName = {}, const QString &directory = {});
    void accept() override;
signals:
    void pathSelected(const QString &path);
    void pathsSelected(const QStringList &paths);
private:
    bool save_;
    QLineEdit *directory_;
    QLineEdit *name_;
    QTreeView *files_;
    QFileSystemModel *model_;
};
