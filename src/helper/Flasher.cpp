#include "Flasher.h"
#include "Protocol.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QThread>

#include <libfdisk.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

void emitBytes(const QByteArray &bytes)
{
    fwrite(bytes.constData(), 1, static_cast<size_t>(bytes.size()), stdout);
    fflush(stdout);
}

bool isLinuxFs(const QString &filesystem)
{
    return filesystem == QLatin1String("ext4") || filesystem == QLatin1String("btrfs")
        || filesystem == QLatin1String("f2fs") || filesystem == QLatin1String("xfs");
}

bool isChildDevice(const QString &disk, const QString &node)
{
    if (node == disk)
        return true;
    const QString diskName = QFileInfo(disk).fileName();
    const QString nodeName = QFileInfo(node).fileName();
    if (diskName.contains(QRegularExpression(QStringLiteral(R"(\d$)"))))
        return nodeName.startsWith(diskName + QLatin1Char('p'))
            && nodeName.mid(diskName.size() + 1).toInt() > 0;
    return nodeName.startsWith(diskName) && nodeName.mid(diskName.size()).toInt() > 0;
}

}

bool Flasher::fail(const QString &message)
{
    emitBytes(proto::error(message, m_imageWritten));
    return false;
}

int Flasher::run()
{
    if (!validateIso() || !validateDevice())
        return 1;
    emitBytes(proto::status(QStringLiteral("Unmounting drive...")));
    if (!unmountDevice())
        return 1;
    emitBytes(proto::status(QStringLiteral("Writing image...")));
    if (!writeIso())
        return 1;
    m_imageWritten = true;
    bool partitioned = true;
    if (makeDataPartition) {
        emitBytes(proto::status(QStringLiteral("Creating data partition...")));
        partitioned = createDataPartition();
    }
    if (m_devFd >= 0) {
        ::fsync(m_devFd);
        ::close(m_devFd);
        m_devFd = -1;
    }
    m_udisks.rescan(m_canonicalDevice);
    if (!partitioned)
        return 1;
    if (makeDataPartition && !m_dataPartition.isEmpty()) {
        emitBytes(proto::status(QStringLiteral("Formatting leftover space...")));
        if (!formatDataPartition())
            return 1;
    }
    emitBytes(proto::done(m_dataPartition, filesystem));
    return 0;
}

bool Flasher::validateIso()
{
    QFile iso(isoPath);
    if (!iso.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("Cannot open ISO: %1").arg(iso.errorString()));
    if (iso.size() < 64 * 1024)
        return fail(QStringLiteral("ISO is too small to be valid"));
    if (!iso.seek(32769))
        return fail(QStringLiteral("Cannot read ISO volume descriptor"));
    const QByteArray magic = iso.read(5);
    if (magic != QByteArrayLiteral("CD001"))
        return fail(QStringLiteral("File is not a valid ISO 9660 image"));
    return true;
}

bool Flasher::validateDevice()
{
    m_canonicalDevice = QFileInfo(devicePath).canonicalFilePath();
    if (m_canonicalDevice.isEmpty())
        return fail(QStringLiteral("Block device does not exist"));

    struct stat st {};
    if (::stat(m_canonicalDevice.toUtf8().constData(), &st) != 0 || !S_ISBLK(st.st_mode))
        return fail(QStringLiteral("Target is not a block device"));

    const QString sysName = QFileInfo(m_canonicalDevice).fileName();
    if (QFile::exists(QStringLiteral("/sys/class/block/%1/partition").arg(sysName)))
        return fail(QStringLiteral("Target must be the whole disk, not a partition"));

    if (isSystemDisk(m_canonicalDevice))
        return fail(QStringLiteral("Refusing to write to a disk that holds a mounted system filesystem"));

    if (!isUsbOrRemovable(sysName))
        return fail(QStringLiteral("Refusing to write to a non-removable disk"));

    const QStringList allowed = {QStringLiteral("exfat"), QStringLiteral("fat32"), QStringLiteral("ntfs"),
                                 QStringLiteral("ext4"),  QStringLiteral("btrfs"), QStringLiteral("f2fs"),
                                 QStringLiteral("xfs")};
    if (!allowed.contains(filesystem))
        return fail(QStringLiteral("Unsupported filesystem: %1").arg(filesystem));

    const qint64 isoSize = QFileInfo(isoPath).size();
    QFile sizeFile(QStringLiteral("/sys/class/block/%1/size").arg(sysName));
    if (!sizeFile.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("Cannot read device size"));
    const quint64 devSize = sizeFile.readAll().trimmed().toULongLong() * 512ull;
    if (devSize == 0)
        return fail(QStringLiteral("Cannot read device size"));
    if (static_cast<quint64>(isoSize) > devSize)
        return fail(QStringLiteral("ISO is larger than the selected drive"));
    return true;
}

bool Flasher::isSystemDisk(const QString &device) const
{
    QFile mounts(QStringLiteral("/proc/mounts"));
    if (!mounts.open(QIODevice::ReadOnly))
        return true;
    const QStringList protectedMounts = {QStringLiteral("/"), QStringLiteral("/boot"), QStringLiteral("/home"),
                                         QStringLiteral("/usr"), QStringLiteral("/var"), QStringLiteral("/nix")};
    while (!mounts.atEnd()) {
        const QString line = QString::fromUtf8(mounts.readLine());
        const QStringList parts = line.split(QLatin1Char(' '));
        if (parts.size() < 2)
            continue;
        if (!protectedMounts.contains(parts[1]))
            continue;
        const QString source = QFileInfo(parts[0]).canonicalFilePath();
        if (!source.isEmpty() && isChildDevice(device, source))
            return true;
    }
    return false;
}

bool Flasher::isUsbOrRemovable(const QString &sysName) const
{
    QFile removable(QStringLiteral("/sys/class/block/%1/removable").arg(sysName));
    if (removable.open(QIODevice::ReadOnly) && removable.readAll().trimmed() == QByteArrayLiteral("1"))
        return true;

    const QString deviceLink = QStringLiteral("/sys/class/block/%1/device").arg(sysName);
    const QString canonical = QFileInfo(deviceLink).canonicalFilePath().toLower();
    if (canonical.contains(QLatin1String("usb")))
        return true;

    QFile uevent(QStringLiteral("/sys/class/block/%1/device/uevent").arg(sysName));
    if (uevent.open(QIODevice::ReadOnly)) {
        const QByteArray data = uevent.readAll().toLower();
        if (data.contains("usb"))
            return true;
    }
    return false;
}

bool Flasher::unmountDevice()
{
    m_udisks.unmountTree(m_canonicalDevice);
    return true;
}

bool Flasher::writeIso()
{
    const int isoFd = ::open(QFile::encodeName(isoPath).constData(), O_RDONLY);
    if (isoFd < 0)
        return fail(QStringLiteral("Cannot open ISO for reading"));
    m_devFd = m_udisks.openWriteFd(m_canonicalDevice);
    if (m_devFd < 0) {
        ::close(isoFd);
        return fail(QStringLiteral("Cannot open device for writing: %1").arg(m_udisks.error));
    }
    const int devFd = m_devFd;

    struct stat st {};
    if (::fstat(isoFd, &st) != 0) {
        ::close(isoFd);
        ::close(m_devFd);
        m_devFd = -1;
        return fail(QStringLiteral("Cannot stat ISO"));
    }
    const qint64 total = st.st_size;
    const size_t bufSize = 4 * 1024 * 1024;
    QByteArray buffer(static_cast<int>(bufSize), Qt::Uninitialized);

    qint64 written = 0;
    int lastPercent = -1;
    while (written < total) {
        const size_t chunk = static_cast<size_t>(qMin<qint64>(bufSize, total - written));
        const ssize_t got = ::read(isoFd, buffer.data(), chunk);
        if (got < 0) {
            ::close(isoFd);
            return fail(QStringLiteral("ISO read failed: %1").arg(QString::fromLocal8Bit(strerror(errno))));
        }
        if (got == 0)
            break;
        ssize_t off = 0;
        while (off < got) {
            const ssize_t put = ::write(devFd, buffer.constData() + off, static_cast<size_t>(got - off));
            if (put < 0) {
                ::close(isoFd);
                return fail(QStringLiteral("Device write failed: %1").arg(QString::fromLocal8Bit(strerror(errno))));
            }
            off += put;
        }
        written += got;
        const int percent = total > 0 ? static_cast<int>((written * 100) / total) : 100;
        if (percent != lastPercent) {
            lastPercent = percent;
            emitBytes(proto::progress(written, total, QStringLiteral("Writing image...")));
        }
    }

    if (::fsync(devFd) != 0) {
        ::close(isoFd);
        return fail(QStringLiteral("fsync failed: %1").arg(QString::fromLocal8Bit(strerror(errno))));
    }
    ::close(isoFd);
    return true;
}

bool Flasher::createDataPartition()
{
    struct fdisk_context *cxt = fdisk_new_context();
    if (!cxt)
        return fail(QStringLiteral("Failed to allocate partition context"));
    fdisk_disable_dialogs(cxt, 1);
    fdisk_enable_bootbits_protection(cxt, 1);

    if (m_devFd < 0)
        m_devFd = m_udisks.openWriteFd(m_canonicalDevice);
    if (m_devFd < 0) {
        fdisk_unref_context(cxt);
        return fail(QStringLiteral("Cannot reopen device to partition leftover space: %1").arg(m_udisks.error));
    }
    const int fdiskFd = ::dup(m_devFd);
    if (fdiskFd < 0 || fdisk_assign_device_by_fd(cxt, fdiskFd, m_canonicalDevice.toUtf8().constData(), 0) != 0) {
        if (fdiskFd >= 0)
            ::close(fdiskFd);
        fdisk_unref_context(cxt);
        return fail(QStringLiteral("Failed to open partition table on device"));
    }

    fdisk_set_last_lba(cxt, fdisk_get_nsectors(cxt) - 1);
    struct fdisk_label *label = fdisk_get_label(cxt, nullptr);
    if (label)
        fdisk_label_set_changed(label, 1);
    if (fdisk_write_disklabel(cxt) != 0) {
        fdisk_deassign_device(cxt, 0);
        fdisk_unref_context(cxt);
        return fail(QStringLiteral("Failed to relocate partition table to the end of the drive"));
    }
    struct fdisk_table *frees = nullptr;
    if (fdisk_get_freespaces(cxt, &frees) != 0 || !frees) {
        fdisk_deassign_device(cxt, 0);
        fdisk_unref_context(cxt);
        m_dataPartition.clear();
        return true;
    }

    struct fdisk_iter *itr = fdisk_new_iter(FDISK_ITER_FORWARD);
    struct fdisk_partition *fp = nullptr;
    fdisk_sector_t bestStart = 0;
    fdisk_sector_t bestSize = 0;
    while (fdisk_table_next_partition(frees, itr, &fp) == 0) {
        if (!fdisk_partition_is_freespace(fp) || !fdisk_partition_has_size(fp))
            continue;
        const fdisk_sector_t size = fdisk_partition_get_size(fp);
        if (size > bestSize) {
            bestSize = size;
            bestStart = fdisk_partition_has_start(fp) ? fdisk_partition_get_start(fp) : 0;
        }
    }
    fdisk_free_iter(itr);
    fdisk_unref_table(frees);

    const unsigned long sectorSize = fdisk_get_sector_size(cxt);
    const quint64 freeBytes = static_cast<quint64>(bestSize) * sectorSize;
    if (freeBytes < 16ull * 1024ull * 1024ull) {
        fdisk_deassign_device(cxt, 0);
        fdisk_unref_context(cxt);
        m_dataPartition.clear();
        return true;
    }

    struct fdisk_partition *pa = fdisk_new_partition();
    fdisk_partition_set_start(pa, bestStart);
    fdisk_partition_set_size(pa, bestSize);
    fdisk_partition_partno_follow_default(pa, 1);
    fdisk_partition_end_follow_default(pa, 1);
    fdisk_partition_set_name(pa, sanitizedLabel().toUtf8().constData());

    label = fdisk_get_label(cxt, nullptr);
    const char *typeName = isLinuxFs(filesystem) ? "linux" : "msftdata";
    struct fdisk_parttype *type = fdisk_label_parse_parttype(label, typeName);
    if (type) {
        fdisk_partition_set_type(pa, type);
        fdisk_unref_parttype(type);
    }

    size_t partno = 0;
    if (fdisk_add_partition(cxt, pa, &partno) != 0) {
        fdisk_unref_partition(pa);
        fdisk_deassign_device(cxt, 0);
        fdisk_unref_context(cxt);
        return fail(QStringLiteral("Failed to add a data partition in leftover space"));
    }
    fdisk_unref_partition(pa);

    if (fdisk_write_disklabel(cxt) != 0) {
        fdisk_deassign_device(cxt, 0);
        fdisk_unref_context(cxt);
        return fail(QStringLiteral("Failed to write updated partition table"));
    }
    fdisk_reread_partition_table(cxt);
    fdisk_deassign_device(cxt, 0);
    fdisk_unref_context(cxt);

    m_dataPartition = partitionNode(static_cast<int>(partno) + 1);
    m_udisks.rescan(m_canonicalDevice);
    for (int i = 0; i < 30 && !QFile::exists(m_dataPartition); ++i)
        QThread::msleep(100);
    if (!QFile::exists(m_dataPartition))
        return fail(QStringLiteral("Data partition did not appear after partitioning"));
    return true;
}

bool Flasher::formatDataPartition()
{
    if (!m_udisks.formatPartition(m_dataPartition, filesystem, sanitizedLabel()))
        return fail(QStringLiteral("Image written, but formatting leftover space failed: %1").arg(m_udisks.error));
    return true;
}

QString Flasher::partitionNode(int partnoOneBased) const
{
    const QString name = QFileInfo(m_canonicalDevice).fileName();
    if (name.contains(QRegularExpression(QStringLiteral(R"(\d$)"))))
        return m_canonicalDevice + QLatin1Char('p') + QString::number(partnoOneBased);
    return m_canonicalDevice + QString::number(partnoOneBased);
}

QString Flasher::sanitizedLabel() const
{
    QString value = label.trimmed();
    if (value.isEmpty())
        return {};
    if (filesystem == QLatin1String("fat32"))
        return value.left(11).toUpper();
    if (filesystem == QLatin1String("exfat"))
        return value.left(15);
    if (filesystem == QLatin1String("ext4") || filesystem == QLatin1String("xfs"))
        return value.left(16);
    return value.left(32);
}
