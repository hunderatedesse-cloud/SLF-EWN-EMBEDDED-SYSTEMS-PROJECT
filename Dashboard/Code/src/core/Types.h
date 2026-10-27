#pragma once

#include <QColor>
#include <QDateTime>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QtMath>
#include <QtNumeric>
#include <algorithm>
#include <cmath>

namespace Slf {

enum class NodeStatus {
    Unknown = 0,
    Safe,
    Warning,
    Critical
};

enum class EventType {
    Warning,
    Critical,
    EmergencyInterrupt
};

enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Failed,
    Simulated
};

enum class Page {
    Dashboard,
    AlertLog,
    MapView,
    Settings
};

enum class TimeRange {
    Hours24,
    Days7,
    Days30
};

inline constexpr int kPacketSize = 12;
inline constexpr int kDumpRecordSize = 16;
inline constexpr quint32 kBaudRate = 115200;
inline constexpr int kLongOfflineSecs = 600;

// Receiver UART protocol (big-endian, 115200 8N1).
inline constexpr quint8 kProtoMagic = 0xAA;
inline constexpr quint8 kCmdStatus = 0x53;       // 'S'
inline constexpr quint8 kCmdFullDump = 0x44;     // 'D'
inline constexpr quint8 kCmdIncremental = 0x4E;  // 'N'
inline constexpr quint8 kCmdClear = 0x43;        // 'C'
inline constexpr quint8 kCmdError = 0xFF;
inline constexpr float kTiltRawSpan = 1950.f; // radio X/Y/Z full scale
inline constexpr float kTiltDegSpan = 180.f;  // mapped display range

inline float tiltRawToDeg(qint16 raw)
{
    return qBound(-kTiltDegSpan, float(raw) * kTiltDegSpan / kTiltRawSpan, kTiltDegSpan);
}

inline qint16 tiltDegToRaw(float deg)
{
    return static_cast<qint16>(qBound(-int(kTiltRawSpan),
                                      int(std::lround(double(deg) * double(kTiltRawSpan) / double(kTiltDegSpan))),
                                      int(kTiltRawSpan)));
}

struct NodeData {
    quint16 deviceId = 0;
    quint16 moistureRaw = 0; // 0..10000 => 0.00% .. 100.00%
    qint16 tiltX = 0;        // radio raw −1950..1950 → −180°..180°
    qint16 tiltY = 0;
    qint16 tiltZ = 0;
    quint8 hardwareStatus = 0;
    QDateTime timestamp;

    float moisturePercent() const { return moistureRaw / 100.0f; }
    float tiltXDeg() const { return tiltRawToDeg(tiltX); }
    float tiltYDeg() const { return tiltRawToDeg(tiltY); }
    float tiltZDeg() const { return tiltRawToDeg(tiltZ); }
    float tiltMagnitudeDeg() const
    {
        return std::max({std::fabs(tiltXDeg()), std::fabs(tiltYDeg()), std::fabs(tiltZDeg())});
    }

    bool hardwareEmergency() const { return (hardwareStatus & 0x80) != 0; }
};

struct Sample {
    QDateTime timestamp;
    float moisture = 0.f;
    float tiltXDeg = 0.f;
    float tiltYDeg = 0.f;
    float tiltZDeg = 0.f;
    float tiltMagDeg = 0.f;
};

struct Thresholds {
    float moistureWarning = 38.0f;
    float moistureCritical = 58.0f;
    float tiltWarningDeg = 8.0f;
    float tiltCriticalDeg = 16.0f;
};

inline bool geoValid(double lat, double lon)
{
    return qIsFinite(lat) && qIsFinite(lon);
}

struct NodeProfile {
    quint16 deviceId = 0;
    QString zoneId;
    QString zoneLabel;
    QPointF mapPos{0.5, 0.5};
    double latitude = qQNaN();
    double longitude = qQNaN();
    double altitude = qQNaN();
    bool useCustomThresholds = false;
    Thresholds thresholds;
    bool autoAssigned = false;
    bool manualPlace = false;
    QString locationName;
    QString description;

    bool hasGeo() const { return geoValid(latitude, longitude); }
};

struct ZoneDef {
    QString id;
    QString name;
    QString notes;
    QRectF bounds; // 0..1 canvas space
    QString color;
    double lat1 = qQNaN();
    double lon1 = qQNaN();
    double alt1 = qQNaN();
    double lat2 = qQNaN();
    double lon2 = qQNaN();
    double alt2 = qQNaN();

    bool hasCoverage() const { return geoValid(lat1, lon1) && geoValid(lat2, lon2); }

    bool contains(double lat, double lon, double alt) const
    {
        if (!hasCoverage() || !geoValid(lat, lon))
            return false;
        const double minLat = std::min(lat1, lat2);
        const double maxLat = std::max(lat1, lat2);
        const double minLon = std::min(lon1, lon2);
        const double maxLon = std::max(lon1, lon2);
        if (lat < minLat || lat > maxLat || lon < minLon || lon > maxLon)
            return false;
        const bool altBand = qIsFinite(alt1) || qIsFinite(alt2);
        if (!altBand)
            return true;
        if (!qIsFinite(alt))
            return false;
        const double a1 = qIsFinite(alt1) ? alt1 : alt2;
        const double a2 = qIsFinite(alt2) ? alt2 : alt1;
        return alt >= std::min(a1, a2) && alt <= std::max(a1, a2);
    }

    double coverageVolume() const
    {
        if (!hasCoverage())
            return 1e300;
        const double dLat = std::fabs(lat1 - lat2);
        const double dLon = std::fabs(lon1 - lon2);
        const double dAlt = (qIsFinite(alt1) && qIsFinite(alt2)) ? std::fabs(alt1 - alt2) : 1.0;
        return std::max(dLat, 1e-9) * std::max(dLon, 1e-9) * std::max(dAlt, 1.0);
    }

    QString coverageLabel() const
    {
        if (!hasCoverage())
            return QStringLiteral("No geographic coverage set");
        QString s = QStringLiteral("Lat %1 … %2    Lon %3 … %4")
                        .arg(std::min(lat1, lat2), 0, 'f', 5)
                        .arg(std::max(lat1, lat2), 0, 'f', 5)
                        .arg(std::min(lon1, lon2), 0, 'f', 5)
                        .arg(std::max(lon1, lon2), 0, 'f', 5);
        if (qIsFinite(alt1) || qIsFinite(alt2)) {
            const double a1 = qIsFinite(alt1) ? alt1 : alt2;
            const double a2 = qIsFinite(alt2) ? alt2 : alt1;
            s += QStringLiteral("    Alt %1 … %2 m")
                     .arg(std::min(a1, a2), 0, 'f', 1)
                     .arg(std::max(a1, a2), 0, 'f', 1);
        }
        return s;
    }
};

struct AlertRecord {
    qint64 id = 0;
    QDateTime timestamp;
    quint16 deviceId = 0;
    QString zone;
    EventType type = EventType::Warning;
    QString triggerValue;
    bool acknowledged = false;
};

inline QString deviceIdString(quint16 id)
{
    // Radio firmware IDs are decimal NODE<n>. Legacy demo tags were hex mine IDs.
    if (id >= 0xA000)
        return QString("%1").arg(id, 4, 16, QChar('0')).toUpper();
    return QStringLiteral("NODE%1").arg(id);
}

inline QString eventTypeLabel(EventType type)
{
    switch (type) {
    case EventType::Warning: return QStringLiteral("Warning");
    case EventType::Critical: return QStringLiteral("Critical");
    case EventType::EmergencyInterrupt: return QStringLiteral("Emergency-Interrupt");
    }
    return {};
}

inline QString statusLabel(NodeStatus status)
{
    switch (status) {
    case NodeStatus::Safe: return QStringLiteral("SAFE");
    case NodeStatus::Warning: return QStringLiteral("WARNING");
    case NodeStatus::Critical: return QStringLiteral("CRITICAL");
    case NodeStatus::Unknown: return QStringLiteral("NO DATA");
    }
    return {};
}

inline QString relativeTime(const QDateTime &when, const QDateTime &now)
{
    if (!when.isValid())
        return QStringLiteral("—");
    const qint64 s = when.secsTo(now);
    if (s < 5)
        return QStringLiteral("just now");
    if (s < 60)
        return QStringLiteral("%1 s ago").arg(s);
    if (s < 3600)
        return QStringLiteral("%1 min ago").arg(s / 60);
    if (s < 86400)
        return QStringLiteral("%1 h ago").arg(s / 3600);
    return when.toString(QStringLiteral("MMM d  HH:mm"));
}

} // namespace Slf
