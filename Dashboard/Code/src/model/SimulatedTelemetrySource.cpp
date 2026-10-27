#include "SimulatedTelemetrySource.h"
#include "core/PacketParser.h"

#include <QRandomGenerator>
#include <QtMath>
#include <algorithm>

namespace Slf {

namespace {

float noise(float amp)
{
    return (QRandomGenerator::global()->generateDouble() * 2.0 - 1.0) * amp;
}

} // namespace

SimulatedTelemetrySource::SimulatedTelemetrySource(QObject *parent)
    : QObject(parent)
{
    timer_.setInterval(380);
    connect(&timer_, &QTimer::timeout, this, &SimulatedTelemetrySource::tick);

    auto add = [this](quint16 id, float moist, bool climb, bool tilt, bool warn) {
        NodeSim n;
        n.id = id;
        n.moisture = moist;
        n.baseMoisture = moist;
        n.moistureClimb = climb;
        n.tiltCreep = tilt;
        n.warningHold = warn;
        n.tiltX = 12.f + noise(8);
        n.tiltY = -8.f + noise(8);
        n.tiltZ = 990.f + noise(6);
        nodes_.append(n);
    };

    add(0xA104, 31.2f, true,  false, false);
    add(0xA108, 18.4f, false, false, false);
    add(0xB201, 22.8f, false, false, false);
    add(0xB207, 41.6f, false, false, true);
    add(0xC312, 16.1f, false, false, false);
    add(0xC318, 24.0f, false, true,  false);
    add(0xD401, 11.3f, false, false, false);
    add(0xE510, 33.7f, false, false, false);
}

void SimulatedTelemetrySource::start()
{
    if (!timer_.isActive())
        timer_.start();
}

void SimulatedTelemetrySource::stop()
{
    timer_.stop();
}

QVector<Sample> SimulatedTelemetrySource::makeHistory(quint16 deviceId, const QDateTime &until, int minutes) const
{
    NodeSim proto;
    bool found = false;
    for (const NodeSim &n : nodes_) {
        if (n.id == deviceId) {
            proto = n;
            found = true;
            break;
        }
    }
    if (!found)
        return {};

    QVector<Sample> out;
    out.reserve(minutes);
    NodeSim n = proto;
    // Walk the same dynamics backwards by applying inverted noise of similar amplitude.
    for (int i = minutes; i >= 0; --i) {
        n.moisture = proto.baseMoisture
            + qSin(i / 47.0) * 2.4
            + qSin(i / 180.0) * 3.1
            + noise(0.35);
        if (n.moistureClimb)
            n.moisture += (minutes - i) * (12.0 / qMax(60, minutes));
        if (n.warningHold)
            n.moisture = proto.baseMoisture + qSin(i / 90.0) * 1.8 + noise(0.4);
        if (n.tiltCreep)
            n.tiltX = proto.tiltX + (minutes - i) * 0.04f + noise(1.2);

        n.moisture = qBound(4.0f, n.moisture, 92.0f);

        Sample s;
        s.timestamp = until.addSecs(-i * 60);
        s.moisture = n.moisture;
        s.tiltXDeg = tiltRawToDeg(static_cast<qint16>(n.tiltX));
        s.tiltYDeg = tiltRawToDeg(static_cast<qint16>(n.tiltY));
        s.tiltZDeg = tiltRawToDeg(static_cast<qint16>(n.tiltZ));
        s.tiltMagDeg = std::max({std::fabs(s.tiltXDeg), std::fabs(s.tiltYDeg), std::fabs(s.tiltZDeg)});
        out.append(s);
    }
    return out;
}

void SimulatedTelemetrySource::tick()
{
    if (nodes_.isEmpty())
        return;
    NodeSim &n = nodes_[cursor_ % nodes_.size()];
    cursor_++;
    emit packetReceived(nextPacket(n));
}

NodeData SimulatedTelemetrySource::nextPacket(NodeSim &n)
{
    n.tick++;

    n.moisture += noise(0.18f);
    if (n.moistureClimb)
        n.moisture += 0.035f;
    if (n.warningHold)
        n.moisture = n.baseMoisture + 3.2f * qSin(n.tick / 40.0) + noise(0.15f);
    if (n.id == 0xB201)
        n.moisture = n.baseMoisture + noise(1.1f);

    // Occasional rain pulse on E510
    if (n.id == 0xE510 && (n.tick % 220) == 80)
        n.drift = 6.0f;
    n.moisture += n.drift * 0.08f;
    n.drift *= 0.92f;

    n.moisture = qBound(5.0f, n.moisture, 88.0f);

    if (n.tiltCreep) {
        n.tiltX += 0.35f + noise(0.2f);
        n.tiltY += 0.12f;
    } else {
        n.tiltX += noise(1.4f);
        n.tiltY += noise(1.2f);
        n.tiltZ += noise(1.0f);
    }
    n.tiltZ = qBound(920.f, n.tiltZ, 1020.f);

    quint8 hw = 0x00;
    // Rare hardware interrupt on C318 once it has crept far enough
    if (n.tiltCreep && n.tiltX > 180.f)
        hw = 0x80;

    NodeData raw;
    raw.deviceId = n.id;
    raw.moistureRaw = static_cast<quint16>(qBound(0, int(n.moisture * 100.0f), 10000));
    raw.tiltX = static_cast<qint16>(n.tiltX);
    raw.tiltY = static_cast<qint16>(n.tiltY);
    raw.tiltZ = static_cast<qint16>(n.tiltZ);
    raw.hardwareStatus = hw;

    const QByteArray frame = PacketParser::encode(raw);
    NodeData parsed;
    PacketParser::decode(reinterpret_cast<const quint8 *>(frame.constData()), parsed);
    return parsed;
}

} // namespace Slf
