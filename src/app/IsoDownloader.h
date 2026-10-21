#pragma once

#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSaveFile>
#include <QUrl>
#include <memory>

class QNetworkReply;

class IsoDownloader : public QObject
{
    Q_OBJECT
public:
    explicit IsoDownloader(QObject *parent = nullptr);
    void start(const QString &directory);
    void cancel();
    bool running() const { return m_active; }
    QString version() const { return m_version; }
    QString fileName() const { return m_fileName; }

signals:
    void resolved(const QString &version, const QString &fileName, bool fallback);
    void progressChanged(qint64 received, qint64 total);
    void finished(bool ok, const QString &pathOrError);

private:
    QNetworkAccessManager m_nam;
    QNetworkReply *m_reply = nullptr;
    std::unique_ptr<QSaveFile> m_file;
    std::unique_ptr<QCryptographicHash> m_hash;
    QByteArray m_magic;
    QString m_expectedHash;
    QString m_rejectReason;
    bool m_active = false;
    bool m_fallback = false;
    bool m_checkedMagic = false;
    QString m_version;
    QString m_fileName;
    void fetchChecksum(const QUrl &isoUrl, const QString &directory);
    void beginDownload(const QUrl &url, const QString &destination);
    void fail(const QString &message);
};
