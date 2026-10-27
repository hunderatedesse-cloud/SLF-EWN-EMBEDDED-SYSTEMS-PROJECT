#ifndef NODELISTMODEL_H
#define NODELISTMODEL_H

#include "controller/NodeManager.h"

#include <QAbstractListModel>
#include <QDateTime>
#include <QVector>

namespace Slf {

class NodeListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        DeviceIdRole = Qt::UserRole + 1,
        IdStringRole,
        ZoneRole,
        ZoneIdRole,
        StatusRole,
        AcknowledgedRole,
        EverSeenRole,
        MoistureRole,
        TiltXRole,
        TiltYRole,
        TiltZRole,
        TiltMagRole,
        LastSeenRole,
        RelativeTimeRole,
        SparklineRole,
        MapXRole,
        MapYRole
    };

    explicit NodeListModel(NodeManager *manager, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setNow(const QDateTime &now);
    void handleNodeUpdated(quint16 id);
    void handleNodeRemoved(quint16 id);
    void refreshAll();

private:
    int indexOf(quint16 id) const;

    NodeManager *manager_ = nullptr;
    QVector<quint16> ids_;
    QDateTime now_ = QDateTime::currentDateTime();
};

} // namespace Slf

#endif
