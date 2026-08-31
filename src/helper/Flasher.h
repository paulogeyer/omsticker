#pragma once

#include "UDisks.h"

#include <QString>

class Flasher
{
public:
    QString isoPath;
    QString devicePath;
    QString filesystem = QStringLiteral("exfat");
    QString label = QStringLiteral("OMARCHY");
    bool makeDataPartition = true;
    qint64 resumeOffset = 0;

    int run();

private:
    UDisks m_udisks;
    QString m_canonicalDevice;
    QString m_dataPartition;
    bool m_imageWritten = false;
    int m_devFd = -1;

    bool fail(const QString &message);
    bool validateIso();
    bool validateDevice();
    bool isSystemDisk(const QString &device) const;
    bool isUsbOrRemovable(const QString &sysName) const;
    bool unmountDevice();
    bool writeIso();
    bool createDataPartition();
    bool formatDataPartition();
    QString partitionNode(int partnoOneBased) const;
    QString sanitizedLabel() const;
};
