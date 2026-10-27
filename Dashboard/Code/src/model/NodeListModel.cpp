#include "NodeListModel.h"

#include "core/Types.h"

#include <algorithm>

namespace Slf {

NodeListModel::NodeListModel(NodeManager *manager, QObject *parent)
    : QAbstractListModel(parent)
    , manager_(manager)
{
}

int NodeListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return ids_.size();
}

QVariant NodeListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= ids_.size())
        return {};

    const NodeState n = manager_->node(ids_.at(index.row()));
    switch (role) {
    case DeviceIdRole: return int(n.profile.deviceId);
    case IdStringRole: return deviceIdString(n.profile.deviceId);
    case ZoneRole: return n.profile.zoneLabel;
    case ZoneIdRole: return n.profile.zoneId;
    case StatusRole: return int(n.status);
    case AcknowledgedRole: return n.acknowledged;
    case EverSeenRole: return n.everSeen;
    case MoistureRole: return n.latest.moisturePercent();
    case TiltXRole: return n.latest.tiltXDeg();
    case TiltYRole: return n.latest.tiltYDeg();
    case TiltZRole: return n.latest.tiltZDeg();
    case TiltMagRole: return n.latest.tiltMagnitudeDeg();
    case LastSeenRole: return n.latest.timestamp;
    case RelativeTimeRole: return relativeTime(n.latest.timestamp, now_);
    case SparklineRole: {
        QVariantList pts;
        const int start = qMax(0, n.history.size() - 20);
        for (int i = start; i < n.history.size(); ++i)
            pts.append(n.history.at(i).moisture);
        return pts;
    }
    case MapXRole: return n.profile.mapPos.x();
    case MapYRole: return n.profile.mapPos.y();
    default: return {};
    }
}

QHash<int, QByteArray> NodeListModel::roleNames() const
{
    return {
        {DeviceIdRole, "deviceId"},
        {IdStringRole, "idString"},
        {ZoneRole, "zone"},
        {ZoneIdRole, "zoneId"},
        {StatusRole, "status"},
        {AcknowledgedRole, "acknowledged"},
        {EverSeenRole, "everSeen"},
        {MoistureRole, "moisture"},
        {TiltXRole, "tiltX"},
        {TiltYRole, "tiltY"},
        {TiltZRole, "tiltZ"},
        {TiltMagRole, "tiltMag"},
        {LastSeenRole, "lastSeen"},
        {RelativeTimeRole, "relativeTime"},
        {SparklineRole, "sparkline"},
        {MapXRole, "mapX"},
        {MapYRole, "mapY"},
    };
}

void NodeListModel::setNow(const QDateTime &now)
{
    now_ = now;
    if (!ids_.isEmpty())
        emit dataChanged(index(0), index(ids_.size() - 1), {RelativeTimeRole});
}

void NodeListModel::handleNodeUpdated(quint16 id)
{
    const NodeState n = manager_->node(id);
    if (!n.everSeen)
        return;

    const int existing = indexOf(id);
    if (existing < 0) {
        int row = 0;
        while (row < ids_.size() && ids_.at(row) < id)
            ++row;
        beginInsertRows(QModelIndex(), row, row);
        ids_.insert(row, id);
        endInsertRows();
        return;
    }

    emit dataChanged(index(existing), index(existing));
}

void NodeListModel::handleNodeRemoved(quint16 id)
{
    const int existing = indexOf(id);
    if (existing < 0)
        return;
    beginRemoveRows(QModelIndex(), existing, existing);
    ids_.removeAt(existing);
    endRemoveRows();
}

void NodeListModel::refreshAll()
{
    if (!ids_.isEmpty())
        emit dataChanged(index(0), index(ids_.size() - 1));
}

int NodeListModel::indexOf(quint16 id) const
{
    return ids_.indexOf(id);
}

} // namespace Slf
