#pragma once

#include <QObject>
#include <QProcess>

class FlashSession : public QObject
{
    Q_OBJECT
public:
    explicit FlashSession(QObject *parent = nullptr);
    static QString helperPath();
    void start(const QString &iso, const QString &device, const QString &filesystem,
               const QString &label, bool dataPartition, qint64 offset = 0);
    void cancel();
    bool running() const;
    bool cancelled() const { return m_cancelled; }

signals:
    void statusChanged(const QString &message);
    void progressChanged(int percent, qint64 current, qint64 total, bool synced);
    void finished(bool ok, const QString &message);

private:
    QProcess m_process;
    QByteArray m_buffer;
    bool m_reported = false;
    bool m_cancelled = false;
    void handleLine(const QByteArray &line);
    void report(bool ok, const QString &message);
};
