#ifndef SERIALPORTREADER_H
#define SERIALPORTREADER_H

#include "core/PacketParser.h"
#include "core/Types.h"

#include <QDateTime>
#include <QObject>
#include <QSerialPort>
#include <QStringList>
#include <QTimer>

namespace Slf {

class SerialPortReader : public QObject
{
    Q_OBJECT
public:
    explicit SerialPortReader(QObject *parent = nullptr);
    ~SerialPortReader() override;

    bool open(const QString &portName);
    void close();
    bool isOpen() const;
    QString portName() const { return portName_; }
    QString lastError() const { return lastError_; }

    bool requestStatus();
    bool requestFullDump();
    bool requestIncremental();
    bool requestClear();

    bool dumpBusy() const { return dumpBusy_; }
    bool fullDumpBusy() const { return dumpBusy_ && fullDumpPending_; }
    bool hasLastRecordId() const { return haveRid_; }
    quint32 lastRecordId() const { return lastRid_; }
    quint32 writeCount() const { return writeCount_; }
    quint32 freeRecords() const { return freeRecords_; }
    void setLastRecordId(quint32 rid, bool valid);
    void clearTrace();

    QString dumpPhase() const { return dumpPhase_; }
    QString dumpStatus() const { return dumpStatus_; }
    QString serialTrace() const { return trace_.join(QLatin1Char('\n')); }
    int dumpPercent() const;

    // Link diagnostics. These separate "the cable is dead" from "the cable is
    // alive but the framing does not match", which is otherwise indistinguishable
    // from the dashboard.
    quint64 bytesReceived() const { return bytesReceived_; }
    quint64 framesDecoded() const { return framesDecoded_; }
    quint64 discardedBytes() const { return parser_.discardedBytes() + protoDiscarded_; }
    QDateTime lastByteAt() const { return lastByteAt_; }
    QDateTime lastFrameAt() const { return lastFrameAt_; }
    QString rawTailHex() const;
    void resetStats();

signals:
    void packetReceived(Slf::NodeData data);
    void connectionStateChanged(Slf::ConnectionState state);
    void errorOccurred(QString message);
    void statsChanged();
    void dumpBusyChanged();
    void statusReceived(quint32 writeCount, quint32 freeRecords);
    void dumpFinished(int recordCount, quint32 lastRid);
    void protocolError(quint8 code);
    void traceChanged();

private slots:
    void onReadyRead();
    void onErrorOccurred(QSerialPort::SerialPortError error);
    void onResponseTimeout();

private:
    enum class QueuedCommand { None, Status, FullDump, Incremental, Clear };

    bool writeCommand(quint8 cmd, const QByteArray &payload = QByteArray(), int timeoutMs = 0);
    void setDumpBusy(bool busy);
    void completeInFlight();
    void dispatchQueued();
    void drainBuffers();
    void handleResponse(quint8 cmd, const QByteArray &payload);
    void emitPackets(const QVector<NodeData> &packets);
    void logTrace(const QString &line);
    void setPhase(const QString &phase, const QString &detail);
    void noteRxProgress();
    static QString commandName(quint8 cmd);
    static QString toHex(const QByteArray &bytes, int maxBytes = 24);

    QSerialPort serial_;
    PacketParser parser_;
    QString portName_;
    QString lastError_;
    QByteArray protoBuf_;
    QTimer responseTimer_;

    quint64 bytesReceived_ = 0;
    quint64 framesDecoded_ = 0;
    quint64 protoDiscarded_ = 0;
    QDateTime lastByteAt_;
    QDateTime lastFrameAt_;
    QByteArray rawTail_;

    bool dumpBusy_ = false;
    bool fullDumpPending_ = false;
    quint8 pendingCmd_ = 0;
    QueuedCommand queued_ = QueuedCommand::None;
    bool haveRid_ = false;
    quint32 lastRid_ = 0;
    quint32 writeCount_ = 0;
    quint32 freeRecords_ = 0;

    QStringList trace_;
    QString dumpPhase_;
    QString dumpStatus_;
    bool headerSeen_ = false;
    quint32 expectPayload_ = 0;
    int lastProgressPct_ = -1;
};

} // namespace Slf

#endif
