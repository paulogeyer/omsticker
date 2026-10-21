#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>

inline bool isChildBlockDevice(const QString &disk, const QString &node)
{
    if (node.isEmpty())
        return false;
    if (node == disk)
        return true;
    const QString diskName = QFileInfo(disk).fileName();
    const QString nodeName = QFileInfo(node).fileName();
    if (diskName.isEmpty() || nodeName.isEmpty())
        return false;
    if (diskName.contains(QRegularExpression(QStringLiteral(R"(\d$)"))))
        return nodeName.startsWith(diskName + QLatin1Char('p'))
            && nodeName.mid(diskName.size() + 1).toInt() > 0;
    return nodeName.startsWith(diskName) && nodeName.mid(diskName.size()).toInt() > 0;
}

inline void collectBlockHolders(const QString &sysName, QSet<QString> &seen, QStringList &nodes)
{
    if (sysName.isEmpty() || seen.contains(sysName))
        return;
    seen.insert(sysName);
    const QString dev = QFileInfo(QStringLiteral("/dev/%1").arg(sysName)).canonicalFilePath();
    if (!dev.isEmpty())
        nodes << dev;
    const QDir holders(QStringLiteral("/sys/class/block/%1/holders").arg(sysName));
    const QStringList entries = holders.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &holder : entries)
        collectBlockHolders(holder, seen, nodes);
}

inline QStringList deviceAndHolders(const QString &device)
{
    const QString canonical = QFileInfo(device).canonicalFilePath();
    QStringList nodes;
    if (canonical.isEmpty())
        return nodes;
    const QString name = QFileInfo(canonical).fileName();
    QSet<QString> seen;
    collectBlockHolders(name, seen, nodes);
    const QDir sys(QStringLiteral("/sys/class/block/%1").arg(name));
    const QStringList children = sys.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &child : children) {
        if (isChildBlockDevice(canonical, QStringLiteral("/dev/%1").arg(child)))
            collectBlockHolders(child, seen, nodes);
    }
    return nodes;
}

inline QString unescapeMountField(QString field)
{
    field.replace(QStringLiteral("\\040"), QStringLiteral(" "));
    field.replace(QStringLiteral("\\011"), QStringLiteral("\t"));
    field.replace(QStringLiteral("\\012"), QStringLiteral("\n"));
    field.replace(QStringLiteral("\\134"), QStringLiteral("\\"));
    return field;
}

inline bool holdsSystemMount(const QString &device)
{
    const QString canonical = QFileInfo(device).canonicalFilePath();
    const QStringList related = deviceAndHolders(device);
    if (canonical.isEmpty() || related.isEmpty())
        return true;

    QFile mounts(QStringLiteral("/proc/mounts"));
    if (!mounts.open(QIODevice::ReadOnly))
        return true;

    const QStringList protectedMounts = {QStringLiteral("/"),
                                         QStringLiteral("/boot"),
                                         QStringLiteral("/boot/efi"),
                                         QStringLiteral("/efi"),
                                         QStringLiteral("/home"),
                                         QStringLiteral("/usr"),
                                         QStringLiteral("/var"),
                                         QStringLiteral("/nix")};
    while (!mounts.atEnd()) {
        const QString line = QString::fromUtf8(mounts.readLine()).trimmed();
        if (line.isEmpty())
            continue;
        const QStringList parts = line.split(QLatin1Char(' '));
        if (parts.size() < 2)
            continue;
        if (!protectedMounts.contains(unescapeMountField(parts[1])))
            continue;
        const QString source = QFileInfo(unescapeMountField(parts[0])).canonicalFilePath();
        if (source.isEmpty())
            continue;
        if (related.contains(source) || isChildBlockDevice(canonical, source))
            return true;
    }
    return false;
}
