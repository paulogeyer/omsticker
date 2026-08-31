#include "DeviceScanner.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QProcess>

QString UsbDrive::displayName() const
{
    QString name = QStringLiteral("%1 %2")
                       .arg(vendor.trimmed(), model.trimmed())
                       .simplified();
    if (name.isEmpty())
        name = QFileInfo(path).fileName();
    const QString size = QLocale().formattedDataSize(sizeBytes);
    return QStringLiteral("%1 — %2 (%3)").arg(name, size, path);
}

DeviceScanner::DeviceScanner(QObject *parent)
    : QObject(parent)
{
}

void DeviceScanner::refresh()
{
    QProcess process;
    process.start(QStringLiteral("lsblk"),
                  {QStringLiteral("-J"), QStringLiteral("-b"), QStringLiteral("-d"),
                   QStringLiteral("-o"),
                   QStringLiteral("NAME,PATH,SIZE,TYPE,TRAN,RM,MODEL,VENDOR,HOTPLUG,SERIAL")});
    if (!process.waitForFinished(4000)) {
        process.kill();
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(process.readAllStandardOutput());
    const QJsonArray devices = doc.object().value(QStringLiteral("blockdevices")).toArray();
    QVector<UsbDrive> found;
    for (const QJsonValue &value : devices) {
        const QJsonObject obj = value.toObject();
        if (obj.value(QStringLiteral("type")).toString() != QLatin1String("disk"))
            continue;
        const QString tran = obj.value(QStringLiteral("tran")).toString();
        const bool removable = obj.value(QStringLiteral("rm")).toBool()
            || obj.value(QStringLiteral("hotplug")).toBool();
        const bool usb = tran == QLatin1String("usb");
        if (!usb && !removable)
            continue;
        UsbDrive drive;
        drive.path = obj.value(QStringLiteral("path")).toString();
        drive.model = obj.value(QStringLiteral("model")).toString();
        drive.vendor = obj.value(QStringLiteral("vendor")).toString();
        drive.sizeBytes = obj.value(QStringLiteral("size")).toVariant().toLongLong();
        drive.usb = usb;
        drive.removable = removable;
        if (!drive.path.isEmpty())
            found.push_back(drive);
    }
    m_drives = found;
    lookupById();
    emit updated();
}

void DeviceScanner::lookupById()
{
    const QDir dir(QStringLiteral("/dev/disk/by-id"));
    const QFileInfoList entries = dir.entryInfoList(QStringList{QStringLiteral("usb-*")}, QDir::Files);
    for (UsbDrive &drive : m_drives) {
        const QString canonical = QFileInfo(drive.path).canonicalFilePath();
        for (const QFileInfo &entry : entries) {
            if (entry.fileName().contains(QLatin1String("-part")))
                continue;
            if (entry.canonicalFilePath() == canonical) {
                drive.byId = entry.absoluteFilePath();
                break;
            }
        }
    }
}
