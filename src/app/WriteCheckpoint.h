#pragma once

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QString>

struct WriteCheckpoint {
    QString isoPath;
    qint64 isoSize = 0;
    qint64 isoMtime = 0;
    QString devicePath;
    QString deviceSerial;
    qint64 deviceSize = 0;
    qint64 offset = 0;

    bool matches(const QString &iso, qint64 size, qint64 mtime, const QString &device,
                 const QString &serial, qint64 driveSize) const
    {
        if (isoPath != iso || isoSize != size || isoMtime != mtime || deviceSize != driveSize)
            return false;
        if (!deviceSerial.isEmpty() && !serial.isEmpty())
            return deviceSerial == serial;
        return devicePath == device;
    }
};

inline QString writeCheckpointPath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/write-checkpoint.json");
}

inline WriteCheckpoint loadWriteCheckpoint()
{
    QFile file(writeCheckpointPath());
    WriteCheckpoint checkpoint;
    if (!file.open(QIODevice::ReadOnly))
        return checkpoint;
    const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    checkpoint.isoPath = obj.value(QStringLiteral("isoPath")).toString();
    checkpoint.isoSize = obj.value(QStringLiteral("isoSize")).toVariant().toLongLong();
    checkpoint.isoMtime = obj.value(QStringLiteral("isoMtime")).toVariant().toLongLong();
    checkpoint.devicePath = obj.value(QStringLiteral("devicePath")).toString();
    checkpoint.deviceSerial = obj.value(QStringLiteral("deviceSerial")).toString();
    checkpoint.deviceSize = obj.value(QStringLiteral("deviceSize")).toVariant().toLongLong();
    checkpoint.offset = obj.value(QStringLiteral("offset")).toVariant().toLongLong();
    return checkpoint;
}

inline void saveWriteCheckpoint(const WriteCheckpoint &checkpoint)
{
    QJsonObject obj{{QStringLiteral("isoPath"), checkpoint.isoPath},
                    {QStringLiteral("isoSize"), checkpoint.isoSize},
                    {QStringLiteral("isoMtime"), checkpoint.isoMtime},
                    {QStringLiteral("devicePath"), checkpoint.devicePath},
                    {QStringLiteral("deviceSerial"), checkpoint.deviceSerial},
                    {QStringLiteral("deviceSize"), checkpoint.deviceSize},
                    {QStringLiteral("offset"), checkpoint.offset}};
    QFile file(writeCheckpointPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

inline void clearWriteCheckpoint()
{
    QFile::remove(writeCheckpointPath());
}
