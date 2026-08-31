#include "FlashSession.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

FlashSession::FlashSession(QObject *parent)
    : QObject(parent)
{
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
        m_buffer += m_process.readAllStandardOutput();
        while (true) {
            const int newline = m_buffer.indexOf('\n');
            if (newline < 0)
                break;
            const QByteArray line = m_buffer.left(newline);
            m_buffer.remove(0, newline + 1);
            handleLine(line);
        }
    });
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        if (!m_buffer.isEmpty()) {
            handleLine(m_buffer);
            m_buffer.clear();
        }
        if (m_reported)
            return;
        if (m_cancelled) {
            report(false, QStringLiteral("Write aborted. The USB stick is incomplete and not bootable until you flash it again."));
            return;
        }
        if (status != QProcess::NormalExit) {
            report(false, QStringLiteral("Helper process crashed"));
            return;
        }
        if (code == 0) {
            report(true, QStringLiteral("USB stick is ready."));
            return;
        }
        if (code == 126 || code == 127)
            report(false, QStringLiteral("Could not start the USB writer"));
        else
            report(false, QStringLiteral("Flash helper failed"));
    });
}

void FlashSession::report(bool ok, const QString &message)
{
    if (m_reported)
        return;
    m_reported = true;
    emit finished(ok, message);
}

QString FlashSession::helperPath()
{
    const QString env = qEnvironmentVariable("OMSTICKER_HELPER");
    if (!env.isEmpty() && QFile::exists(env))
        return env;
    const QString local = QCoreApplication::applicationDirPath() + QStringLiteral("/omsticker-helper");
    if (QFile::exists(local))
        return local;
    return QStringLiteral("/usr/lib/omsticker/omsticker-helper");
}

bool FlashSession::running() const
{
    return m_process.state() != QProcess::NotRunning;
}

void FlashSession::start(const QString &iso, const QString &device, const QString &filesystem,
                         const QString &label, bool dataPartition, qint64 offset)
{
    if (running())
        return;
    m_buffer.clear();
    m_reported = false;
    m_cancelled = false;
    const QString helper = helperPath();
    if (!QFileInfo::exists(helper)) {
        report(false, QStringLiteral("USB writer helper was not found"));
        return;
    }
    QStringList args{QStringLiteral("flash"), QStringLiteral("--iso"), iso, QStringLiteral("--device"),
                     device, QStringLiteral("--filesystem"), filesystem, QStringLiteral("--label"), label,
                     QStringLiteral("--offset"), QString::number(offset)};
    if (!dataPartition)
        args << QStringLiteral("--no-data-partition");
    m_process.start(helper, args);
}

void FlashSession::cancel()
{
    if (!running())
        return;
    m_cancelled = true;
    m_process.kill();
    m_process.waitForFinished(3000);
}

void FlashSession::handleLine(const QByteArray &line)
{
    const QJsonDocument doc = QJsonDocument::fromJson(line);
    if (!doc.isObject())
        return;
    const QJsonObject obj = doc.object();
    const QString type = obj.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("status")) {
        emit statusChanged(obj.value(QStringLiteral("message")).toString());
    } else if (type == QLatin1String("progress")) {
        const qint64 current = obj.value(QStringLiteral("current")).toVariant().toLongLong();
        const qint64 total = obj.value(QStringLiteral("total")).toVariant().toLongLong();
        const int percent = total > 0 ? static_cast<int>((current * 100) / total) : 0;
        emit progressChanged(percent, current, total, obj.value(QStringLiteral("synced")).toBool());
    } else if (type == QLatin1String("error")) {
        report(false, obj.value(QStringLiteral("message")).toString());
    } else if (type == QLatin1String("done")) {
        const QString warning = obj.value(QStringLiteral("warning")).toString();
        const QString part = obj.value(QStringLiteral("dataPartition")).toString();
        const QString fs = obj.value(QStringLiteral("filesystem")).toString();
        if (!warning.isEmpty())
            report(true, warning);
        else if (part.isEmpty())
            report(true, QStringLiteral("USB stick is ready. No leftover space was available for a data partition."));
        else
            report(true, QStringLiteral("USB stick is ready. Leftover space is %1 on %2.").arg(fs, part));
    }
}
