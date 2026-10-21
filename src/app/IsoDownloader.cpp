#include "IsoDownloader.h"
#include "Version.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

namespace {
constexpr auto kFallbackIso = "https://iso.omarchy.org/omarchy-4.0.6.iso";
constexpr int kIsoMagicOffset = 32769;

QString versionFromFileName(const QString &fileName)
{
    const QRegularExpression re(QStringLiteral(R"(omarchy-([0-9][0-9.]*)\.iso)"));
    const auto match = re.match(fileName);
    return match.hasMatch() ? match.captured(1) : fileName;
}

bool trustedIsoUrl(const QString &url)
{
    static const QRegularExpression re(
        QStringLiteral(R"(^https://iso\.omarchy\.org/omarchy-[0-9][0-9.]*\.iso$)"));
    return re.match(url).hasMatch();
}

bool newerVersion(const QString &a, const QString &b)
{
    const QStringList pa = a.split(QLatin1Char('.'));
    const QStringList pb = b.split(QLatin1Char('.'));
    const int n = qMax(pa.size(), pb.size());
    for (int i = 0; i < n; ++i) {
        const int va = i < pa.size() ? pa.at(i).toInt() : 0;
        const int vb = i < pb.size() ? pb.at(i).toInt() : 0;
        if (va != vb)
            return va > vb;
    }
    return false;
}

QString newestIsoUrl(const QString &html)
{
    const QRegularExpression re(QStringLiteral(R"(https://iso\.omarchy\.org/omarchy-[0-9][0-9.]*\.iso)"));
    QString best;
    QString bestVersion;
    auto it = re.globalMatch(html);
    while (it.hasNext()) {
        const QString url = it.next().captured();
        if (!trustedIsoUrl(url))
            continue;
        const QString version = versionFromFileName(QFileInfo(QUrl(url).path()).fileName());
        if (best.isEmpty() || newerVersion(version, bestVersion)) {
            best = url;
            bestVersion = version;
        }
    }
    return best;
}

QString parseSha256(const QByteArray &body)
{
    const QString hex = QString::fromUtf8(body)
                            .trimmed()
                            .section(QRegularExpression(QStringLiteral("\\s+")), 0, 0)
                            .trimmed()
                            .toLower();
    if (hex.size() != 64)
        return {};
    for (const QChar c : hex) {
        if (!c.isDigit() && (c < QLatin1Char('a') || c > QLatin1Char('f')))
            return {};
    }
    return hex;
}

QNetworkRequest requestFor(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("OmSticker/%1").arg(QString::fromUtf8(kVersion)));
    return request;
}
}

IsoDownloader::IsoDownloader(QObject *parent)
    : QObject(parent)
{
}

void IsoDownloader::fail(const QString &message)
{
    if (m_reply) {
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_file.reset();
    m_hash.reset();
    m_active = false;
    emit finished(false, message);
}

void IsoDownloader::start(const QString &directory)
{
    if (m_active)
        return;
    m_active = true;
    m_fallback = false;
    m_version.clear();
    m_fileName.clear();
    m_expectedHash.clear();
    m_rejectReason.clear();
    m_checkedMagic = false;
    m_magic.clear();

    m_reply = m_nam.get(requestFor(QUrl(QStringLiteral("https://omarchy.org/"))));
    connect(m_reply, &QNetworkReply::finished, this, [this, directory] {
        QNetworkReply *discover = m_reply;
        m_reply = nullptr;
        if (!discover)
            return;
        if (!m_active || discover->error() == QNetworkReply::OperationCanceledError) {
            discover->deleteLater();
            if (m_active)
                fail(QStringLiteral("Download cancelled"));
            return;
        }
        QString url = QString::fromUtf8(kFallbackIso);
        m_fallback = true;
        if (discover->error() == QNetworkReply::NoError) {
            const QString found = newestIsoUrl(QString::fromUtf8(discover->readAll()));
            if (!found.isEmpty()) {
                url = found;
                m_fallback = false;
            }
        }
        discover->deleteLater();
        if (!m_active)
            return;
        m_fileName = QFileInfo(QUrl(url).path()).fileName();
        if (m_fileName.isEmpty())
            m_fileName = QStringLiteral("omarchy.iso");
        m_version = versionFromFileName(m_fileName);
        emit resolved(m_version, m_fileName, m_fallback);
        fetchChecksum(QUrl(url), directory);
    });
}

void IsoDownloader::fetchChecksum(const QUrl &isoUrl, const QString &directory)
{
    QUrl checksumUrl(isoUrl.toString() + QStringLiteral(".sha256"));
    m_reply = m_nam.get(requestFor(checksumUrl));
    connect(m_reply, &QNetworkReply::finished, this, [this, isoUrl, directory] {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        if (!reply)
            return;
        if (!m_active || reply->error() == QNetworkReply::OperationCanceledError) {
            reply->deleteLater();
            if (m_active)
                fail(QStringLiteral("Download cancelled"));
            return;
        }
        QString hash;
        if (reply->error() == QNetworkReply::NoError)
            hash = parseSha256(reply->readAll());
        reply->deleteLater();
        if (!m_active)
            return;
        if (hash.isEmpty()) {
            fail(QStringLiteral("Could not verify the ISO download: missing SHA-256 checksum"));
            return;
        }
        m_expectedHash = hash;
        beginDownload(isoUrl, QDir(directory).filePath(m_fileName));
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
        fail(QStringLiteral("Could not save ISO: %1").arg(error));
        return;
    }
    m_hash = std::make_unique<QCryptographicHash>(QCryptographicHash::Sha256);
    m_checkedMagic = false;
    m_magic.clear();
    m_rejectReason.clear();

    m_reply = m_nam.get(requestFor(url));
    connect(m_reply, &QNetworkReply::downloadProgress, this, &IsoDownloader::progressChanged);
    connect(m_reply, &QNetworkReply::readyRead, this, [this] {
        if (!m_reply || !m_file || !m_hash)
            return;
        const QByteArray chunk = m_reply->readAll();
        if (chunk.isEmpty())
            return;
        if (!m_checkedMagic && m_magic.size() < kIsoMagicOffset + 5)
            m_magic += chunk;
        if (!m_checkedMagic && m_magic.size() >= kIsoMagicOffset + 5) {
            m_checkedMagic = true;
            if (m_magic.mid(kIsoMagicOffset, 5) != QByteArrayLiteral("CD001")) {
                m_rejectReason = QStringLiteral("Downloaded file is not a valid ISO 9660 image");
                m_reply->abort();
                return;
            }
            m_magic.clear();
        }
        m_hash->addData(chunk);
        m_file->write(chunk);
    });
    connect(m_reply, &QNetworkReply::finished, this, [this, destination] {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        if (!reply)
            return;
        if (!m_rejectReason.isEmpty()) {
            reply->deleteLater();
            const QString reason = m_rejectReason;
            m_file.reset();
            m_hash.reset();
            m_active = false;
            emit finished(false, reason);
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            const QString error = reply->error() == QNetworkReply::OperationCanceledError
                ? QStringLiteral("Download cancelled")
                : reply->errorString();
            reply->deleteLater();
            m_file.reset();
            m_hash.reset();
            m_active = false;
            emit finished(false, error);
            return;
        }
        const QByteArray tail = reply->readAll();
        if (!tail.isEmpty() && m_file && m_hash) {
            if (!m_checkedMagic)
                m_magic += tail;
            m_hash->addData(tail);
            m_file->write(tail);
        }
        reply->deleteLater();
        if (!m_checkedMagic && m_magic.size() >= kIsoMagicOffset + 5
            && m_magic.mid(kIsoMagicOffset, 5) == QByteArrayLiteral("CD001"))
            m_checkedMagic = true;
        if (!m_checkedMagic || !m_hash || !m_file) {
            m_file.reset();
            m_hash.reset();
            m_active = false;
            emit finished(false, QStringLiteral("Downloaded file is not a valid ISO 9660 image"));
            return;
        }
        const QString got = QString::fromLatin1(m_hash->result().toHex());
        if (got != m_expectedHash) {
            m_file.reset();
            m_hash.reset();
            m_active = false;
            emit finished(false, QStringLiteral("Downloaded ISO failed the SHA-256 check"));
            return;
        }
        const bool committed = m_file->commit();
        const QString error = m_file->errorString();
        m_file.reset();
        m_hash.reset();
        m_active = false;
        if (!committed) {
            QFile::remove(destination);
            emit finished(false, QStringLiteral("Could not save ISO: %1").arg(error));
        } else {
            emit finished(true, destination);
        }
    });
}
