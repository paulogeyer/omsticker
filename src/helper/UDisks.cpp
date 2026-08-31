#include "UDisks.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>
#include <QFileInfo>
#include <QProcess>
#include <QVariant>

#include <unistd.h>

namespace {

QString udisksFsType(const QString &filesystem)
{
    if (filesystem == QLatin1String("fat32"))
        return QStringLiteral("vfat");
    return filesystem;
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
    QProcess lsblk;
    lsblk.start(QStringLiteral("lsblk"),
                {QStringLiteral("-ln"), QStringLiteral("-o"), QStringLiteral("PATH,MOUNTPOINT"), device});
    lsblk.waitForFinished(5000);
    const QString output = QString::fromUtf8(lsblk.readAllStandardOutput());
    for (const QString &line : output.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const QStringList cols = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (cols.size() < 2)
            continue;
        const QString mountpoint = cols.mid(1).join(QLatin1Char(' '));
        if (mountpoint.isEmpty() || mountpoint == QLatin1String("[SWAP]"))
            continue;
        QVariantMap options{{QStringLiteral("force"), true}};
        call(blockPath(cols.first()), QStringLiteral("org.freedesktop.UDisks2.Filesystem"),
             QStringLiteral("Unmount"), {QVariant::fromValue(options)});
    }
    return true;
}

int UDisks::openWriteFd(const QString &device)
{
    QList<QVariant> reply;
    const QVariantMap options;
    if (!call(blockPath(device), QStringLiteral("org.freedesktop.UDisks2.Block"), QStringLiteral("OpenDevice"),
              {QStringLiteral("w"), QVariant::fromValue(options)}, &reply))
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
    const int duped = ::dup(fd.fileDescriptor());
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

bool UDisks::formatPartition(const QString &partition, const QString &filesystem, const QString &label)
{
    QVariantMap options;
    if (!label.isEmpty())
        options.insert(QStringLiteral("label"), label);
    options.insert(QStringLiteral("update-partition-type"), true);
    if (filesystem == QLatin1String("fat32"))
        options.insert(QStringLiteral("label"), label.left(11).toUpper());
    return call(blockPath(partition), QStringLiteral("org.freedesktop.UDisks2.Block"), QStringLiteral("Format"),
                {udisksFsType(filesystem), QVariant::fromValue(options)});
}
