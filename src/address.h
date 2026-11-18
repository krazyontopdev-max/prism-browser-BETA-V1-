#pragma once

#include <QString>
#include <QUrl>

struct AddressResult {
    QUrl url;
    QString error;
};

AddressResult resolveAddress(const QString &text, const QString &engine = "google");
QUrl searchUrl(const QString &text, const QString &engine = "google");
QUrl newTabUrl();
