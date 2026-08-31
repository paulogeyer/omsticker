#include "IsoDownloader.h"

#include <QDir>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

namespace {
constexpr auto kFallbackIso = "https://iso.omarchy.org/omarchy-4.0.2.iso";

QString versionFromFileName(const QString &fileName)
{
    const QRegularExpression re(QStringLiteral(R"(omarchy-([0-9][0-9.]*)\.iso)"));
    const auto match = re.match(fileName);
    return match.hasMatch() ? match.captured(1) : fileName;
}
}

IsoDownloader::IsoDownloader(QObject *parent)
    : QObject(parent)
{
}

void IsoDownloader::start(const QString &directory)
{
    if (m_active)
        return;
    m_active = true;
    m_version.clear();
    m_fileName.clear();

    QNetworkRequest page(QUrl(QStringLiteral("https://omarchy.org/")));
    page.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *discover = m_nam.get(page);
    connect(discover, &QNetworkReply::finished, this, [this, discover, directory] {
        QString url = QString::fromUtf8(kFallbackIso);
        if (discover->error() == QNetworkReply::NoError) {
            const QString html = QString::fromUtf8(discover->readAll());
            const QRegularExpression re(QStringLiteral(R"(https://iso\.omarchy\.org/omarchy-[0-9][0-9.]*\.iso)"));
            const auto match = re.match(html);
            if (match.hasMatch())
                url = match.captured();
        }
        discover->deleteLater();
        if (!m_active || m_reply)
            return;
        m_fileName = QFileInfo(QUrl(url).path()).fileName();
        if (m_fileName.isEmpty())
            m_fileName = QStringLiteral("omarchy.iso");
        m_version = versionFromFileName(m_fileName);
        emit resolved(m_version, m_fileName);
        beginDownload(QUrl(url), QDir(directory).filePath(m_fileName));
    });
}

void IsoDownloader::cancel()
{
    if (!m_active)
        return;
    if (m_reply) {
        m_reply->abort();
        return;
    }
    m_active = false;
    emit finished(false, QStringLiteral("Download cancelled"));
}

void IsoDownloader::beginDownload(const QUrl &url, const QString &destination)
{
    m_file = std::make_unique<QSaveFile>(destination);
    if (!m_file->open(QIODevice::WriteOnly)) {
        const QString error = m_file->errorString();
        m_file.reset();
        m_active = false;
        emit finished(false, QStringLiteral("Could not save ISO: %1").arg(error));
        return;
    }

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_nam.get(request);
    connect(m_reply, &QNetworkReply::downloadProgress, this, &IsoDownloader::progressChanged);
    connect(m_reply, &QNetworkReply::readyRead, this, [this] {
        if (m_reply && m_file)
            m_file->write(m_reply->readAll());
    });
    connect(m_reply, &QNetworkReply::finished, this, [this, destination] {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        if (!reply)
            return;
        if (reply->error() != QNetworkReply::NoError) {
            const QString error = reply->error() == QNetworkReply::OperationCanceledError
                ? QStringLiteral("Download cancelled")
                : reply->errorString();
            reply->deleteLater();
            m_file.reset();
            m_active = false;
            emit finished(false, error);
            return;
        }
        if (m_file)
            m_file->write(reply->readAll());
        const bool committed = m_file && m_file->commit();
        const QString error = m_file ? m_file->errorString() : QStringLiteral("write failed");
        m_file.reset();
        reply->deleteLater();
        m_active = false;
        if (!committed)
            emit finished(false, QStringLiteral("Could not save ISO: %1").arg(error));
        else
            emit finished(true, destination);
    });
}
