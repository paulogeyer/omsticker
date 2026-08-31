#pragma once

#include <QObject>
#include <QVector>

struct UsbDrive {
    QString path;
    QString byId;
    QString model;
    QString vendor;
    qint64 sizeBytes = 0;
    bool usb = false;
    bool removable = false;

    QString displayName() const;
};

class DeviceScanner : public QObject
{
    Q_OBJECT
public:
    explicit DeviceScanner(QObject *parent = nullptr);
    QVector<UsbDrive> drives() const { return m_drives; }
    void refresh();

signals:
    void updated();

private:
    QVector<UsbDrive> m_drives;
    void lookupById();
};
