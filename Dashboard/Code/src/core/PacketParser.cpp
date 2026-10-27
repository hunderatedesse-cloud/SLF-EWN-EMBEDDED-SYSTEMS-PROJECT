#include "PacketParser.h"
#include "Crc8.h"

#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QtMath>
#include <cmath>

namespace Slf {

namespace {

quint16 readU16be(const quint8 *p)
{
    return static_cast<quint16>((p[0] << 8) | p[1]);
}

qint16 readI16be(const quint8 *p)
{
    return static_cast<qint16>(readU16be(p));
}

void writeU16be(quint8 *p, quint16 v)
{
    p[0] = static_cast<quint8>(v >> 8);
    p[1] = static_cast<quint8>(v & 0xFF);
}

void writeI16be(quint8 *p, qint16 v)
{
    writeU16be(p, static_cast<quint16>(v));
}

qint16 degToRaw(double deg)
{
    return tiltDegToRaw(float(deg));
}

} // namespace

bool PacketParser::decode(const quint8 *frame, NodeData &out)
{
    const quint8 crc = Crc8::compute(frame, kPacketSize - 1);
    if (crc != frame[kPacketSize - 1])
        return false;

    out.deviceId = readU16be(frame + 0);
    out.moistureRaw = readU16be(frame + 2);
    out.tiltX = readI16be(frame + 4);
    out.tiltY = readI16be(frame + 6);
    out.tiltZ = readI16be(frame + 8);
    out.hardwareStatus = frame[10];
    out.timestamp = QDateTime::currentDateTime();
    return true;
}

bool PacketParser::decodeDumpRecord(const quint8 *record16, NodeData &out, quint32 *ridOut)
{
    if (!record16)
        return false;
    const quint32 rid = (quint32(record16[0]) << 24) | (quint32(record16[1]) << 16)
                        | (quint32(record16[2]) << 8) | quint32(record16[3]);
    if (ridOut)
        *ridOut = rid;
    return decode(record16 + 4, out);
}

QByteArray PacketParser::encode(const NodeData &data)
{
    QByteArray frame(kPacketSize, '\0');
    auto *p = reinterpret_cast<quint8 *>(frame.data());
    writeU16be(p + 0, data.deviceId);
    writeU16be(p + 2, data.moistureRaw);
    writeI16be(p + 4, data.tiltX);
    writeI16be(p + 6, data.tiltY);
    writeI16be(p + 8, data.tiltZ);
    p[10] = data.hardwareStatus;
    p[11] = Crc8::compute(p, kPacketSize - 1);
    return frame;
}

bool PacketParser::looksLikeText(const QByteArray &bytes)
{
    if (bytes.isEmpty())
        return false;
    int printable = 0;
    for (char c : bytes) {
        const auto u = static_cast<unsigned char>(c);
        if (u == '\n' || u == '\r' || u == '\t' || (u >= 32 && u < 127))
            ++printable;
    }
    return printable * 100 / bytes.size() >= 85;
}

bool PacketParser::parseLoraLine(const QString &text, NodeData &out)
{
    static const QRegularExpression re(
        QStringLiteral(R"(NODE(\d+)_DATA:X_(-?\d+):Y_(-?\d+):Z_(-?\d+):MOIST_(\d+(?:\.\d+)?)%\s*(?::\s*EMERGENCY)?)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(text);
    if (!m.hasMatch())
        return false;

    const uint id = m.captured(1).toUInt();
    if (id == 0 || id > 0xFFFF)
        return false;

    out.deviceId = static_cast<quint16>(id);
    out.tiltX = static_cast<qint16>(qBound(-32000, m.captured(2).toInt(), 32000));
    out.tiltY = static_cast<qint16>(qBound(-32000, m.captured(3).toInt(), 32000));
    out.tiltZ = static_cast<qint16>(qBound(-32000, m.captured(4).toInt(), 32000));
    const double moistPct = m.captured(5).toDouble();
    out.moistureRaw = static_cast<quint16>(qBound(0, int(moistPct * 100.0 + 0.5), 10000));
    out.hardwareStatus = text.contains(QLatin1String("EMERGENCY"), Qt::CaseInsensitive) ? 0x80 : 0;
    out.timestamp = QDateTime::currentDateTime();
    return true;
}

bool PacketParser::parseAsciiLine(const QByteArray &line, NodeData &out)
{
    const QString text = QString::fromLatin1(line).trimmed();
    if (text.isEmpty() || text.startsWith(QLatin1Char('#')))
        return false;
    if (parseLoraLine(text, out))
        return true;

    const QStringList parts = text.split(QRegularExpression(QStringLiteral("[,;\\s]+")), Qt::SkipEmptyParts);
    if (parts.size() < 5)
        return false;

    bool idOk = false;
    QString idTok = parts.at(0);
    if (idTok.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
        idTok = idTok.mid(2);
    quint16 id = static_cast<quint16>(idTok.toUInt(&idOk, 16));
    if (!idOk || id == 0)
        return false;

    bool mOk = false, xOk = false, yOk = false, zOk = false;
    const double moisture = parts.at(1).toDouble(&mOk);
    const double tx = parts.at(2).toDouble(&xOk);
    const double ty = parts.at(3).toDouble(&yOk);
    const double tz = parts.at(4).toDouble(&zOk);
    if (!mOk || !xOk || !yOk || !zOk)
        return false;

    out.deviceId = id;
    if (moisture <= 100.0)
        out.moistureRaw = static_cast<quint16>(qBound(0, int(moisture * 100.0 + 0.5), 10000));
    else
        out.moistureRaw = static_cast<quint16>(qBound(0, int(moisture + 0.5), 10000));

    const bool degrees = std::fabs(tx) <= 90.0 && std::fabs(ty) <= 90.0 && std::fabs(tz) <= 90.0
                         && (parts.at(2).contains(QLatin1Char('.')) || parts.at(3).contains(QLatin1Char('.'))
                             || (std::fabs(tx) < 90.0 && std::fabs(tz) < 90.0 && std::fabs(tz) != 1000.0));
    if (degrees && std::fabs(tz) < 200.0) {
        out.tiltX = degToRaw(tx);
        out.tiltY = degToRaw(ty);
        out.tiltZ = degToRaw(tz);
    } else {
        out.tiltX = static_cast<qint16>(qBound(-32000, int(tx), 32000));
        out.tiltY = static_cast<qint16>(qBound(-32000, int(ty), 32000));
        out.tiltZ = static_cast<qint16>(qBound(-32000, int(tz), 32000));
    }

    if (parts.size() >= 6) {
        bool sOk = false;
        const int st = parts.at(5).toInt(&sOk);
        out.hardwareStatus = sOk ? static_cast<quint8>(st) : 0;
    } else {
        out.hardwareStatus = 0;
    }
    out.timestamp = QDateTime::currentDateTime();
    return true;
}

QVector<NodeData> PacketParser::feed(const QByteArray &chunk)
{
    buffer_.append(chunk);
    QVector<NodeData> packets;

    static const QRegularExpression loraRe(
        QStringLiteral(R"(NODE(\d+)_DATA:X_(-?\d+):Y_(-?\d+):Z_(-?\d+):MOIST_(\d+(?:\.\d+)?)%\s*(?::\s*EMERGENCY)?)"),
        QRegularExpression::CaseInsensitiveOption);

    if (looksLikeText(buffer_)) {
        const QString text = QString::fromLatin1(buffer_);
        int consumed = 0;
        QRegularExpressionMatchIterator it = loraRe.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            NodeData data;
            if (parseLoraLine(m.captured(0), data))
                packets.append(data);
            consumed = m.capturedEnd();
        }
        if (consumed > 0) {
            buffer_.remove(0, consumed);
        } else {
            // Banner / "awaiting radio" lines — drop complete lines that are not telemetry.
            int nl = 0;
            while ((nl = buffer_.indexOf('\n')) >= 0) {
                QByteArray line = buffer_.left(nl + 1);
                buffer_.remove(0, nl + 1);
                NodeData data;
                if (parseAsciiLine(line.trimmed(), data))
                    packets.append(data);
                else
                    discarded_ += static_cast<quint64>(line.size());
            }
            int cr = buffer_.indexOf('\r');
            if (cr >= 0 && !buffer_.contains('\n')) {
                QByteArray line = buffer_.left(cr);
                buffer_.remove(0, cr + 1);
                NodeData data;
                if (parseAsciiLine(line.trimmed(), data))
                    packets.append(data);
            }
        }
        if (buffer_.size() > 4096) {
            discarded_ += static_cast<quint64>(buffer_.size() - 256);
            buffer_ = buffer_.right(256);
        }
        return packets;
    }

    if (buffer_.size() > 8192) {
        discarded_ += static_cast<quint64>(buffer_.size() - 256);
        buffer_ = buffer_.right(256);
    }

    while (buffer_.size() >= kPacketSize) {
        const auto *raw = reinterpret_cast<const quint8 *>(buffer_.constData());
        NodeData data;
        if (decode(raw, data)) {
            packets.append(data);
            buffer_.remove(0, kPacketSize);
        } else {
            buffer_.remove(0, 1);
            ++discarded_;
        }
    }
    return packets;
}

void PacketParser::reset()
{
    buffer_.clear();
    discarded_ = 0;
}

} // namespace Slf
