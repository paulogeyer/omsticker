#pragma once

#include "DeviceScanner.h"
#include "FlashSession.h"
#include "IsoDownloader.h"
#include "WriteCheckpoint.h"

#include <QElapsedTimer>
#include <QMainWindow>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    DeviceScanner m_scanner;
    FlashSession m_flash;
    IsoDownloader m_downloader;

    QLineEdit *m_isoEdit = nullptr;
    QLabel *m_isoHint = nullptr;
    QComboBox *m_driveCombo = nullptr;
    QComboBox *m_fsCombo = nullptr;
    QLineEdit *m_labelEdit = nullptr;
    QCheckBox *m_dataCheck = nullptr;
    QLabel *m_spaceHint = nullptr;
    QLabel *m_status = nullptr;
    QProgressBar *m_progress = nullptr;
    QPushButton *m_flashButton = nullptr;
    QPushButton *m_browseButton = nullptr;
    QPushButton *m_downloadButton = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QElapsedTimer m_downloadClock;
    QElapsedTimer m_writeClock;
    WriteCheckpoint m_writeCheckpoint;
    qint64 m_writeOffsetStart = 0;

    void rebuildDriveList();
    void detectExistingIso();
    void setBusy(bool busy);
    void updateSpaceHint();
    void flash();
    bool confirmAbort();
    void abortWrite();
    qint64 selectedDriveSize() const;
    QString selectedDevice() const;
    QString selectedDriveSerial() const;
    QString selectedFilesystem() const;
    void setIsoPath(const QString &path);
};
