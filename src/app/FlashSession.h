#pragma once

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <memory>

class FlashSession : public QObject
{
    Q_OBJECT
public:
    explicit FlashSession(QObject *parent = nullptr);
    ~FlashSession() override;
    static QString helperPath();
    void start(const QString &iso, const QString &device, const QString &filesystem,
               const QString &label, bool dataPartition, qint64 offset = 0);
    void cancel();
    bool running() const;
    bool cancelled() const { return m_cancelled; }
    bool imageWritten() const { return m_imageWritten; }
    bool clearCheckpoint() const { return m_clearCheckpoint; }

signals:
    void statusChanged(const QString &message);
    void progressChanged(int percent, qint64 current, qint64 total, bool synced);
    void finished(bool ok, const QString &message);

private:
    std::unique_ptr<QProcess> m_process;
    QByteArray m_buffer;
    QByteArray m_stderr;
    bool m_reported = false;
    bool m_cancelled = false;
    bool m_imageWritten = false;
    bool m_clearCheckpoint = false;
    void handleLine(const QByteArray &line);
    void report(bool ok, const QString &message);
    void drainStderr();
};
