#include "AlertListModel.h"

#include "core/Types.h"

namespace Slf {

AlertListModel::AlertListModel(NodeManager *manager, QObject *parent)
    : QAbstractListModel(parent)
    , manager_(manager)
{
    reload();
}

int AlertListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return rows_.size();
}

QVariant AlertListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size())
        return {};
    const AlertRecord &a = rows_.at(index.row());
    switch (role) {
    case AlertIdRole: return int(a.id);
    case TimestampRole: return a.timestamp;
    case TimestampTextRole: return a.timestamp.toString(QStringLiteral("yyyy-MM-dd  HH:mm:ss"));
    case DeviceIdRole: return int(a.deviceId);
    case IdStringRole: return deviceIdString(a.deviceId);
    case ZoneRole: return a.zone;
    case TypeRole: return int(a.type);
    case TypeLabelRole: return eventTypeLabel(a.type);
    case ValueRole: return a.triggerValue;
    case AcknowledgedRole: return a.acknowledged;
    case FreshRole: return a.timestamp.secsTo(QDateTime::currentDateTime()) < 2;
    default: return {};
    }
}

QHash<int, QByteArray> AlertListModel::roleNames() const
{
    return {
        {AlertIdRole, "alertId"},
        {TimestampRole, "timestamp"},
        {TimestampTextRole, "timestampText"},
        {DeviceIdRole, "deviceId"},
        {IdStringRole, "idString"},
        {ZoneRole, "zone"},
        {TypeRole, "eventType"},
        {TypeLabelRole, "typeLabel"},
        {ValueRole, "triggerValue"},
        {AcknowledgedRole, "acknowledged"},
        {FreshRole, "fresh"},
    };
}

void AlertListModel::setTypeFilter(int v)
{
    if (typeFilter_ == v)
        return;
    typeFilter_ = v;
    reload();
    emit filterChanged();
}

void AlertListModel::setZoneFilter(const QString &v)
{
    if (zoneFilter_ == v)
        return;
    zoneFilter_ = v;
    reload();
    emit filterChanged();
}

void AlertListModel::setDateFilter(int v)
{
    if (dateFilter_ == v)
        return;
    dateFilter_ = v;
    reload();
    emit filterChanged();
}

void AlertListModel::handleAlertRaised(const AlertRecord &alert)
{
    if (!matches(alert))
        return;
    beginInsertRows(QModelIndex(), 0, 0);
    rows_.prepend(alert);
    endInsertRows();
    emit alertInserted();
}

void AlertListModel::reload()
{
    beginResetModel();
    rows_ = filtered();
    endResetModel();
}

bool AlertListModel::matches(const AlertRecord &a) const
{
    if (typeFilter_ >= 0 && int(a.type) != typeFilter_)
        return false;
    if (!zoneFilter_.isEmpty() && a.zone != zoneFilter_)
        return false;
    if (dateFilter_ == 1 && a.timestamp.secsTo(QDateTime::currentDateTime()) > 24 * 3600)
        return false;
    if (dateFilter_ == 2 && a.timestamp.secsTo(QDateTime::currentDateTime()) > 7 * 24 * 3600)
        return false;
    return true;
}

QVector<AlertRecord> AlertListModel::filtered() const
{
    QVector<AlertRecord> out;
    const auto all = manager_->alerts();
    for (const AlertRecord &a : all) {
        if (matches(a))
            out.append(a);
    }
    return out;
}

} // namespace Slf
