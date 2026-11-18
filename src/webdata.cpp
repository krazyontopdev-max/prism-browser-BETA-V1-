#include "webdata.h"
#include <QDir>
#include <QFileInfo>
#include <QFile>

namespace {
bool linked(const QString &path)
{
    const QFileInfo file(path);
    return file.isSymLink() || file.isJunction();
}
bool safeTree(const QString &path)
{
    if (linked(path)) return false;
    for (const auto &file : QDir(path).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System))
        if (linked(file.absoluteFilePath()) || (file.isDir() && !safeTree(file.absoluteFilePath()))) return false;
    return true;
}
bool removeTree(const QString &path)
{
    if (linked(path)) return false;
    for (const auto &file : QDir(path).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System)) {
        if (linked(file.absoluteFilePath())) return false;
        if (file.isDir() ? !removeTree(file.absoluteFilePath()) : !QFile::remove(file.absoluteFilePath())) return false;
    }
    return QDir().rmdir(path);
}
}

bool resetWebData(const QStringList &paths, const QString &applicationData, QString *error)
{
    const auto root = QDir::cleanPath(QFileInfo(applicationData).absoluteFilePath());
    if (linked(root)) { *error = "Website-data reset refused a linked application directory."; return false; }
    QStringList checked;
    for (const auto &path : paths) {
        const auto absolute = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
        const auto relative = QDir(root).relativeFilePath(absolute);
        if (relative != "QtWebEngine/Prism" && relative != "cache/QtWebEngine/Prism") {
            *error = "Website-data reset refused an unexpected storage path."; return false;
        }
        for (QString ancestor = absolute; ancestor.compare(root, Qt::CaseInsensitive) != 0; ancestor = QFileInfo(ancestor).absolutePath()) {
            const QFileInfo info(ancestor);
            if (info.isSymLink() || info.isJunction()) { *error = "Website-data reset refused a linked directory."; return false; }
        }
        checked.append(absolute);
    }
    for (const auto &path : checked) {
        if (!safeTree(path)) { *error = "Website-data reset refused a linked directory inside website storage."; return false; }
    }
    for (const auto &path : checked) {
        if (QFileInfo::exists(path) && !removeTree(path)) {
            *error = "Some website data could not be removed. Close all Prism processes and try again."; return false;
        }
    }
    return true;
}
