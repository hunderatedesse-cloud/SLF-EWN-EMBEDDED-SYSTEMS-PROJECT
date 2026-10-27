#ifndef DATABASE_H
#define DATABASE_H

#include "core/Types.h"

#include <QDateTime>
#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QTimer>
#include <QVector>

namespace Slf {

enum class Role {
    None = 0,
    Guest = 1,
    Admin = 2
};

struct UserRecord {
    QString username;
    Role role = Role::Guest;
    QDateTime createdAt;
    QDateTime lastLogin;
    bool defaultPassword = false;
};

struct PendingSample {
    quint16 deviceId = 0;
    Sample sample;
    NodeStatus status = NodeStatus::Unknown;
};

// Local store for telemetry history, alert journal, node profiles and operator
// accounts. Samples are buffered and flushed on a timer so a node reporting
// every few hundred milliseconds does not cause a disk write per packet.
class Database : public QObject
{
    Q_OBJECT
public:
    explicit Database(QObject *parent = nullptr);
    ~Database() override;

    bool open(QString *errorOut = nullptr);
    void close();
    bool isOpen() const { return db_.isOpen(); }
    QString path() const { return path_; }
    QString lastError() const { return lastError_; }

    // --- accounts ---
    bool authenticate(const QString &username, const QString &password, Role *roleOut, QString *errorOut);
    bool createUser(const QString &username, const QString &password, Role role, QString *errorOut);
    bool changePassword(const QString &username, const QString &newPassword, QString *errorOut);
    bool deleteUser(const QString &username, QString *errorOut);
    bool userExists(const QString &username);
    bool usesDefaultPassword(const QString &username);
    QVector<UserRecord> users();
    void touchLogin(const QString &username);

    // --- key/value settings ---
    QString setting(const QString &key, const QString &fallback = QString());
    void setSetting(const QString &key, const QString &value);

    // --- node profiles ---
    QVector<NodeProfile> loadProfiles();
    void saveProfile(const NodeProfile &profile);
    void deleteProfile(quint16 deviceId);

    // --- places / zones ---
    QVector<ZoneDef> loadPlaces();
    bool savePlace(const ZoneDef &place);
    bool deletePlace(const QString &id);
    void seedDefaultPlaces();

    // --- telemetry ---
    void queueSample(quint16 deviceId, const Sample &sample, NodeStatus status);
    void insertSamples(quint16 deviceId, const QVector<Sample> &samples);
    QVector<Sample> loadSamples(quint16 deviceId, qint64 sinceMsecs, int limit = 6000);
    bool hasSamples();
    qint64 sampleCount();
    void pruneOlderThan(int days);
    void flush();

    // Write telemetry so the operator can confirm the store is live rather than
    // having to trust that it is.
    QDateTime lastWrite() const { return lastWrite_; }
    qint64 rowsWrittenThisSession() const { return rowsWritten_; }
    int pendingWrites() const { return pending_.size(); }

    // --- alerts ---
    qint64 insertAlert(const AlertRecord &alert);
    QVector<AlertRecord> loadAlerts(int limit = 2000);
    void setAlertAcknowledged(qint64 alertId, const QString &by);
    void acknowledgeDeviceAlerts(quint16 deviceId, const QString &by);
    void clearAlerts();

    // --- audit trail ---
    void audit(const QString &username, const QString &action, const QString &detail = QString());
    QVector<QVariantMap> recentAudit(int limit = 60);

private:
    bool createSchema(QString *errorOut);
    void migrateColumns();
    void ensureColumn(const QString &table, const QString &column, const QString &decl);
    void seedDefaultAccounts();
    static QString makeSalt();
    static QString hashPassword(const QString &password, const QString &salt);

    QSqlDatabase db_;
    QString path_;
    QString lastError_;
    QVector<PendingSample> pending_;
    QTimer flushTimer_;
    // Cached so the UI can show store size without a COUNT(*) scan each tick.
    qint64 sampleCount_ = 0;
    QDateTime lastWrite_;
    qint64 rowsWritten_ = 0;
};

} // namespace Slf

#endif
