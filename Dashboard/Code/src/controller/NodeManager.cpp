#include "NodeManager.h"

#include <algorithm>

namespace Slf {

NodeManager::NodeManager(QObject *parent)
    : QObject(parent)
{
    ensureDefaults();
}

void NodeManager::attachDatabase(Database *db)
{
    db_ = db;
    if (!db_)
        return;

    loadPersistedState();
    autoAssignNodes();

    alerts_ = db_->loadAlerts();
    for (const AlertRecord &a : alerts_) {
        nextAlertId_ = qMax(nextAlertId_, a.id + 1);
        if (a.acknowledged && nodes_.contains(a.deviceId))
            nodes_[a.deviceId].acknowledged = true;
    }

    const qint64 since = QDateTime::currentDateTime().addDays(-30).toMSecsSinceEpoch();
    for (auto it = nodes_.begin(); it != nodes_.end(); ++it) {
        const QVector<Sample> history = db_->loadSamples(it.key(), since, kMaxHistory);
        if (history.isEmpty())
            continue;
        it->history = history;
        it->latest.deviceId = it.key();
        const Sample &last = history.last();
        it->latest.moistureRaw = static_cast<quint16>(qBound(0, int(last.moisture * 100), 10000));
        it->latest.timestamp = last.timestamp;
        it->status = evaluate(it->latest);
        it->everSeen = true;
    }

    emit alertsChanged();
    emit profilesChanged();
}

void NodeManager::ensureDefaults()
{
    zones_.clear();
    nodes_.clear();
}

void NodeManager::ingest(const NodeData &data)
{
    const bool isNew = !nodes_.contains(data.deviceId);
    NodeState &n = nodes_[data.deviceId];
    if (isNew) {
        n.profile.deviceId = data.deviceId;
        if (n.profile.zoneLabel.isEmpty()) {
            n.profile.zoneLabel = QStringLiteral("Unassigned");
            n.profile.zoneId = QStringLiteral("none");
            n.profile.mapPos = QPointF(0.5, 0.5);
        }
        if (db_)
            db_->saveProfile(n.profile);
        autoAssignNodes();
    }

    const NodeStatus previous = n.status;
    n.latest = data;
    n.everSeen = true;

    Sample s;
    s.timestamp = data.timestamp;
    s.moisture = data.moisturePercent();
    s.tiltXDeg = data.tiltXDeg();
    s.tiltYDeg = data.tiltYDeg();
    s.tiltZDeg = data.tiltZDeg();
    s.tiltMagDeg = data.tiltMagnitudeDeg();
    n.history.append(s);
    trimHistory(n);

    const NodeStatus next = evaluate(data);
    if (db_)
        db_->queueSample(data.deviceId, s, next);
    const bool worse = static_cast<int>(next) > static_cast<int>(previous)
                       && next != NodeStatus::Unknown;
    if (next != previous) {
        n.status = next;
        if (worse)
            n.acknowledged = false;
        emit nodeStateChanged(data.deviceId, next);
    } else {
        n.status = next;
    }

    if (data.hardwareEmergency()) {
        n.status = NodeStatus::Critical;
        n.acknowledged = false;
        raiseAlert(n, EventType::EmergencyInterrupt,
                   QStringLiteral("HW INT 0x%1").arg(data.hardwareStatus, 2, 16, QChar('0')));
        emit emergencyTriggered(data.deviceId);
        emit nodeStateChanged(data.deviceId, NodeStatus::Critical);
    } else if (worse) {
        if (next == NodeStatus::Critical) {
            raiseAlert(n, EventType::Critical,
                       QStringLiteral("M %1%  /  T %2°")
                           .arg(data.moisturePercent(), 0, 'f', 1)
                           .arg(data.tiltMagnitudeDeg(), 0, 'f', 1));
        } else if (next == NodeStatus::Warning) {
            raiseAlert(n, EventType::Warning,
                       QStringLiteral("M %1%  /  T %2°")
                           .arg(data.moisturePercent(), 0, 'f', 1)
                           .arg(data.tiltMagnitudeDeg(), 0, 'f', 1));
        }
    }

    if (isNew)
        emit nodeAdded(data.deviceId);
    emit nodeUpdated(data.deviceId, data);
}

void NodeManager::seedHistory(quint16 id, const QVector<Sample> &samples)
{
    if (!nodes_.contains(id))
        return;
    NodeState &n = nodes_[id];
    n.history = samples;
    trimHistory(n);
    if (!samples.isEmpty() && !n.everSeen) {
        n.latest.deviceId = id;
        n.latest.moistureRaw = static_cast<quint16>(qBound(0, int(samples.last().moisture * 100), 10000));
        n.latest.timestamp = samples.last().timestamp;
        n.status = evaluate(n.latest);
    }
}

QList<quint16> NodeManager::nodeIds() const
{
    QList<quint16> ids = nodes_.keys();
    std::sort(ids.begin(), ids.end());
    return ids;
}

NodeState NodeManager::node(quint16 id) const
{
    return nodes_.value(id);
}

bool NodeManager::hasNode(quint16 id) const
{
    return nodes_.contains(id);
}

void NodeManager::setThresholds(const Thresholds &t)
{
    thresholds_ = t;
    savePersistedState();
    emit thresholdsChanged(t);

    for (auto it = nodes_.begin(); it != nodes_.end(); ++it) {
        if (!it->everSeen)
            continue;
        it->latest.deviceId = it.key();
        const NodeStatus next = evaluate(it->latest);
        if (next != it->status) {
            it->status = next;
            emit nodeStateChanged(it.key(), next);
            emit nodeUpdated(it.key(), it->latest);
        }
    }
}

Thresholds NodeManager::thresholdsFor(quint16 id) const
{
    if (nodes_.contains(id) && nodes_[id].profile.useCustomThresholds)
        return nodes_[id].profile.thresholds;
    return thresholds_;
}

void NodeManager::setNodeThresholds(quint16 id, bool custom, const Thresholds &t)
{
    if (!nodes_.contains(id))
        return;
    NodeState &n = nodes_[id];
    n.profile.useCustomThresholds = custom;
    n.profile.thresholds = t;
    if (db_)
        db_->saveProfile(n.profile);
    if (n.everSeen) {
        n.latest.deviceId = id;
        const NodeStatus next = evaluate(n.latest);
        if (next != n.status) {
            n.status = next;
            emit nodeStateChanged(id, next);
            emit nodeUpdated(id, n.latest);
        }
    }
    emit profilesChanged();
}

void NodeManager::setNodeGeo(quint16 id, double lat, double lon, double alt)
{
    if (id == 0)
        return;
    const bool isNew = !nodes_.contains(id);
    NodeState &n = nodes_[id];
    if (isNew) {
        n.profile.deviceId = id;
        n.profile.zoneLabel = QStringLiteral("Unassigned");
        n.profile.zoneId = QStringLiteral("none");
        n.profile.mapPos = QPointF(0.5, 0.5);
        emit nodeAdded(id);
    }
    n.profile.latitude = lat;
    n.profile.longitude = lon;
    n.profile.altitude = alt;
    n.profile.manualPlace = false;
    if (db_)
        db_->saveProfile(n.profile);
    autoAssignNodes();
    emit profilesChanged();
}

void NodeManager::setNodeMeta(quint16 id, const QString &locationName, const QString &description)
{
    if (!nodes_.contains(id))
        return;
    nodes_[id].profile.locationName = locationName.trimmed();
    nodes_[id].profile.description = description.trimmed();
    if (db_)
        db_->saveProfile(nodes_[id].profile);
    emit profilesChanged();
}

void NodeManager::moveNodeOnMap(quint16 id, double lat, double lon, const QString &placeId)
{
    if (id == 0 || !geoValid(lat, lon))
        return;
    const bool isNew = !nodes_.contains(id);
    NodeState &n = nodes_[id];
    if (isNew) {
        n.profile.deviceId = id;
        n.profile.zoneLabel = QStringLiteral("Unassigned");
        n.profile.zoneId = QStringLiteral("none");
        n.profile.mapPos = QPointF(0.5, 0.5);
        emit nodeAdded(id);
    }
    n.profile.latitude = lat;
    n.profile.longitude = lon;

    if (!placeId.isEmpty() && placeId != QLatin1String("none")) {
        const ZoneDef z = place(placeId);
        if (!z.id.isEmpty()) {
            applyAssignment(id, z, true);
            emit profilesChanged();
            return;
        }
    }

    n.profile.manualPlace = false;
    if (db_)
        db_->saveProfile(n.profile);
    autoAssignNodes();
    emit profilesChanged();
}

void NodeManager::setZoneLabel(quint16 id, const QString &label)
{
    if (!nodes_.contains(id))
        return;
    nodes_[id].profile.zoneLabel = label;
    if (db_)
        db_->saveProfile(nodes_[id].profile);
    emit profilesChanged();
}

void NodeManager::setMapPos(quint16 id, const QPointF &pos)
{
    if (!nodes_.contains(id))
        return;
    nodes_[id].profile.mapPos = QPointF(qBound(0.02, pos.x(), 0.98), qBound(0.02, pos.y(), 0.98));
    if (db_)
        db_->saveProfile(nodes_[id].profile);
    emit profilesChanged();
}

void NodeManager::assignNode(quint16 id, const QString &placeId, bool manual)
{
    if (placeId.isEmpty() || placeId == QLatin1String("none")) {
        unassignNode(id, manual);
        return;
    }
    ZoneDef z;
    bool found = false;
    for (const ZoneDef &p : zones_) {
        if (p.id == placeId) {
            z = p;
            found = true;
            break;
        }
    }
    if (!found)
        return;

    applyAssignment(id, z, manual);
    emit profilesChanged();
}

void NodeManager::applyAssignment(quint16 id, const ZoneDef &z, bool manual)
{
    if (!nodes_.contains(id)) {
        NodeState n;
        n.profile.deviceId = id;
        nodes_.insert(id, n);
    }
    NodeState &n = nodes_[id];
    n.profile.zoneId = z.id;
    n.profile.zoneLabel = z.name;
    n.profile.manualPlace = manual;
    n.profile.autoAssigned = !manual;
    n.profile.mapPos = QPointF(z.bounds.center().x(), z.bounds.center().y());
    const int peers = [&]() {
        int c = 0;
        for (auto it = nodes_.constBegin(); it != nodes_.constEnd(); ++it) {
            if (it.key() != id && it->profile.zoneId == z.id)
                ++c;
        }
        return c;
    }();
    n.profile.mapPos += QPointF((peers % 3) * 0.03 - 0.03, (peers / 3) * 0.03);
    n.profile.mapPos.setX(qBound(z.bounds.left() + 0.02, n.profile.mapPos.x(), z.bounds.right() - 0.02));
    n.profile.mapPos.setY(qBound(z.bounds.top() + 0.02, n.profile.mapPos.y(), z.bounds.bottom() - 0.02));
    if (db_)
        db_->saveProfile(n.profile);
}

void NodeManager::unassignNode(quint16 id, bool manual)
{
    if (!nodes_.contains(id))
        return;
    nodes_[id].profile.zoneId = QStringLiteral("none");
    nodes_[id].profile.zoneLabel = QStringLiteral("Unassigned");
    nodes_[id].profile.manualPlace = manual;
    nodes_[id].profile.autoAssigned = false;
    if (db_)
        db_->saveProfile(nodes_[id].profile);
    emit profilesChanged();
}

void NodeManager::autoAssignNodes()
{
    bool changed = false;
    for (auto it = nodes_.begin(); it != nodes_.end(); ++it) {
        if (it->profile.manualPlace || !it->profile.hasGeo())
            continue;

        QString bestId;
        double bestVol = 1e300;
        for (const ZoneDef &z : zones_) {
            if (!z.contains(it->profile.latitude, it->profile.longitude, it->profile.altitude))
                continue;
            const double vol = z.coverageVolume();
            if (vol < bestVol) {
                bestVol = vol;
                bestId = z.id;
            }
        }

        if (!bestId.isEmpty()) {
            if (it->profile.zoneId != bestId) {
                applyAssignment(it.key(), place(bestId), false);
                changed = true;
            } else {
                it->profile.autoAssigned = true;
            }
        } else if (it->profile.autoAssigned) {
            it->profile.zoneId = QStringLiteral("none");
            it->profile.zoneLabel = QStringLiteral("Unassigned");
            it->profile.autoAssigned = false;
            if (db_)
                db_->saveProfile(it->profile);
            changed = true;
        }
    }
    if (changed)
        emit profilesChanged();
}

bool NodeManager::removeNode(quint16 id)
{
    if (!nodes_.contains(id))
        return false;
    nodes_.remove(id);
    if (db_)
        db_->deleteProfile(id);
    emit nodeRemoved(id);
    emit profilesChanged();
    return true;
}

ZoneDef NodeManager::place(const QString &id) const
{
    for (const ZoneDef &z : zones_) {
        if (z.id == id)
            return z;
    }
    return {};
}

bool NodeManager::upsertPlace(const ZoneDef &place)
{
    if (place.id.isEmpty() || place.name.trimmed().isEmpty())
        return false;
    ZoneDef z = place;
    z.name = z.name.trimmed();
    if (z.bounds.width() < 0.05)
        z.bounds.setWidth(0.18);
    if (z.bounds.height() < 0.05)
        z.bounds.setHeight(0.16);
    bool found = false;
    for (ZoneDef &existing : zones_) {
        if (existing.id == z.id) {
            existing = z;
            found = true;
            break;
        }
    }
    if (!found)
        zones_.append(z);
    if (db_)
        db_->savePlace(z);

    for (auto it = nodes_.begin(); it != nodes_.end(); ++it) {
        if (it->profile.zoneId == z.id)
            it->profile.zoneLabel = z.name;
    }
    autoAssignNodes();
    emit profilesChanged();
    return true;
}

bool NodeManager::removePlace(const QString &id)
{
    if (id.isEmpty())
        return false;
    for (int i = 0; i < zones_.size(); ++i) {
        if (zones_.at(i).id != id)
            continue;
        zones_.removeAt(i);
        if (db_)
            db_->deletePlace(id);
        for (auto it = nodes_.begin(); it != nodes_.end(); ++it) {
            if (it->profile.zoneId == id) {
                it->profile.zoneId = QStringLiteral("none");
                it->profile.zoneLabel = QStringLiteral("Unassigned");
                if (db_)
                    db_->saveProfile(it->profile);
            }
        }
        emit profilesChanged();
        return true;
    }
    return false;
}

int NodeManager::unacknowledgedCount() const
{
    int n = 0;
    for (const AlertRecord &a : alerts_) {
        if (!a.acknowledged)
            ++n;
    }
    return n;
}

void NodeManager::acknowledgeAlert(qint64 alertId)
{
    for (AlertRecord &a : alerts_) {
        if (a.id == alertId) {
            a.acknowledged = true;
            if (db_)
                db_->setAlertAcknowledged(alertId, operator_);
            if (nodes_.contains(a.deviceId)) {
                nodes_[a.deviceId].acknowledged = true;
                emit nodeUpdated(a.deviceId, nodes_[a.deviceId].latest);
            }
            emit alertsChanged();
            return;
        }
    }
}

void NodeManager::acknowledgeNode(quint16 id)
{
    if (!nodes_.contains(id))
        return;
    nodes_[id].acknowledged = true;
    for (AlertRecord &a : alerts_) {
        if (a.deviceId == id)
            a.acknowledged = true;
    }
    if (db_)
        db_->acknowledgeDeviceAlerts(id, operator_);
    emit nodeUpdated(id, nodes_[id].latest);
    emit alertsChanged();
}

void NodeManager::clearAlerts()
{
    alerts_.clear();
    if (db_) {
        db_->clearAlerts();
        db_->audit(operator_, QStringLiteral("clear-alert-log"));
    }
    emit alertsChanged();
}

QVector<Sample> NodeManager::samplesInRange(quint16 id, TimeRange range) const
{
    const NodeState n = nodes_.value(id);
    const qint64 spanMs =
        range == TimeRange::Hours24 ? 24LL * 3600 * 1000
        : range == TimeRange::Days7  ? 7LL * 24 * 3600 * 1000
                                     : 30LL * 24 * 3600 * 1000;
    const QDateTime cutoff = QDateTime::currentDateTime().addMSecs(-spanMs);
    QVector<Sample> out;
    out.reserve(n.history.size());
    for (const Sample &s : n.history) {
        if (s.timestamp >= cutoff)
            out.append(s);
    }
    return out;
}

void NodeManager::loadPersistedState()
{
    if (!db_)
        return;

    thresholds_.moistureWarning =
        db_->setting(QStringLiteral("th.moistureWarning"),
                     QString::number(thresholds_.moistureWarning)).toFloat();
    thresholds_.moistureCritical =
        db_->setting(QStringLiteral("th.moistureCritical"),
                     QString::number(thresholds_.moistureCritical)).toFloat();
    thresholds_.tiltWarningDeg =
        db_->setting(QStringLiteral("th.tiltWarning"),
                     QString::number(thresholds_.tiltWarningDeg)).toFloat();
    thresholds_.tiltCriticalDeg =
        db_->setting(QStringLiteral("th.tiltCritical"),
                     QString::number(thresholds_.tiltCriticalDeg)).toFloat();

    zones_ = db_->loadPlaces();

    const QVector<NodeProfile> stored = db_->loadProfiles();
    for (const NodeProfile &p : stored) {
        NodeState &n = nodes_[p.deviceId];
        n.profile = p;
    }
}

void NodeManager::savePersistedState() const
{
    if (!db_)
        return;

    db_->setSetting(QStringLiteral("th.moistureWarning"), QString::number(thresholds_.moistureWarning));
    db_->setSetting(QStringLiteral("th.moistureCritical"), QString::number(thresholds_.moistureCritical));
    db_->setSetting(QStringLiteral("th.tiltWarning"), QString::number(thresholds_.tiltWarningDeg));
    db_->setSetting(QStringLiteral("th.tiltCritical"), QString::number(thresholds_.tiltCriticalDeg));

    for (auto it = nodes_.constBegin(); it != nodes_.constEnd(); ++it)
        db_->saveProfile(it->profile);
}

NodeStatus NodeManager::evaluate(const NodeData &data) const
{
    const Thresholds th = thresholdsFor(data.deviceId);
    const float m = data.moisturePercent();
    const float t = data.tiltMagnitudeDeg();
    if (m >= th.moistureCritical || t >= th.tiltCriticalDeg)
        return NodeStatus::Critical;
    if (m >= th.moistureWarning || t >= th.tiltWarningDeg)
        return NodeStatus::Warning;
    return NodeStatus::Safe;
}

void NodeManager::raiseAlert(const NodeState &n, EventType type, const QString &value)
{
    AlertRecord a;
    a.id = nextAlertId_++;
    a.timestamp = QDateTime::currentDateTime();
    a.deviceId = n.profile.deviceId;
    a.zone = n.profile.zoneLabel;
    a.type = type;
    a.triggerValue = value;
    a.acknowledged = false;
    if (db_) {
        const qint64 rowId = db_->insertAlert(a);
        if (rowId > 0) {
            a.id = rowId;
            nextAlertId_ = qMax(nextAlertId_, rowId + 1);
        }
    }
    alerts_.prepend(a);
    emit alertRaised(a);
}

void NodeManager::trimHistory(NodeState &n)
{
    if (n.history.size() > kMaxHistory)
        n.history.erase(n.history.begin(), n.history.begin() + (n.history.size() - kMaxHistory));
}

} // namespace Slf
