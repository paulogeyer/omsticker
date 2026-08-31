#pragma once

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace proto {

inline QByteArray line(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
}

inline QByteArray status(const QString &message)
{
    return line({{QStringLiteral("type"), QStringLiteral("status")},
                 {QStringLiteral("message"), message}});
}

inline QByteArray progress(qint64 current, qint64 total, const QString &message)
{
    return line({{QStringLiteral("type"), QStringLiteral("progress")},
                 {QStringLiteral("current"), current},
                 {QStringLiteral("total"), total},
                 {QStringLiteral("message"), message}});
}

inline QByteArray error(const QString &message, bool imageWritten = false)
{
    return line({{QStringLiteral("type"), QStringLiteral("error")},
                 {QStringLiteral("message"), message},
                 {QStringLiteral("imageWritten"), imageWritten}});
}

inline QByteArray done(const QString &dataPartition, const QString &filesystem,
                       const QString &warning = {})
{
    QJsonObject object{{QStringLiteral("type"), QStringLiteral("done")},
                       {QStringLiteral("dataPartition"), dataPartition},
                       {QStringLiteral("filesystem"), filesystem}};
    if (!warning.isEmpty())
        object.insert(QStringLiteral("warning"), warning);
    return line(object);
}

}
