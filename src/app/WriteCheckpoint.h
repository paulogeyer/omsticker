#pragma once

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QString>
#include <QVector>

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
        if (offset <= 0)
            return false;
        if (isoPath != iso || isoSize != size || isoMtime != mtime || deviceSize != driveSize)
            return false;
        if (devicePath != device)
            return false;
        if (!deviceSerial.isEmpty() && !serial.isEmpty() && deviceSerial != serial)
            return false;
        return true;
    }
};

inline bool sameCheckpointDevice(const WriteCheckpoint &a, const WriteCheckpoint &b)
{
    if (a.devicePath != b.devicePath || a.deviceSize != b.deviceSize)
        return false;
    if (!a.deviceSerial.isEmpty() && !b.deviceSerial.isEmpty() && a.deviceSerial != b.deviceSerial)
        return false;
    return true;
}

inline QString writeCheckpointPath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/write-checkpoint.json");
}

inline WriteCheckpoint checkpointFromJson(const QJsonObject &obj)
{
    WriteCheckpoint checkpoint;
    checkpoint.isoPath = obj.value(QStringLiteral("isoPath")).toString();
    checkpoint.isoSize = obj.value(QStringLiteral("isoSize")).toVariant().toLongLong();
    checkpoint.isoMtime = obj.value(QStringLiteral("isoMtime")).toVariant().toLongLong();
    checkpoint.devicePath = obj.value(QStringLiteral("devicePath")).toString();
    checkpoint.deviceSerial = obj.value(QStringLiteral("deviceSerial")).toString();
    checkpoint.deviceSize = obj.value(QStringLiteral("deviceSize")).toVariant().toLongLong();
    checkpoint.offset = obj.value(QStringLiteral("offset")).toVariant().toLongLong();
    return checkpoint;
}

inline QJsonObject checkpointToJson(const WriteCheckpoint &checkpoint)
{
    return {{QStringLiteral("isoPath"), checkpoint.isoPath},
            {QStringLiteral("isoSize"), checkpoint.isoSize},
            {QStringLiteral("isoMtime"), checkpoint.isoMtime},
            {QStringLiteral("devicePath"), checkpoint.devicePath},
            {QStringLiteral("deviceSerial"), checkpoint.deviceSerial},
            {QStringLiteral("deviceSize"), checkpoint.deviceSize},
            {QStringLiteral("offset"), checkpoint.offset}};
}

inline QVector<WriteCheckpoint> loadWriteCheckpoints()
{
    QFile file(writeCheckpointPath());
    QVector<WriteCheckpoint> checkpoints;
    if (!file.open(QIODevice::ReadOnly))
        return checkpoints;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonArray array;
    if (doc.isArray())
        array = doc.array();
    else if (doc.isObject())
        array.append(doc.object());
    for (const QJsonValue &value : array) {
        const WriteCheckpoint checkpoint = checkpointFromJson(value.toObject());
        if (!checkpoint.isoPath.isEmpty() && !checkpoint.devicePath.isEmpty())
            checkpoints.push_back(checkpoint);
    }
    return checkpoints;
}

inline void writeCheckpointFile(const QVector<WriteCheckpoint> &checkpoints)
{
    QJsonArray array;
    const int limit = qMin(checkpoints.size(), 8);
    for (int i = 0; i < limit; ++i)
        array.append(checkpointToJson(checkpoints.at(i)));
    QSaveFile file(writeCheckpointPath());
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
    file.commit();
}

inline void saveWriteCheckpoint(const WriteCheckpoint &checkpoint)
{
    QVector<WriteCheckpoint> kept;
    kept.push_back(checkpoint);
    for (const WriteCheckpoint &existing : loadWriteCheckpoints()) {
        if (!sameCheckpointDevice(existing, checkpoint))
            kept.push_back(existing);
    }
    writeCheckpointFile(kept);
}

inline void clearWriteCheckpoint(const WriteCheckpoint &checkpoint)
{
    QVector<WriteCheckpoint> kept;
    for (const WriteCheckpoint &existing : loadWriteCheckpoints()) {
        if (!sameCheckpointDevice(existing, checkpoint))
            kept.push_back(existing);
    }
    if (kept.isEmpty())
        QFile::remove(writeCheckpointPath());
    else
        writeCheckpointFile(kept);
}
