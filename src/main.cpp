#include "browserwindow.h"
#include "webdata.h"

#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QDir>
#include <QLockFile>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <QPointer>
#include <QRegularExpression>
#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Prism");
    QCoreApplication::setApplicationName("Prism");
    const auto arguments = app.arguments(); const auto profileOption = arguments.indexOf("--profile");
    if (profileOption >= 0) {
        const auto id = arguments.value(profileOption + 1).toLower();
        if (!QRegularExpression("^[a-z0-9][a-z0-9_-]{0,31}$").match(id).hasMatch()) {
            QMessageBox::warning(nullptr, "Invalid profile", "Profile IDs use 1–32 letters, numbers, hyphens or underscores."); return 1;
        }
        if (id != "default") QCoreApplication::setApplicationName("Prism-profile-" + id);
    }
    QCoreApplication::setApplicationVersion(PRISM_VERSION);
    QApplication::setApplicationDisplayName("Prism Browser");
    QApplication::setStyle("Fusion");
    QApplication::setWindowIcon(QIcon(":/branding/prism.ico"));
#ifdef Q_OS_WIN
    QApplication::setFont(QFont("Segoe UI", 10));
#endif
    const bool privateWindow = app.arguments().contains("--private");
    const auto dataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!privateWindow) QDir().mkpath(dataPath);
    QLockFile lock(dataPath + "/instance.lock");
    if (!privateWindow && !lock.tryLock(0)) {
        QMessageBox::information(nullptr, "Prism is running", "Use the existing Prism window, or close it before opening another ordinary window.");
        return 1;
    }
    QSettings settings;
    const auto resetPaths = settings.value("privacy/resetPaths").toStringList();
    if (!privateWindow && !resetPaths.isEmpty()) {
        bool otherProcess = false;
#ifdef Q_OS_WIN
        const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) otherProcess = true;
        else {
            PROCESSENTRY32W entry{}; entry.dwSize = sizeof(entry);
            for (BOOL next = Process32FirstW(snapshot, &entry); next; next = Process32NextW(snapshot, &entry))
                if (entry.th32ProcessID != GetCurrentProcessId() && QString::fromWCharArray(entry.szExeFile).compare("Prism.exe", Qt::CaseInsensitive) == 0) otherProcess = true;
            CloseHandle(snapshot);
        }
#endif
        QString error;
        if (otherProcess || !resetWebData(resetPaths, dataPath, &error)) {
            QMessageBox::warning(nullptr, "Website data was not fully cleared", otherProcess ? "Close every other Prism process, including private windows, then launch Prism again. The reset is still pending." : error);
            return 1;
        }
        settings.remove("privacy/resetPaths"); settings.sync();
        if (settings.status() != QSettings::NoError) {
            QMessageBox::warning(nullptr, "Reset could not be completed", "Prism cannot remove the pending-reset setting. Browsing will stay closed to prevent a repeated reset on a later launch.");
            return 1;
        }
    }
    bool restoreSession = false;
    if (!privateWindow) {
        if (settings.value("session/running", false).toBool() && !settings.value("session/urls").toStringList().isEmpty()) {
            const auto choice = QMessageBox::question(nullptr, "Recover Prism tabs",
                "Prism did not close normally. Restore the last saved tab URLs? Unsaved forms cannot be recovered.\n\nChoose No to use your configured startup behavior.",
                QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
            if (choice == QMessageBox::Cancel) return 0;
            restoreSession = choice == QMessageBox::Yes;
        }
        settings.setValue("session/running", true); settings.sync();
        if (settings.status() != QSettings::NoError) {
            QMessageBox::warning(nullptr, "Profile is not writable", "Prism cannot save recovery information. Check the profile folder and available disk space.");
            return 1;
        }
    }
    QPointer<BrowserWindow> window = new BrowserWindow(nullptr, privateWindow, restoreSession);
    window->setAttribute(Qt::WA_DeleteOnClose); window->show();
    const int result = app.exec();
    if (window) delete window;
    if (!privateWindow) { settings.setValue("session/running", false); settings.sync(); }
    return result;
}
