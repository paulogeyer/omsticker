#include "MainWindow.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QString isoDownloadDir()
{
    const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation))
                             .filePath(QStringLiteral("omsticker"));
    QDir().mkpath(path);
    return path;
}

QString formatSpeed(qint64 bytes, qint64 elapsedMs)
{
    if (elapsedMs < 200 || bytes <= 0)
        return QStringLiteral("…");
    const qint64 bps = (bytes * 1000) / elapsedMs;
    return QLocale().formattedDataSize(bps) + QStringLiteral("/s");
}

QString formatEta(qint64 bytes, qint64 total, qint64 elapsedMs)
{
    if (total <= 0 || bytes <= 0 || elapsedMs < 500)
        return QStringLiteral("…");
    if (bytes >= total)
        return QStringLiteral("0s");
    const qint64 bps = (bytes * 1000) / elapsedMs;
    if (bps <= 0)
        return QStringLiteral("…");
    qint64 secs = (total - bytes) / bps;
    const qint64 hours = secs / 3600;
    secs %= 3600;
    const qint64 mins = secs / 60;
    secs %= 60;
    if (hours > 0)
        return QStringLiteral("%1h %2m").arg(hours).arg(mins, 2, 10, QLatin1Char('0'));
    if (mins > 0)
        return QStringLiteral("%1m %2s").arg(mins).arg(secs, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1s").arg(secs);
}

QFrame *makeCard()
{
    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("card"));
    return card;
}

struct FsChoice {
    const char *id;
    const char *label;
    const char *mkfs;
    const char *note;
};

const FsChoice kFilesystems[] = {
    {"exfat", "exFAT", "mkfs.exfat", "Best for Windows, macOS, and Linux"},
    {"fat32", "FAT32", "mkfs.fat", "Maximum compatibility · 4 GB file limit"},
    {"ntfs", "NTFS", "mkfs.ntfs", "Windows-friendly"},
    {"ext4", "ext4", "mkfs.ext4", "Linux native"},
    {"btrfs", "Btrfs", "mkfs.btrfs", "Linux with snapshots"},
    {"f2fs", "F2FS", "mkfs.f2fs", "Optimized for flash storage"},
    {"xfs", "XFS", "mkfs.xfs", "Linux high-performance"},
};

bool hasMkfs(const char *binary)
{
    return !QStandardPaths::findExecutable(QString::fromUtf8(binary)).isEmpty();
}

}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("OmSticker"));
    setMinimumSize(480, 640);
    resize(520, 700);
    setAcceptDrops(true);

    auto *root = new QWidget;
    auto *layout = new QVBoxLayout(root);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto *header = new QHBoxLayout;
    auto *icon = new QLabel;
    icon->setPixmap(QIcon(QStringLiteral(":/icons/omsticker.svg")).pixmap(48, 48));
    auto *titles = new QVBoxLayout;
    titles->setSpacing(2);
    auto *title = new QLabel(QStringLiteral("OmSticker"));
    title->setObjectName(QStringLiteral("title"));
    auto *subtitle = new QLabel(QStringLiteral("Create a bootable Omarchy USB stick"));
    subtitle->setObjectName(QStringLiteral("subtitle"));
    titles->addWidget(title);
    titles->addWidget(subtitle);
    header->addWidget(icon);
    header->addSpacing(12);
    header->addLayout(titles);
    header->addStretch();
    layout->addLayout(header);

    auto *isoCard = makeCard();
    auto *isoLayout = new QVBoxLayout(isoCard);
    isoLayout->setContentsMargins(16, 16, 16, 16);
    isoLayout->setSpacing(10);
    auto *isoSection = new QLabel(QStringLiteral("IMAGE"));
    isoSection->setObjectName(QStringLiteral("section"));
    isoLayout->addWidget(isoSection);
    m_isoEdit = new QLineEdit;
    m_isoEdit->setPlaceholderText(QStringLiteral("Path to Omarchy ISO"));
    m_browseButton = new QPushButton(QStringLiteral("Browse"));
    auto *isoRow = new QHBoxLayout;
    isoRow->addWidget(m_isoEdit, 1);
    isoRow->addWidget(m_browseButton);
    isoLayout->addLayout(isoRow);
    m_downloadButton = new QPushButton(QStringLiteral("Download latest Omarchy ISO"));
    isoLayout->addWidget(m_downloadButton);
    m_isoHint = new QLabel(QStringLiteral("Drop an .iso here, browse, or download."));
    m_isoHint->setObjectName(QStringLiteral("hint"));
    isoLayout->addWidget(m_isoHint);
    layout->addWidget(isoCard);

    auto *driveCard = makeCard();
    auto *driveLayout = new QVBoxLayout(driveCard);
    driveLayout->setContentsMargins(16, 16, 16, 16);
    driveLayout->setSpacing(10);
    auto *driveSection = new QLabel(QStringLiteral("USB DRIVE"));
    driveSection->setObjectName(QStringLiteral("section"));
    driveLayout->addWidget(driveSection);
    auto *driveRow = new QHBoxLayout;
    m_driveCombo = new QComboBox;
    m_refreshButton = new QPushButton(QStringLiteral("Refresh"));
    driveRow->addWidget(m_driveCombo, 1);
    driveRow->addWidget(m_refreshButton);
    driveLayout->addLayout(driveRow);
    layout->addWidget(driveCard);

    auto *spaceCard = makeCard();
    auto *spaceLayout = new QVBoxLayout(spaceCard);
    spaceLayout->setContentsMargins(16, 16, 16, 16);
    spaceLayout->setSpacing(10);
    auto *spaceSection = new QLabel(QStringLiteral("LEFTOVER SPACE"));
    spaceSection->setObjectName(QStringLiteral("section"));
    spaceLayout->addWidget(spaceSection);
    m_dataCheck = new QCheckBox(QStringLiteral("Make leftover space usable"));
    m_dataCheck->setChecked(true);
    spaceLayout->addWidget(m_dataCheck);
    auto *fsRow = new QHBoxLayout;
    m_fsCombo = new QComboBox;
    for (const FsChoice &choice : kFilesystems) {
        if (!hasMkfs(choice.mkfs))
            continue;
        m_fsCombo->addItem(QString::fromUtf8(choice.label), QString::fromUtf8(choice.id));
        m_fsCombo->setItemData(m_fsCombo->count() - 1, QString::fromUtf8(choice.note), Qt::ToolTipRole);
    }
    m_labelEdit = new QLineEdit(QStringLiteral("OMARCHY"));
    m_labelEdit->setPlaceholderText(QStringLiteral("Volume label"));
    fsRow->addWidget(m_fsCombo, 1);
    fsRow->addWidget(m_labelEdit, 1);
    spaceLayout->addLayout(fsRow);
    m_spaceHint = new QLabel;
    m_spaceHint->setObjectName(QStringLiteral("hint"));
    m_spaceHint->setWordWrap(true);
    spaceLayout->addWidget(m_spaceHint);
    layout->addWidget(spaceCard);

    auto *warning = new QLabel(QStringLiteral("Writing will erase all data on the selected drive."));
    warning->setObjectName(QStringLiteral("warning"));
    warning->setWordWrap(true);
    layout->addWidget(warning);

    m_flashButton = new QPushButton(QStringLiteral("Flash USB Stick"));
    m_flashButton->setObjectName(QStringLiteral("flash"));
    layout->addWidget(m_flashButton);

    m_progress = new QProgressBar;
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setTextVisible(false);
    layout->addWidget(m_progress);
    m_status = new QLabel(QStringLiteral("Ready"));
    m_status->setObjectName(QStringLiteral("hint"));
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    layout->addStretch();
    setCentralWidget(root);

    connect(m_browseButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("Select ISO"),
            isoDownloadDir(),
            QStringLiteral("Disk images (*.iso *.img)"));
        if (!path.isEmpty())
            setIsoPath(path);
    });
    connect(m_isoEdit, &QLineEdit::textChanged, this, [this](const QString &) { updateSpaceHint(); });
    connect(m_downloadButton, &QPushButton::clicked, this, [this] {
        if (m_downloader.running()) {
            m_downloader.cancel();
            return;
        }
        m_downloadClock.invalidate();
        m_downloader.start(isoDownloadDir());
        setBusy(true);
        m_downloadButton->setEnabled(true);
        m_downloadButton->setText(QStringLiteral("Cancel download"));
        m_status->setText(QStringLiteral("Finding latest Omarchy ISO..."));
    });
    connect(&m_downloader, &IsoDownloader::resolved, this,
            [this](const QString &version, const QString &fileName) {
                m_status->setText(QStringLiteral("Downloading Omarchy %1 (%2)...").arg(version, fileName));
            });
    connect(&m_downloader, &IsoDownloader::progressChanged, this,
            [this](qint64 received, qint64 total) {
                if (!m_downloadClock.isValid())
                    m_downloadClock.start();
                if (total > 0) {
                    m_progress->setRange(0, 100);
                    m_progress->setValue(static_cast<int>((received * 100) / total));
                } else {
                    m_progress->setRange(0, 0);
                }
                const QString ver = m_downloader.version();
                m_status->setText(QStringLiteral("Downloading Omarchy %1… %2 / %3 · %4 · %5 left")
                                      .arg(ver.isEmpty() ? QStringLiteral("ISO") : ver,
                                           QLocale().formattedDataSize(received),
                                           total > 0 ? QLocale().formattedDataSize(total)
                                                     : QStringLiteral("?"),
                                           formatSpeed(received, m_downloadClock.elapsed()),
                                           formatEta(received, total, m_downloadClock.elapsed())));
            });
    connect(&m_downloader, &IsoDownloader::finished, this, [this](bool ok, const QString &value) {
        m_downloadButton->setText(QStringLiteral("Download latest Omarchy ISO"));
        setBusy(false);
        m_progress->setRange(0, 100);
        if (ok) {
            setIsoPath(value);
            m_status->setText(QStringLiteral("Saved Omarchy %1 to %2")
                                  .arg(m_downloader.version(), value));
            m_progress->setValue(100);
        } else {
            m_status->setText(value);
            m_progress->setValue(0);
        }
    });
    connect(m_refreshButton, &QPushButton::clicked, &m_scanner, &DeviceScanner::refresh);
    connect(&m_scanner, &DeviceScanner::updated, this, &MainWindow::rebuildDriveList);
    connect(m_driveCombo, &QComboBox::currentIndexChanged, this, [this](int) { updateSpaceHint(); });
    connect(m_fsCombo, &QComboBox::currentIndexChanged, this, [this](int) { updateSpaceHint(); });
    connect(m_dataCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_fsCombo->setEnabled(checked);
        m_labelEdit->setEnabled(checked);
        updateSpaceHint();
    });
    connect(m_flashButton, &QPushButton::clicked, this, &MainWindow::flash);
    connect(&m_flash, &FlashSession::statusChanged, m_status, &QLabel::setText);
    connect(&m_flash, &FlashSession::progressChanged, this,
            [this](int percent, qint64 current, qint64 total) {
                if (!m_writeClock.isValid())
                    m_writeClock.start();
                m_progress->setRange(0, 100);
                m_progress->setValue(percent);
                m_status->setText(QStringLiteral("Writing image… %1 / %2 · %3 · %4 left")
                                      .arg(QLocale().formattedDataSize(current),
                                           total > 0 ? QLocale().formattedDataSize(total)
                                                     : QStringLiteral("?"),
                                           formatSpeed(current, m_writeClock.elapsed()),
                                           formatEta(current, total, m_writeClock.elapsed())));
            });
    connect(&m_flash, &FlashSession::finished, this, [this](bool ok, const QString &message) {
        setBusy(false);
        m_status->setText(message);
        m_status->setObjectName(ok ? QStringLiteral("ok") : QStringLiteral("warning"));
        m_status->style()->unpolish(m_status);
        m_status->style()->polish(m_status);
        if (ok)
            m_progress->setValue(100);
        QMessageBox box(this);
        box.setWindowTitle(ok ? QStringLiteral("Done") : QStringLiteral("Flash failed"));
        box.setText(message);
        box.setIcon(ok ? QMessageBox::Information : QMessageBox::Critical);
        box.exec();
        m_scanner.refresh();
    });

    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, &m_scanner, &DeviceScanner::refresh);
    timer->start(2500);
    m_scanner.refresh();
    detectExistingIso();
    updateSpaceHint();
}

void MainWindow::rebuildDriveList()
{
    const QString previous = selectedDevice();
    m_driveCombo->clear();
    for (const UsbDrive &drive : m_scanner.drives()) {
        m_driveCombo->addItem(drive.displayName(), drive.byId.isEmpty() ? drive.path : drive.byId);
        m_driveCombo->setItemData(m_driveCombo->count() - 1, drive.sizeBytes, Qt::UserRole + 1);
        m_driveCombo->setItemData(m_driveCombo->count() - 1, drive.path, Qt::UserRole + 2);
    }
    if (m_driveCombo->count() == 0)
        m_driveCombo->addItem(QStringLiteral("No USB drive detected"));
    const int index = m_driveCombo->findData(previous);
    if (index >= 0)
        m_driveCombo->setCurrentIndex(index);
    updateSpaceHint();
}

void MainWindow::detectExistingIso()
{
    const QDir dir(isoDownloadDir());
    QFileInfoList matches = dir.entryInfoList({QStringLiteral("omarchy*.iso")}, QDir::Files, QDir::Time);
    if (matches.isEmpty())
        matches = dir.entryInfoList({QStringLiteral("*.iso")}, QDir::Files, QDir::Time);
    if (!matches.isEmpty())
        setIsoPath(matches.first().absoluteFilePath());
}

void MainWindow::setBusy(bool busy)
{
    m_isoEdit->setEnabled(!busy);
    m_browseButton->setEnabled(!busy);
    m_driveCombo->setEnabled(!busy);
    m_refreshButton->setEnabled(!busy);
    m_fsCombo->setEnabled(!busy && m_dataCheck->isChecked());
    m_labelEdit->setEnabled(!busy && m_dataCheck->isChecked());
    m_dataCheck->setEnabled(!busy);
    m_flashButton->setEnabled(!busy);
    m_downloadButton->setEnabled(!busy || m_downloader.running());
}

void MainWindow::updateSpaceHint()
{
    const QFileInfo iso(m_isoEdit->text());
    if (iso.exists()) {
        m_isoHint->setText(QStringLiteral("%1 · %2")
                               .arg(iso.fileName(), QLocale().formattedDataSize(iso.size())));
        m_isoHint->setObjectName(QStringLiteral("hint"));
    }

    const qint64 driveSize = selectedDriveSize();
    const qint64 isoSize = iso.exists() ? iso.size() : 0;
    m_fsCombo->setEnabled(m_dataCheck->isChecked() && !m_flash.running());
    m_labelEdit->setEnabled(m_dataCheck->isChecked() && !m_flash.running());

    if (driveSize <= 0) {
        m_spaceHint->setText(QStringLiteral("Plug in a USB stick to continue."));
        return;
    }
    if (isoSize <= 0) {
        m_spaceHint->setText(QStringLiteral("Select an ISO to estimate leftover space."));
        return;
    }
    const qint64 leftover = driveSize - isoSize;
    if (leftover < 16ll * 1024 * 1024) {
        m_spaceHint->setText(QStringLiteral("This drive is barely larger than the ISO. Leftover space may be too small."));
        return;
    }
    if (!m_dataCheck->isChecked()) {
        m_spaceHint->setText(QStringLiteral("%1 will be left unused.")
                                 .arg(QLocale().formattedDataSize(leftover)));
        return;
    }
    const QString note = m_fsCombo->currentData(Qt::ToolTipRole).toString();
    m_spaceHint->setText(QStringLiteral("%1 will be formatted as %2. %3")
                             .arg(QLocale().formattedDataSize(leftover), m_fsCombo->currentText(), note));
}

void MainWindow::flash()
{
    const QString iso = m_isoEdit->text().trimmed();
    if (!QFileInfo::exists(iso)) {
        QMessageBox::warning(this, QStringLiteral("Missing ISO"), QStringLiteral("Choose a valid ISO image first."));
        return;
    }
    const QString device = selectedDevice();
    if (device.isEmpty() || m_scanner.drives().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("No drive"), QStringLiteral("Select a USB drive."));
        return;
    }
    const qint64 leftover = selectedDriveSize() - QFileInfo(iso).size();
    QString extra;
    if (m_dataCheck->isChecked() && leftover > 16ll * 1024 * 1024) {
        extra = QStringLiteral("\n\nLeftover %1 will be formatted as %2 (%3).")
                    .arg(QLocale().formattedDataSize(leftover), m_fsCombo->currentText(),
                         m_labelEdit->text().trimmed().isEmpty() ? QStringLiteral("unlabeled")
                                                                 : m_labelEdit->text().trimmed());
    }
    const QString driveName = m_driveCombo->currentText();
    const auto answer = QMessageBox::warning(
        this, QStringLiteral("Erase USB drive?"),
        QStringLiteral("This will erase all data on:\n\n%1\n\nand write:\n%2%3\n\nThis cannot be undone.")
            .arg(driveName, iso, extra),
        QMessageBox::Cancel | QMessageBox::Ok, QMessageBox::Cancel);
    if (answer != QMessageBox::Ok)
        return;

    setBusy(true);
    m_progress->setValue(0);
    m_writeClock.invalidate();
    m_status->setText(QStringLiteral("Starting..."));
    m_flash.start(QFileInfo(iso).absoluteFilePath(), device, selectedFilesystem(),
                  m_labelEdit->text().trimmed().isEmpty() ? QStringLiteral("OMARCHY")
                                                          : m_labelEdit->text().trimmed(),
                  m_dataCheck->isChecked());
}

qint64 MainWindow::selectedDriveSize() const
{
    if (m_scanner.drives().isEmpty())
        return 0;
    return m_driveCombo->currentData(Qt::UserRole + 1).toLongLong();
}

QString MainWindow::selectedDevice() const
{
    if (m_scanner.drives().isEmpty())
        return {};
    return m_driveCombo->currentData().toString();
}

QString MainWindow::selectedFilesystem() const
{
    const QString id = m_fsCombo->currentData().toString();
    return id.isEmpty() ? QStringLiteral("exfat") : id;
}

void MainWindow::setIsoPath(const QString &path)
{
    m_isoEdit->setText(path);
    updateSpaceHint();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_flash.running()) {
        event->ignore();
        QMessageBox::information(this, QStringLiteral("Busy"),
                                 QStringLiteral("Wait for the USB write to finish before closing."));
        return;
    }
    if (m_downloader.running())
        m_downloader.cancel();
    QMainWindow::closeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const auto urls = event->mimeData()->urls();
    if (urls.isEmpty())
        return;
    const QString path = urls.first().toLocalFile();
    if (path.endsWith(QLatin1String(".iso"), Qt::CaseInsensitive)
        || path.endsWith(QLatin1String(".img"), Qt::CaseInsensitive))
        setIsoPath(path);
}
