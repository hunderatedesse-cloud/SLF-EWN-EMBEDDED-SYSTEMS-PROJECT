#include "SerialPortReader.h"

#include <QSerialPortInfo>
#include <QTime>

// Official receiver protocol v1.0: dashboard sends one command byte (+ optional
// payload) and waits for a complete 0xAA | cmd | uint32be length | payload reply
// before sending the next command. The receiver never transmits unsolicited.

namespace Slf {

namespace {
constexpr int kRawTailBytes = 48;
constexpr quint32 kMaxDumpPayload = 262144;

void applyLinkSettings(QSerialPort &serial)
{
    serial.setBaudRate(QSerialPort::Baud115200);
    serial.setDataBits(QSerialPort::Data8);
    serial.setParity(QSerialPort::NoParity);
    serial.setStopBits(QSerialPort::OneStop);
    serial.setFlowControl(QSerialPort::NoFlowControl);
}

quint32 readU32be(const quint8 *p)
{
    return (quint32(p[0]) << 24) | (quint32(p[1]) << 16) | (quint32(p[2]) << 8) | quint32(p[3]);
}

} // namespace

SerialPortReader::SerialPortReader(QObject *parent)
    : QObject(parent)
{
    applyLinkSettings(serial_);
    responseTimer_.setSingleShot(true);

    connect(&serial_, &QSerialPort::readyRead, this, &SerialPortReader::onReadyRead);
    connect(&serial_, &QSerialPort::errorOccurred, this, &SerialPortReader::onErrorOccurred);
    connect(&responseTimer_, &QTimer::timeout, this, &SerialPortReader::onResponseTimeout);
}

SerialPortReader::~SerialPortReader()
{
    close();
}

bool SerialPortReader::open(const QString &portName)
{
    if (serial_.isOpen())
        serial_.close();

    portName_ = portName;

    bool matched = false;
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        if (info.portName().compare(portName, Qt::CaseInsensitive) == 0
            || info.systemLocation().compare(portName, Qt::CaseInsensitive) == 0) {
            serial_.setPort(info);
            matched = true;
            break;
        }
    }
    if (!matched)
        serial_.setPortName(portName);

    parser_.reset();
    protoBuf_.clear();
    queued_ = QueuedCommand::None;
    pendingCmd_ = 0;
    resetStats();
    applyLinkSettings(serial_);

    emit connectionStateChanged(ConnectionState::Connecting);

    // ReadWrite keeps the DTR/RTS lines asserted, which many USB-UART bridges
    // and Arduino-style boards require before they will start transmitting.
    if (!serial_.open(QIODevice::ReadWrite)) {
        lastError_ = serial_.errorString();
        emit errorOccurred(lastError_);
        emit connectionStateChanged(ConnectionState::Failed);
        return false;
    }

    applyLinkSettings(serial_);
    serial_.setReadBufferSize(int(kMaxDumpPayload) + 64);
    serial_.setDataTerminalReady(true);
    serial_.setRequestToSend(true);
    serial_.clear(QSerialPort::AllDirections);

    lastError_.clear();
    logTrace(QStringLiteral("OPEN  %1  115200 8N1").arg(portName_));
    setPhase(QStringLiteral("Open"), QStringLiteral("Port open — sending commands when ready"));
    emit connectionStateChanged(ConnectionState::Connected);
    return true;
}

void SerialPortReader::close()
{
    const bool wasOpen = serial_.isOpen();
    responseTimer_.stop();
    queued_ = QueuedCommand::None;
    pendingCmd_ = 0;
    setDumpBusy(false);
    if (wasOpen)
        serial_.close();
    parser_.reset();
    protoBuf_.clear();
    if (wasOpen) {
        logTrace(QStringLiteral("CLOSE  %1").arg(portName_));
        setPhase(QStringLiteral("Closed"), QStringLiteral("Port closed"));
        emit connectionStateChanged(ConnectionState::Disconnected);
    }
}

bool SerialPortReader::isOpen() const
{
    return serial_.isOpen();
}

void SerialPortReader::setLastRecordId(quint32 rid, bool valid)
{
    lastRid_ = rid;
    haveRid_ = valid;
}

void SerialPortReader::clearTrace()
{
    trace_.clear();
    emit traceChanged();
}

int SerialPortReader::dumpPercent() const
{
    if (!dumpBusy_)
        return dumpPhase_.isEmpty() ? 0 : 100;
    const int total = int(6 + expectPayload_);
    if (total <= 0)
        return 0;
    return qBound(0, (protoBuf_.size() * 100) / total, dumpBusy_ ? 99 : 100);
}

QString SerialPortReader::commandName(quint8 cmd)
{
    switch (cmd) {
    case kCmdStatus: return QStringLiteral("STATUS");
    case kCmdFullDump: return QStringLiteral("FULL_DUMP");
    case kCmdIncremental: return QStringLiteral("INCREMENTAL");
    case kCmdClear: return QStringLiteral("CLEAR");
    case kCmdError: return QStringLiteral("ERROR");
    default: return QStringLiteral("0x%1").arg(cmd, 2, 16, QLatin1Char('0')).toUpper();
    }
}

QString SerialPortReader::toHex(const QByteArray &bytes, int maxBytes)
{
    const QByteArray cut = bytes.left(maxBytes);
    QString s = QString::fromLatin1(cut.toHex(' ')).toUpper();
    if (bytes.size() > maxBytes)
        s += QStringLiteral(" … +%1 B").arg(bytes.size() - maxBytes);
    return s;
}

void SerialPortReader::logTrace(const QString &line)
{
    const QString stamped = QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz"))
                            + QLatin1String("  ") + line;
    trace_.append(stamped);
    while (trace_.size() > 80)
        trace_.removeFirst();
    dumpStatus_ = line;
    emit traceChanged();
}

void SerialPortReader::setPhase(const QString &phase, const QString &detail)
{
    dumpPhase_ = phase;
    if (!detail.isEmpty())
        dumpStatus_ = detail;
    emit traceChanged();
}

void SerialPortReader::noteRxProgress()
{
    if (!dumpBusy_)
        return;
    if (!headerSeen_) {
        setPhase(QStringLiteral("Wait AA"),
                 protoBuf_.isEmpty()
                     ? QStringLiteral("TX sent — waiting for 0xAA header")
                     : QStringLiteral("Waiting for header  ·  %1 / 6 bytes  %2")
                           .arg(qMin(protoBuf_.size(), 6))
                           .arg(toHex(protoBuf_, 8)));
        return;
    }
    const int got = protoBuf_.size();
    const int total = int(6 + expectPayload_);
    const int pct = total > 0 ? qBound(0, (got * 100) / total, 99) : 0;
    if (pct == lastProgressPct_ && got > 6)
        return;
    lastProgressPct_ = pct;
    const int recs = int(expectPayload_ / kDumpRecordSize);
    setPhase(QStringLiteral("RX %1/%2").arg(got).arg(total),
             QStringLiteral("Receiving %1  ·  %2 / %3 bytes (%4%)  ·  %5 records expected")
                 .arg(commandName(pendingCmd_), QString::number(got), QString::number(total),
                      QString::number(pct), QString::number(recs)));
}

QString SerialPortReader::rawTailHex() const
{
    if (rawTail_.isEmpty())
        return QString();
    return QString::fromLatin1(rawTail_.toHex(' ')).toUpper();
}

void SerialPortReader::resetStats()
{
    bytesReceived_ = 0;
    framesDecoded_ = 0;
    protoDiscarded_ = 0;
    lastByteAt_ = QDateTime();
    lastFrameAt_ = QDateTime();
    rawTail_.clear();
    emit statsChanged();
}

void SerialPortReader::setDumpBusy(bool busy)
{
    if (!busy)
        fullDumpPending_ = false;
    if (dumpBusy_ == busy)
        return;
    dumpBusy_ = busy;
    emit dumpBusyChanged();
}

bool SerialPortReader::writeCommand(quint8 cmd, const QByteArray &payload, int timeoutMs)
{
    if (!serial_.isOpen()) {
        lastError_ = QStringLiteral("Port closed");
        emit errorOccurred(lastError_);
        return false;
    }
    if (dumpBusy_)
        return false;

    // Arm the receiver state *before* write(). On Windows a COMPIM/FTDI
    // reply can surface as a synchronous readyRead during flush; if dumpBusy
    // is still false those 0xAA bytes are fed to the ASCII parser and lost.
    pendingCmd_ = cmd;
    headerSeen_ = false;
    expectPayload_ = 0;
    lastProgressPct_ = -1;
    protoBuf_.clear();
    parser_.reset();
    if (timeoutMs > 0)
        responseTimer_.start(timeoutMs);
    setDumpBusy(true);

    QByteArray buf;
    buf.reserve(1 + payload.size());
    buf.append(char(cmd));
    buf.append(payload);

    const qint64 n = serial_.write(buf);
    serial_.waitForBytesWritten(500);
    serial_.flush();
    if (n != buf.size()) {
        lastError_ = serial_.errorString();
        if (lastError_.isEmpty())
            lastError_ = QStringLiteral("Failed to write receiver command");
        responseTimer_.stop();
        pendingCmd_ = 0;
        setDumpBusy(false);
        emit errorOccurred(lastError_);
        return false;
    }

    logTrace(QStringLiteral("TX  %1  %2").arg(commandName(cmd), toHex(buf)));
    setPhase(QStringLiteral("TX %1").arg(commandName(cmd)),
             QStringLiteral("Sent %1 — waiting for 0xAA %2")
                 .arg(commandName(cmd), commandName(cmd)));
    return true;
}

void SerialPortReader::completeInFlight()
{
    responseTimer_.stop();
    pendingCmd_ = 0;
    headerSeen_ = false;
    expectPayload_ = 0;
    lastProgressPct_ = -1;
    setDumpBusy(false);
    dispatchQueued();
}

void SerialPortReader::dispatchQueued()
{
    const QueuedCommand next = queued_;
    queued_ = QueuedCommand::None;
    switch (next) {
    case QueuedCommand::Status:
        requestStatus();
        break;
    case QueuedCommand::FullDump:
        requestFullDump();
        break;
    case QueuedCommand::Incremental:
        requestIncremental();
        break;
    case QueuedCommand::Clear:
        requestClear();
        break;
    case QueuedCommand::None:
        break;
    }
}

bool SerialPortReader::requestStatus()
{
    if (dumpBusy_) {
        if (queued_ == QueuedCommand::None) {
            queued_ = QueuedCommand::Status;
            logTrace(QStringLiteral("QUEUE  STATUS  (waiting for current reply)"));
        }
        return true;
    }
    return writeCommand(kCmdStatus, {}, 4000);
}

bool SerialPortReader::requestFullDump()
{
    if (dumpBusy_) {
        queued_ = QueuedCommand::FullDump;
        logTrace(QStringLiteral("QUEUE  FULL_DUMP  (waiting for current reply)"));
        return true;
    }
    fullDumpPending_ = true;
    if (writeCommand(kCmdFullDump, {}, 30000))
        return true;
    fullDumpPending_ = false;
    return false;
}

bool SerialPortReader::requestIncremental()
{
    if (!haveRid_)
        return requestFullDump();
    if (dumpBusy_) {
        if (queued_ != QueuedCommand::FullDump && queued_ != QueuedCommand::Clear) {
            queued_ = QueuedCommand::Incremental;
            logTrace(QStringLiteral("QUEUE  INCREMENTAL  last RID %1").arg(lastRid_));
        }
        return true;
    }

    QByteArray id(4, 0);
    auto *p = reinterpret_cast<quint8 *>(id.data());
    p[0] = static_cast<quint8>((lastRid_ >> 24) & 0xFF);
    p[1] = static_cast<quint8>((lastRid_ >> 16) & 0xFF);
    p[2] = static_cast<quint8>((lastRid_ >> 8) & 0xFF);
    p[3] = static_cast<quint8>(lastRid_ & 0xFF);
    return writeCommand(kCmdIncremental, id, 15000);
}

bool SerialPortReader::requestClear()
{
    if (dumpBusy_) {
        queued_ = QueuedCommand::Clear;
        logTrace(QStringLiteral("QUEUE  CLEAR  (waiting for current reply)"));
        return true;
    }
    return writeCommand(kCmdClear, {}, 3000);
}

void SerialPortReader::onResponseTimeout()
{
    lastError_ = QStringLiteral("Receiver did not answer");
    logTrace(QStringLiteral("TIMEOUT  %1  had %2 bytes  %3")
                 .arg(commandName(pendingCmd_))
                 .arg(protoBuf_.size())
                 .arg(protoBuf_.isEmpty() ? QStringLiteral("no reply") : toHex(protoBuf_, 16)));
    setPhase(QStringLiteral("Timeout"), lastError_);
    emit protocolError(0x02);
    completeInFlight();
}

void SerialPortReader::emitPackets(const QVector<NodeData> &packets)
{
    if (packets.isEmpty())
        return;
    framesDecoded_ += static_cast<quint64>(packets.size());
    lastFrameAt_ = QDateTime::currentDateTime();
    for (const NodeData &p : packets) {
        emit packetReceived(p);
        logTrace(QStringLiteral("RX  ASCII  %1  moist %2%  X=%3 Y=%4 Z=%5")
                     .arg(deviceIdString(p.deviceId))
                     .arg(p.moisturePercent(), 0, 'f', 2)
                     .arg(p.tiltX)
                     .arg(p.tiltY)
                     .arg(p.tiltZ));
    }
}

void SerialPortReader::handleResponse(quint8 cmd, const QByteArray &payload)
{
    if (pendingCmd_ != 0 && cmd != pendingCmd_ && cmd != kCmdError) {
        protoDiscarded_ += static_cast<quint64>(payload.size());
        logTrace(QStringLiteral("RX  ignored %1  (waiting for %2)  %3")
                     .arg(commandName(cmd), commandName(pendingCmd_), toHex(payload, 12)));
        return;
    }

    const auto *raw = reinterpret_cast<const quint8 *>(payload.constData());

    if (cmd == kCmdStatus) {
        if (payload.size() >= 8) {
            writeCount_ = readU32be(raw);
            freeRecords_ = readU32be(raw + 4);
            logTrace(QStringLiteral("RX  STATUS  write_count=%1  free=%2  %3")
                         .arg(writeCount_)
                         .arg(freeRecords_)
                         .arg(toHex(payload, 8)));
            setPhase(QStringLiteral("STATUS"),
                     QStringLiteral("STATUS  %1 stored  ·  %2 free").arg(writeCount_).arg(freeRecords_));
            emit statusReceived(writeCount_, freeRecords_);
        } else {
            logTrace(QStringLiteral("RX  STATUS  short payload %1 B").arg(payload.size()));
            emit statusReceived(writeCount_, freeRecords_);
        }
        completeInFlight();
        return;
    }

    if (cmd == kCmdFullDump || cmd == kCmdIncremental) {
        const int n = payload.size() / kDumpRecordSize;
        protoDiscarded_ += static_cast<quint64>(payload.size() % kDumpRecordSize);
        int accepted = 0;
        int badCrc = 0;
        for (int i = 0; i < n; ++i) {
            NodeData data;
            quint32 rid = 0;
            if (!PacketParser::decodeDumpRecord(raw + i * kDumpRecordSize, data, &rid)) {
                ++protoDiscarded_;
                ++badCrc;
                continue;
            }
            if (!haveRid_ || rid > lastRid_) {
                lastRid_ = rid;
                haveRid_ = true;
            }
            emit packetReceived(data);
            ++accepted;
            if (accepted <= 5) {
                logTrace(QStringLiteral("    RID %1  %2  moist %3%  X=%4 Y=%5 Z=%6%7")
                             .arg(rid)
                             .arg(deviceIdString(data.deviceId))
                             .arg(data.moisturePercent(), 0, 'f', 2)
                             .arg(data.tiltX)
                             .arg(data.tiltY)
                             .arg(data.tiltZ)
                             .arg(data.hardwareEmergency() ? QStringLiteral("  EMERGENCY") : QString()));
            }
        }
        if (accepted > 5)
            logTrace(QStringLiteral("    … %1 more records").arg(accepted - 5));
        logTrace(QStringLiteral("RX  %1  %2 bytes  %3 ok  %4 bad CRC  last RID %5")
                     .arg(commandName(cmd))
                     .arg(payload.size())
                     .arg(accepted)
                     .arg(badCrc)
                     .arg(haveRid_ ? QString::number(lastRid_) : QStringLiteral("—")));
        setPhase(accepted > 0 ? QStringLiteral("%1 recs").arg(accepted) : QStringLiteral("Empty"),
                 QStringLiteral("%1 done  ·  %2 records stored  ·  last RID %3")
                     .arg(commandName(cmd))
                     .arg(accepted)
                     .arg(haveRid_ ? QString::number(lastRid_) : QStringLiteral("—")));
        if (accepted > 0) {
            framesDecoded_ += static_cast<quint64>(accepted);
            lastFrameAt_ = QDateTime::currentDateTime();
        }
        completeInFlight();
        emit dumpFinished(accepted, lastRid_);
        return;
    }

    if (cmd == kCmdClear) {
        writeCount_ = 0;
        freeRecords_ = 0;
        lastRid_ = 0;
        haveRid_ = false;
        logTrace(QStringLiteral("RX  CLEAR  EEPROM erased  next RID = 0"));
        setPhase(QStringLiteral("Cleared"), QStringLiteral("Receiver log cleared — next RID starts at 0"));
        emit statusReceived(0, 0);
        emit dumpFinished(0, 0);
        completeInFlight();
        return;
    }

    if (cmd == kCmdError) {
        const quint8 code = payload.isEmpty() ? quint8(0) : quint8(payload.at(0));
        lastError_ = (code == 0x02)
                         ? QStringLiteral("Receiver rejected the command (timeout / bad payload)")
                         : QStringLiteral("Receiver rejected the command");
        logTrace(QStringLiteral("RX  ERROR  code 0x%1  %2")
                     .arg(code, 2, 16, QLatin1Char('0'))
                     .arg(lastError_));
        setPhase(QStringLiteral("Error"), lastError_);
        emit errorOccurred(lastError_);
        emit protocolError(code);
        completeInFlight();
        return;
    }

    protoDiscarded_ += static_cast<quint64>(payload.size());
    logTrace(QStringLiteral("RX  unknown cmd %1  %2 B").arg(commandName(cmd)).arg(payload.size()));
    completeInFlight();
}

void SerialPortReader::drainBuffers()
{
    if (!dumpBusy_) {
        if (!protoBuf_.isEmpty()) {
            emitPackets(parser_.feed(protoBuf_));
            protoBuf_.clear();
        }
        return;
    }

    while (!protoBuf_.isEmpty()) {
        const auto first = static_cast<quint8>(protoBuf_.at(0));
        if (first != kProtoMagic) {
            const int aa = protoBuf_.indexOf(char(kProtoMagic));
            if (aa < 0) {
                protoDiscarded_ += static_cast<quint64>(protoBuf_.size());
                protoBuf_.clear();
                return;
            }
            protoDiscarded_ += static_cast<quint64>(aa);
            protoBuf_.remove(0, aa);
            continue;
        }

        if (protoBuf_.size() < 6)
            return;

        const auto *hdr = reinterpret_cast<const quint8 *>(protoBuf_.constData());
        const quint8 cmd = hdr[1];
        const quint32 length = readU32be(hdr + 2);
        if (length > kMaxDumpPayload) {
            protoBuf_.remove(0, 1);
            ++protoDiscarded_;
            continue;
        }
        if (!headerSeen_) {
            headerSeen_ = true;
            expectPayload_ = length;
            lastProgressPct_ = -1;
            logTrace(QStringLiteral("RX  header  AA %1  length %2  (%3 records)  %4")
                         .arg(commandName(cmd))
                         .arg(length)
                         .arg(length / kDumpRecordSize)
                         .arg(toHex(protoBuf_.left(6))));
            noteRxProgress();
        }
        if (quint32(protoBuf_.size()) < 6 + length)
            return;

        const QByteArray payload = protoBuf_.mid(6, int(length));
        protoBuf_.remove(0, 6 + int(length));
        handleResponse(cmd, payload);
    }
}

void SerialPortReader::onReadyRead()
{
    const QByteArray chunk = serial_.readAll();
    if (chunk.isEmpty())
        return;

    bytesReceived_ += static_cast<quint64>(chunk.size());
    lastByteAt_ = QDateTime::currentDateTime();

    rawTail_.append(chunk);
    if (rawTail_.size() > kRawTailBytes)
        rawTail_ = rawTail_.right(kRawTailBytes);

    protoBuf_.append(chunk);
    drainBuffers();
    if (dumpBusy_)
        noteRxProgress();
    emit statsChanged();
}

void SerialPortReader::onErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError)
        return;

    // A yanked cable surfaces as ResourceError. Report it so the dashboard stops
    // claiming the link is healthy while no packets are arriving.
    switch (error) {
    case QSerialPort::ResourceError:
    case QSerialPort::ReadError:
    case QSerialPort::DeviceNotFoundError:
    case QSerialPort::PermissionError:
    case QSerialPort::OpenError:
        lastError_ = serial_.errorString();
        emit errorOccurred(lastError_);
        responseTimer_.stop();
        queued_ = QueuedCommand::None;
        pendingCmd_ = 0;
        setDumpBusy(false);
        if (serial_.isOpen())
            serial_.close();
        emit connectionStateChanged(ConnectionState::Failed);
        break;
    default:
        break;
    }
}

} // namespace Slf
