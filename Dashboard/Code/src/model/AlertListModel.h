#ifndef ALERTLISTMODEL_H
#define ALERTLISTMODEL_H

#include "controller/NodeManager.h"

#include <QAbstractListModel>
#include <QVector>

namespace Slf {

class AlertListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int typeFilter READ typeFilter WRITE setTypeFilter NOTIFY filterChanged)
    Q_PROPERTY(QString zoneFilter READ zoneFilter WRITE setZoneFilter NOTIFY filterChanged)
    Q_PROPERTY(int dateFilter READ dateFilter WRITE setDateFilter NOTIFY filterChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY filterChanged)
public:
    enum Roles {
        AlertIdRole = Qt::UserRole + 1,
        TimestampRole,
        TimestampTextRole,
        DeviceIdRole,
        IdStringRole,
        ZoneRole,
        TypeRole,
        TypeLabelRole,
        ValueRole,
        AcknowledgedRole,
        FreshRole
    };

    explicit AlertListModel(NodeManager *manager, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int typeFilter() const { return typeFilter_; }
    void setTypeFilter(int v);
    QString zoneFilter() const { return zoneFilter_; }
    void setZoneFilter(const QString &v);
    int dateFilter() const { return dateFilter_; }
    void setDateFilter(int v);

    void handleAlertRaised(const AlertRecord &alert);
    void reload();

signals:
    void filterChanged();
    void alertInserted();

private:
    bool matches(const AlertRecord &a) const;
    QVector<AlertRecord> filtered() const;

    NodeManager *manager_ = nullptr;
    QVector<AlertRecord> rows_;
    int typeFilter_ = -1;   // -1 all, else EventType
    QString zoneFilter_;
    int dateFilter_ = 0;    // 0 all, 1 24h, 2 7d
};

} // namespace Slf

#endif
