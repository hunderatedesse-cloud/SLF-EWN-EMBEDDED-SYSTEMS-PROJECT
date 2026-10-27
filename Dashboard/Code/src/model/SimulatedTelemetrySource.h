#ifndef SIMULATEDTELEMETRYSOURCE_H
#define SIMULATEDTELEMETRYSOURCE_H

#include "core/Types.h"

#include <QHash>
#include <QObject>
#include <QTimer>
#include <QVector>

namespace Slf {

class SimulatedTelemetrySource : public QObject
{
    Q_OBJECT
public:
    explicit SimulatedTelemetrySource(QObject *parent = nullptr);

    void start();
    void stop();
    bool isRunning() const { return timer_.isActive(); }

    QVector<Sample> makeHistory(quint16 deviceId, const QDateTime &until, int minutes) const;

signals:
    void packetReceived(Slf::NodeData data);

private:
    struct NodeSim {
        quint16 id = 0;
        float moisture = 20.f;
        float baseMoisture = 20.f;
        float drift = 0.f;
        float tiltX = 20.f; // milli-g offset from 0
        float tiltY = -10.f;
        float tiltZ = 980.f;
        int tick = 0;
        bool moistureClimb = false;
        bool tiltCreep = false;
        bool warningHold = false;
    };

    void tick();
    NodeData nextPacket(NodeSim &n);

    QTimer timer_;
    QVector<NodeSim> nodes_;
    int cursor_ = 0;
};

} // namespace Slf

#endif
