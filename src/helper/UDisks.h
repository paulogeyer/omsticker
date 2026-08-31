#pragma once

#include <QString>
#include <QVariantMap>

class UDisks
{
public:
    QString error;

    QString blockPath(const QString &device) const;
    bool unmountTree(const QString &device);
    int openWriteFd(const QString &device);
    bool rescan(const QString &device);
    bool waitForBlock(const QString &device, int timeoutMs = 20000);
    bool formatPartition(const QString &partition, const QString &filesystem, const QString &label);

private:
    bool call(const QString &path, const QString &interface, const QString &method,
              const QList<QVariant> &args, QList<QVariant> *reply = nullptr, int timeoutMs = 300000);
};
