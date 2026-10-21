#include "UDisks.h"
#include "BlockDevice.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <QVariant>

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>

namespace {

QString udisksFsType(const QString &filesystem)
{
    if (filesystem == QLatin1String("fat32"))
        return QStringLiteral("vfat");
    return filesystem;
}

struct BusyNodes {
    QStringList mounts;
    QStringList swaps;
};

BusyNodes busyNodes(const QString &device)
{
    const QString canonical = QFileInfo(device).canonicalFilePath();
    const QStringList related = deviceAndHolders(device);
    BusyNodes busy;

    auto relatedTo = [&](const QString &node) {
        const QString source = QFileInfo(unescapeMountField(node)).canonicalFilePath();
        if (source.isEmpty())
            return QString();
        if (related.contains(source) || isChildBlockDevice(canonical, source))
            return source;
        return QString();
    };

    QFile mounts(QStringLiteral("/proc/mounts"));
    if (mounts.open(QIODevice::ReadOnly)) {
        while (!mounts.atEnd()) {
            const QString line = QString::fromUtf8(mounts.readLine()).trimmed();
            if (line.isEmpty())
                continue;
            const QStringList parts = line.split(QLatin1Char(' '));
            if (parts.isEmpty())
                continue;
            const QString source = relatedTo(parts[0]);
            if (!source.isEmpty() && !busy.mounts.contains(source))
                busy.mounts << source;
        }
    }

    QFile swaps(QStringLiteral("/proc/swaps"));
    if (swaps.open(QIODevice::ReadOnly)) {
        swaps.readLine();
        while (!swaps.atEnd()) {
            const QString line = QString::fromUtf8(swaps.readLine()).simplified();
            if (line.isEmpty())
                continue;
            const QString source = relatedTo(line.section(QLatin1Char(' '), 0, 0));
            if (!source.isEmpty() && !busy.swaps.contains(source))
                busy.swaps << source;
        }
    }
    return busy;
}

}

QString UDisks::blockPath(const QString &device) const
{
    return QStringLiteral("/org/freedesktop/UDisks2/block_devices/%1").arg(QFileInfo(device).fileName());
}

bool UDisks::call(const QString &path, const QString &interface, const QString &method,
                  const QList<QVariant> &args, QList<QVariant> *replyOut, int timeoutMs)
{
    error.clear();
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.UDisks2"), path,
                                                          interface, method);
    message.setArguments(args);
    const QDBusMessage reply = QDBusConnection::systemBus().call(message, QDBus::Block, timeoutMs);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        error = reply.errorMessage();
        if (error.isEmpty())
            error = reply.errorName();
        return false;
    }
    if (replyOut)
        *replyOut = reply.arguments();
    return true;
}

bool UDisks::unmountTree(const QString &device)
{
    const BusyNodes busy = busyNodes(device);
    QStringList failures;
    for (const QString &source : busy.swaps) {
        const QVariantMap options;
        if (!call(blockPath(source), QStringLiteral("org.freedesktop.UDisks2.Swapspace"),
                  QStringLiteral("Stop"), {QVariant::fromValue(options)}))
            failures << QStringLiteral("%1 (%2)").arg(source, error);
    }
    const QString canonical = QFileInfo(device).canonicalFilePath();
    QStringList mounts = busy.mounts;
    std::sort(mounts.begin(), mounts.end(), [&](const QString &a, const QString &b) {
        return isChildBlockDevice(canonical, a) < isChildBlockDevice(canonical, b);
    });
    for (const QString &source : mounts) {
        const QVariantMap options{{QStringLiteral("force"), true}};
        if (!call(blockPath(source), QStringLiteral("org.freedesktop.UDisks2.Filesystem"),
                  QStringLiteral("Unmount"), {QVariant::fromValue(options)}))
            failures << QStringLiteral("%1 (%2)").arg(source, error);
    }
    if (!failures.isEmpty()) {
        error = QStringLiteral("Could not unmount the drive: %1").arg(failures.join(QStringLiteral("; ")));
        return false;
    }
    const BusyNodes stillBusy = busyNodes(device);
    if (!stillBusy.mounts.isEmpty() || !stillBusy.swaps.isEmpty()) {
        error = QStringLiteral("Drive is still mounted or in use as swap: %1")
                    .arg((stillBusy.mounts + stillBusy.swaps).join(QStringLiteral(", ")));
        return false;
    }
    return true;
}

int UDisks::openWriteFd(const QString &device)
{
    QList<QVariant> reply;
    QVariantMap options;
    options.insert(QStringLiteral("flags"), static_cast<int>(O_EXCL | O_CLOEXEC));
    if (!call(blockPath(device), QStringLiteral("org.freedesktop.UDisks2.Block"), QStringLiteral("OpenDevice"),
              {QStringLiteral("rw"), QVariant::fromValue(options)}, &reply))
        return -1;
    if (reply.isEmpty()) {
        error = QStringLiteral("UDisks2 returned no file descriptor");
        return -1;
    }
    const auto fd = qvariant_cast<QDBusUnixFileDescriptor>(reply.first());
    if (!fd.isValid()) {
        error = QStringLiteral("Invalid file descriptor from UDisks2");
        return -1;
    }
    const int duped = ::fcntl(fd.fileDescriptor(), F_DUPFD_CLOEXEC, 0);
    if (duped < 0) {
        error = QStringLiteral("Failed to duplicate device descriptor");
        return -1;
    }
    return duped;
}

bool UDisks::rescan(const QString &device)
{
    const QVariantMap options;
    return call(blockPath(device), QStringLiteral("org.freedesktop.UDisks2.Block"), QStringLiteral("Rescan"),
                {QVariant::fromValue(options)});
}

bool UDisks::waitForBlock(const QString &device, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        QList<QVariant> reply;
        if (call(blockPath(device), QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"),
                 {QStringLiteral("org.freedesktop.UDisks2.Block"), QStringLiteral("Device")}, &reply, 2000))
            return true;
        QThread::msleep(250);
    }
    error = QStringLiteral("Timed out waiting for %1").arg(device);
    return false;
}

bool UDisks::formatPartition(const QString &partition, const QString &filesystem, const QString &label)
{
    QVariantMap options;
    if (!label.isEmpty())
        options.insert(QStringLiteral("label"), label);
    options.insert(QStringLiteral("update-partition-type"), true);
    return call(blockPath(partition), QStringLiteral("org.freedesktop.UDisks2.Block"), QStringLiteral("Format"),
                {udisksFsType(filesystem), QVariant::fromValue(options)}, nullptr, 600000);
}
