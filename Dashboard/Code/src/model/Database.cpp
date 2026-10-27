#include "Database.h"

#include <QCryptographicHash>
#include <QDir>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariantMap>

#include <algorithm>

namespace Slf {

namespace {

constexpr int kPbkdf2Iterations = 60000;
constexpr int kKeyLength = 32;
constexpr int kFlushIntervalMs = 4000;
constexpr int kMaxPendingBeforeFlush = 200;
const char *kDefaultAdminPassword = "admin";
const char *kConnectionName = "slf-ewn-local";

int roleToInt(Role r) { return static_cast<int>(r); }

Role roleFromInt(int v)
{
    switch (v) {
    case 2: return Role::Admin;
    case 1: return Role::Guest;
    default: return Role::None;
    }
}

QDateTime fromMsecs(qint64 ms)
{
    return ms > 0 ? QDateTime::fromMSecsSinceEpoch(ms) : QDateTime();
}

QVariant realOrNull(double v)
{
    return qIsFinite(v) ? QVariant(v) : QVariant();
}

double fromSqlReal(const QVariant &v)
{
    if (!v.isValid() || v.isNull())
        return qQNaN();
    bool ok = false;
    const double d = v.toDouble(&ok);
    return ok ? d : qQNaN();
}

} // namespace

Database::Database(QObject *parent)
    : QObject(parent)
{
    flushTimer_.setInterval(kFlushIntervalMs);
    connect(&flushTimer_, &QTimer::timeout, this, &Database::flush);
}

Database::~Database()
{
    close();
}

bool Database::open(QString *errorOut)
{
    if (db_.isOpen())
        return true;

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) {
        lastError_ = QStringLiteral("No writable application data location");
        if (errorOut)
            *errorOut = lastError_;
        return false;
    }
    QDir().mkpath(dir);
    path_ = dir + QStringLiteral("/slfewn.db");

    db_ = QSqlDatabase::contains(QLatin1String(kConnectionName))
              ? QSqlDatabase::database(QLatin1String(kConnectionName))
              : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QLatin1String(kConnectionName));
    db_.setDatabaseName(path_);

    if (!db_.open()) {
        lastError_ = db_.lastError().text();
        if (errorOut)
            *errorOut = lastError_;
        return false;
    }

    QSqlQuery pragma(db_);
    pragma.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
    pragma.exec(QStringLiteral("PRAGMA synchronous = NORMAL"));
    pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"));

    if (!createSchema(errorOut))
        return false;

    seedDefaultPlaces();
    seedDefaultAccounts();

    QSqlQuery count(db_);
    if (count.exec(QStringLiteral("SELECT COUNT(*) FROM samples")) && count.next())
        sampleCount_ = count.value(0).toLongLong();

    flushTimer_.start();
    return true;
}

void Database::close()
{
    if (!db_.isOpen())
        return;
    flush();
    flushTimer_.stop();
    // Fold the write-ahead log back into the main file. Without this the .db on
    // disk stays a 4 KB stub while the real data sits in slfewn.db-wal, which
    // makes a working store look empty to anyone inspecting the folder.
    QSqlQuery checkpoint(db_);
    checkpoint.exec(QStringLiteral("PRAGMA wal_checkpoint(TRUNCATE)"));
    db_.close();
    db_ = QSqlDatabase();
    QSqlDatabase::removeDatabase(QLatin1String(kConnectionName));
}

bool Database::createSchema(QString *errorOut)
{
    const QStringList statements = {
        QStringLiteral("CREATE TABLE IF NOT EXISTS users ("
                       "username TEXT PRIMARY KEY COLLATE NOCASE,"
                       "role INTEGER NOT NULL,"
                       "salt TEXT NOT NULL,"
                       "hash TEXT NOT NULL,"
                       "created_at INTEGER NOT NULL,"
                       "last_login INTEGER)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS settings ("
                       "key TEXT PRIMARY KEY,"
                       "value TEXT)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS nodes ("
                       "device_id INTEGER PRIMARY KEY,"
                       "zone_id TEXT,"
                       "zone_label TEXT,"
                       "map_x REAL,"
                       "map_y REAL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS places ("
                       "id TEXT PRIMARY KEY,"
                       "name TEXT NOT NULL,"
                       "notes TEXT,"
                       "x REAL NOT NULL,"
                       "y REAL NOT NULL,"
                       "w REAL NOT NULL,"
                       "h REAL NOT NULL,"
                       "color TEXT)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS samples ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       "device_id INTEGER NOT NULL,"
                       "ts INTEGER NOT NULL,"
                       "moisture REAL,"
                       "tilt_x REAL,"
                       "tilt_y REAL,"
                       "tilt_z REAL,"
                       "tilt_mag REAL,"
                       "status INTEGER)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_samples_device_ts ON samples(device_id, ts)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS alerts ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       "ts INTEGER NOT NULL,"
                       "device_id INTEGER NOT NULL,"
                       "zone TEXT,"
                       "event_type INTEGER NOT NULL,"
                       "trigger_value TEXT,"
                       "acknowledged INTEGER NOT NULL DEFAULT 0,"
                       "acknowledged_by TEXT,"
                       "acknowledged_at INTEGER)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_alerts_ts ON alerts(ts)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS audit_log ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       "ts INTEGER NOT NULL,"
                       "username TEXT,"
                       "action TEXT NOT NULL,"
                       "detail TEXT)"),
    };

    for (const QString &sql : statements) {
        QSqlQuery q(db_);
        if (!q.exec(sql)) {
            lastError_ = q.lastError().text();
            if (errorOut)
                *errorOut = lastError_;
            return false;
        }
    }
    migrateColumns();
    return true;
}

void Database::ensureColumn(const QString &table, const QString &column, const QString &decl)
{
    QSqlQuery info(db_);
    if (!info.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table)))
        return;
    while (info.next()) {
        if (info.value(1).toString().compare(column, Qt::CaseInsensitive) == 0)
            return;
    }
    QSqlQuery alter(db_);
    alter.exec(QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3").arg(table, column, decl));
}

void Database::migrateColumns()
{
    ensureColumn(QStringLiteral("nodes"), QStringLiteral("latitude"), QStringLiteral("REAL"));
    ensureColumn(QStringLiteral("nodes"), QStringLiteral("longitude"), QStringLiteral("REAL"));
    ensureColumn(QStringLiteral("nodes"), QStringLiteral("altitude"), QStringLiteral("REAL"));
    ensureColumn(QStringLiteral("nodes"), QStringLiteral("m_warn"), QStringLiteral("REAL"));
    ensureColumn(QStringLiteral("nodes"), QStringLiteral("m_crit"), QStringLiteral("REAL"));
    ensureColumn(QStringLiteral("nodes"), QStringLiteral("t_warn"), QStringLiteral("REAL"));
    ensureColumn(QStringLiteral("nodes"), QStringLiteral("t_crit"), QStringLiteral("REAL"));
    ensureColumn(QStringLiteral("nodes"), QStringLiteral("use_custom_th"), QStringLiteral("INTEGER NOT NULL DEFAULT 0"));
    ensureColumn(QStringLiteral("nodes"), QStringLiteral("auto_assigned"), QStringLiteral("INTEGER NOT NULL DEFAULT 0"));
    ensureColumn(QStringLiteral("nodes"), QStringLiteral("manual_place"), QStringLiteral("INTEGER NOT NULL DEFAULT 0"));
    ensureColumn(QStringLiteral("nodes"), QStringLiteral("location_name"), QStringLiteral("TEXT"));
    ensureColumn(QStringLiteral("nodes"), QStringLiteral("description"), QStringLiteral("TEXT"));

    ensureColumn(QStringLiteral("places"), QStringLiteral("lat1"), QStringLiteral("REAL"));
    ensureColumn(QStringLiteral("places"), QStringLiteral("lon1"), QStringLiteral("REAL"));
    ensureColumn(QStringLiteral("places"), QStringLiteral("alt1"), QStringLiteral("REAL"));
    ensureColumn(QStringLiteral("places"), QStringLiteral("lat2"), QStringLiteral("REAL"));
    ensureColumn(QStringLiteral("places"), QStringLiteral("lon2"), QStringLiteral("REAL"));
    ensureColumn(QStringLiteral("places"), QStringLiteral("alt2"), QStringLiteral("REAL"));
}

void Database::seedDefaultAccounts()
{
    if (!userExists(QStringLiteral("admin")))
        createUser(QStringLiteral("admin"), QLatin1String(kDefaultAdminPassword), Role::Admin, nullptr);
    if (!userExists(QStringLiteral("guest")))
        createUser(QStringLiteral("guest"), QStringLiteral("guest"), Role::Guest, nullptr);
}

QString Database::makeSalt()
{
    QByteArray salt(16, '\0');
    auto *gen = QRandomGenerator::system();
    for (int i = 0; i < salt.size(); ++i)
        salt[i] = static_cast<char>(gen->bounded(256));
    return QString::fromLatin1(salt.toHex());
}

QString Database::hashPassword(const QString &password, const QString &salt)
{
    const QByteArray derived = QPasswordDigestor::deriveKeyPbkdf2(
        QCryptographicHash::Sha256,
        password.toUtf8(),
        QByteArray::fromHex(salt.toLatin1()),
        kPbkdf2Iterations,
        kKeyLength);
    return QString::fromLatin1(derived.toHex());
}

bool Database::authenticate(const QString &username, const QString &password, Role *roleOut, QString *errorOut)
{
    if (!db_.isOpen()) {
        if (errorOut)
            *errorOut = QStringLiteral("Local store unavailable");
        return false;
    }

    QSqlQuery q(db_);
    q.prepare(QStringLiteral("SELECT role, salt, hash FROM users WHERE username = ?"));
    q.addBindValue(username.trimmed());
    if (!q.exec() || !q.next()) {
        if (errorOut)
            *errorOut = QStringLiteral("Unknown operator or wrong password");
        return false;
    }

    const int role = q.value(0).toInt();
    const QString salt = q.value(1).toString();
    const QString stored = q.value(2).toString();

    if (hashPassword(password, salt) != stored) {
        if (errorOut)
            *errorOut = QStringLiteral("Unknown operator or wrong password");
        return false;
    }

    if (roleOut)
        *roleOut = roleFromInt(role);
    return true;
}

bool Database::createUser(const QString &username, const QString &password, Role role, QString *errorOut)
{
    const QString name = username.trimmed();
    if (name.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Username required");
        return false;
    }
    if (userExists(name)) {
        if (errorOut)
            *errorOut = QStringLiteral("That operator already exists");
        return false;
    }

    const QString salt = makeSalt();
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("INSERT INTO users (username, role, salt, hash, created_at)"
                             " VALUES (?, ?, ?, ?, ?)"));
    q.addBindValue(name);
    q.addBindValue(roleToInt(role));
    q.addBindValue(salt);
    q.addBindValue(hashPassword(password, salt));
    q.addBindValue(QDateTime::currentMSecsSinceEpoch());
    if (!q.exec()) {
        lastError_ = q.lastError().text();
        if (errorOut)
            *errorOut = lastError_;
        return false;
    }
    return true;
}

bool Database::changePassword(const QString &username, const QString &newPassword, QString *errorOut)
{
    if (newPassword.size() < 4) {
        if (errorOut)
            *errorOut = QStringLiteral("Password must be at least 4 characters");
        return false;
    }

    const QString salt = makeSalt();
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("UPDATE users SET salt = ?, hash = ? WHERE username = ?"));
    q.addBindValue(salt);
    q.addBindValue(hashPassword(newPassword, salt));
    q.addBindValue(username.trimmed());
    if (!q.exec() || q.numRowsAffected() <= 0) {
        lastError_ = q.lastError().text();
        if (errorOut)
            *errorOut = lastError_.isEmpty() ? QStringLiteral("No such operator") : lastError_;
        return false;
    }
    return true;
}

bool Database::deleteUser(const QString &username, QString *errorOut)
{
    const QString name = username.trimmed();
    if (name.compare(QStringLiteral("admin"), Qt::CaseInsensitive) == 0) {
        if (errorOut)
            *errorOut = QStringLiteral("The primary admin account cannot be removed");
        return false;
    }
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("DELETE FROM users WHERE username = ?"));
    q.addBindValue(name);
    if (!q.exec()) {
        lastError_ = q.lastError().text();
        if (errorOut)
            *errorOut = lastError_;
        return false;
    }
    return true;
}

bool Database::userExists(const QString &username)
{
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("SELECT 1 FROM users WHERE username = ?"));
    q.addBindValue(username.trimmed());
    return q.exec() && q.next();
}

bool Database::usesDefaultPassword(const QString &username)
{
    Role r = Role::None;
    if (username.compare(QStringLiteral("admin"), Qt::CaseInsensitive) == 0)
        return authenticate(username, QLatin1String(kDefaultAdminPassword), &r, nullptr);
    return false;
}

QVector<UserRecord> Database::users()
{
    QVector<UserRecord> out;
    QSqlQuery q(db_);
    if (!q.exec(QStringLiteral("SELECT username, role, created_at, last_login FROM users"
                               " ORDER BY role DESC, username ASC")))
        return out;
    while (q.next()) {
        UserRecord u;
        u.username = q.value(0).toString();
        u.role = roleFromInt(q.value(1).toInt());
        u.createdAt = fromMsecs(q.value(2).toLongLong());
        u.lastLogin = fromMsecs(q.value(3).toLongLong());
        u.defaultPassword = usesDefaultPassword(u.username);
        out.append(u);
    }
    return out;
}

void Database::touchLogin(const QString &username)
{
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("UPDATE users SET last_login = ? WHERE username = ?"));
    q.addBindValue(QDateTime::currentMSecsSinceEpoch());
    q.addBindValue(username.trimmed());
    q.exec();
}

QString Database::setting(const QString &key, const QString &fallback)
{
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("SELECT value FROM settings WHERE key = ?"));
    q.addBindValue(key);
    if (q.exec() && q.next())
        return q.value(0).toString();
    return fallback;
}

void Database::setSetting(const QString &key, const QString &value)
{
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("INSERT INTO settings (key, value) VALUES (?, ?)"
                             " ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    q.addBindValue(key);
    q.addBindValue(value);
    q.exec();
}

QVector<NodeProfile> Database::loadProfiles()
{
    QVector<NodeProfile> out;
    QSqlQuery q(db_);
    if (!q.exec(QStringLiteral("SELECT device_id, zone_id, zone_label, map_x, map_y,"
                               " latitude, longitude, altitude,"
                               " m_warn, m_crit, t_warn, t_crit,"
                               " use_custom_th, auto_assigned, manual_place,"
                               " location_name, description FROM nodes")))
        return out;
    while (q.next()) {
        NodeProfile p;
        p.deviceId = static_cast<quint16>(q.value(0).toUInt());
        p.zoneId = q.value(1).toString();
        p.zoneLabel = q.value(2).toString();
        p.mapPos = QPointF(q.value(3).toDouble(), q.value(4).toDouble());
        p.latitude = fromSqlReal(q.value(5));
        p.longitude = fromSqlReal(q.value(6));
        p.altitude = fromSqlReal(q.value(7));
        const double mw = fromSqlReal(q.value(8));
        const double mc = fromSqlReal(q.value(9));
        const double tw = fromSqlReal(q.value(10));
        const double tc = fromSqlReal(q.value(11));
        if (qIsFinite(mw))
            p.thresholds.moistureWarning = float(mw);
        if (qIsFinite(mc))
            p.thresholds.moistureCritical = float(mc);
        if (qIsFinite(tw))
            p.thresholds.tiltWarningDeg = float(tw);
        if (qIsFinite(tc))
            p.thresholds.tiltCriticalDeg = float(tc);
        p.useCustomThresholds = q.value(12).toInt() != 0;
        p.autoAssigned = q.value(13).toInt() != 0;
        p.manualPlace = q.value(14).toInt() != 0;
        p.locationName = q.value(15).toString();
        p.description = q.value(16).toString();
        out.append(p);
    }
    return out;
}

void Database::saveProfile(const NodeProfile &profile)
{
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("INSERT INTO nodes (device_id, zone_id, zone_label, map_x, map_y,"
                             " latitude, longitude, altitude,"
                             " m_warn, m_crit, t_warn, t_crit,"
                             " use_custom_th, auto_assigned, manual_place,"
                             " location_name, description)"
                             " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
                             " ON CONFLICT(device_id) DO UPDATE SET"
                             " zone_id = excluded.zone_id,"
                             " zone_label = excluded.zone_label,"
                             " map_x = excluded.map_x,"
                             " map_y = excluded.map_y,"
                             " latitude = excluded.latitude,"
                             " longitude = excluded.longitude,"
                             " altitude = excluded.altitude,"
                             " m_warn = excluded.m_warn,"
                             " m_crit = excluded.m_crit,"
                             " t_warn = excluded.t_warn,"
                             " t_crit = excluded.t_crit,"
                             " use_custom_th = excluded.use_custom_th,"
                             " auto_assigned = excluded.auto_assigned,"
                             " manual_place = excluded.manual_place,"
                             " location_name = excluded.location_name,"
                             " description = excluded.description"));
    q.addBindValue(profile.deviceId);
    q.addBindValue(profile.zoneId);
    q.addBindValue(profile.zoneLabel);
    q.addBindValue(profile.mapPos.x());
    q.addBindValue(profile.mapPos.y());
    q.addBindValue(realOrNull(profile.latitude));
    q.addBindValue(realOrNull(profile.longitude));
    q.addBindValue(realOrNull(profile.altitude));
    q.addBindValue(profile.thresholds.moistureWarning);
    q.addBindValue(profile.thresholds.moistureCritical);
    q.addBindValue(profile.thresholds.tiltWarningDeg);
    q.addBindValue(profile.thresholds.tiltCriticalDeg);
    q.addBindValue(profile.useCustomThresholds ? 1 : 0);
    q.addBindValue(profile.autoAssigned ? 1 : 0);
    q.addBindValue(profile.manualPlace ? 1 : 0);
    q.addBindValue(profile.locationName);
    q.addBindValue(profile.description);
    q.exec();
}

void Database::deleteProfile(quint16 deviceId)
{
    if (!db_.isOpen())
        return;
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("DELETE FROM nodes WHERE device_id = ?"));
    q.addBindValue(deviceId);
    q.exec();
}

QVector<ZoneDef> Database::loadPlaces()
{
    QVector<ZoneDef> out;
    if (!db_.isOpen())
        return out;
    QSqlQuery q(db_);
    if (!q.exec(QStringLiteral("SELECT id, name, notes, x, y, w, h, color,"
                               " lat1, lon1, alt1, lat2, lon2, alt2 FROM places ORDER BY name")))
        return out;
    while (q.next()) {
        ZoneDef z;
        z.id = q.value(0).toString();
        z.name = q.value(1).toString();
        z.notes = q.value(2).toString();
        z.bounds = QRectF(q.value(3).toDouble(), q.value(4).toDouble(),
                          q.value(5).toDouble(), q.value(6).toDouble());
        z.color = q.value(7).toString();
        z.lat1 = fromSqlReal(q.value(8));
        z.lon1 = fromSqlReal(q.value(9));
        z.alt1 = fromSqlReal(q.value(10));
        z.lat2 = fromSqlReal(q.value(11));
        z.lon2 = fromSqlReal(q.value(12));
        z.alt2 = fromSqlReal(q.value(13));
        out.append(z);
    }
    return out;
}

bool Database::savePlace(const ZoneDef &place)
{
    if (!db_.isOpen() || place.id.isEmpty() || place.name.trimmed().isEmpty())
        return false;
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("INSERT INTO places (id, name, notes, x, y, w, h, color,"
                             " lat1, lon1, alt1, lat2, lon2, alt2)"
                             " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
                             " ON CONFLICT(id) DO UPDATE SET"
                             " name = excluded.name,"
                             " notes = excluded.notes,"
                             " x = excluded.x, y = excluded.y,"
                             " w = excluded.w, h = excluded.h,"
                             " color = excluded.color,"
                             " lat1 = excluded.lat1, lon1 = excluded.lon1, alt1 = excluded.alt1,"
                             " lat2 = excluded.lat2, lon2 = excluded.lon2, alt2 = excluded.alt2"));
    q.addBindValue(place.id);
    q.addBindValue(place.name.trimmed());
    q.addBindValue(place.notes);
    q.addBindValue(place.bounds.x());
    q.addBindValue(place.bounds.y());
    q.addBindValue(place.bounds.width());
    q.addBindValue(place.bounds.height());
    q.addBindValue(place.color);
    q.addBindValue(realOrNull(place.lat1));
    q.addBindValue(realOrNull(place.lon1));
    q.addBindValue(realOrNull(place.alt1));
    q.addBindValue(realOrNull(place.lat2));
    q.addBindValue(realOrNull(place.lon2));
    q.addBindValue(realOrNull(place.alt2));
    if (!q.exec()) {
        lastError_ = q.lastError().text();
        return false;
    }
    return true;
}

bool Database::deletePlace(const QString &id)
{
    if (!db_.isOpen() || id.isEmpty())
        return false;
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("DELETE FROM places WHERE id = ?"));
    q.addBindValue(id);
    return q.exec();
}

void Database::seedDefaultPlaces()
{
    if (!db_.isOpen())
        return;
    QSqlQuery count(db_);
    if (count.exec(QStringLiteral("SELECT COUNT(*) FROM places")) && count.next() && count.value(0).toInt() > 0)
        return;

    const QVector<ZoneDef> defaults = {
        {QStringLiteral("north"), QStringLiteral("North Slope"), QStringLiteral("Mine Area A"), QRectF(0.06, 0.08, 0.42, 0.36), QStringLiteral("#3DDC97")},
        {QStringLiteral("south"), QStringLiteral("South Berm"),  QStringLiteral("Drainage B"),  QRectF(0.08, 0.52, 0.36, 0.38), QStringLiteral("#5B9BD5")},
        {QStringLiteral("east"),  QStringLiteral("East Face"),   QStringLiteral("Tunnel Adit C"), QRectF(0.54, 0.10, 0.40, 0.40), QStringLiteral("#C4A35A")},
        {QStringLiteral("west"),  QStringLiteral("West Cut"),    QStringLiteral("Stockpile D"), QRectF(0.50, 0.56, 0.28, 0.32), QStringLiteral("#8FA6B2")},
        {QStringLiteral("road"),  QStringLiteral("Access Road"), QStringLiteral("Culvert E"),   QRectF(0.80, 0.58, 0.16, 0.30), QStringLiteral("#4A9EFF")},
    };
    for (const ZoneDef &z : defaults)
        savePlace(z);
}

void Database::queueSample(quint16 deviceId, const Sample &sample, NodeStatus status)
{
    if (!db_.isOpen())
        return;
    PendingSample p;
    p.deviceId = deviceId;
    p.sample = sample;
    p.status = status;
    pending_.append(p);
    if (pending_.size() >= kMaxPendingBeforeFlush)
        flush();
}

void Database::flush()
{
    if (pending_.isEmpty() || !db_.isOpen())
        return;

    const QVector<PendingSample> batch = pending_;
    pending_.clear();

    db_.transaction();
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("INSERT INTO samples"
                             " (device_id, ts, moisture, tilt_x, tilt_y, tilt_z, tilt_mag, status)"
                             " VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    for (const PendingSample &p : batch) {
        q.addBindValue(p.deviceId);
        q.addBindValue(p.sample.timestamp.toMSecsSinceEpoch());
        q.addBindValue(p.sample.moisture);
        q.addBindValue(p.sample.tiltXDeg);
        q.addBindValue(p.sample.tiltYDeg);
        q.addBindValue(p.sample.tiltZDeg);
        q.addBindValue(p.sample.tiltMagDeg);
        q.addBindValue(static_cast<int>(p.status));
        if (q.exec()) {
            ++sampleCount_;
            ++rowsWritten_;
        } else {
            lastError_ = q.lastError().text();
        }
    }
    db_.commit();
    lastWrite_ = QDateTime::currentDateTime();
}

void Database::insertSamples(quint16 deviceId, const QVector<Sample> &samples)
{
    if (samples.isEmpty() || !db_.isOpen())
        return;

    db_.transaction();
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("INSERT INTO samples"
                             " (device_id, ts, moisture, tilt_x, tilt_y, tilt_z, tilt_mag, status)"
                             " VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    for (const Sample &s : samples) {
        q.addBindValue(deviceId);
        q.addBindValue(s.timestamp.toMSecsSinceEpoch());
        q.addBindValue(s.moisture);
        q.addBindValue(s.tiltXDeg);
        q.addBindValue(s.tiltYDeg);
        q.addBindValue(s.tiltZDeg);
        q.addBindValue(s.tiltMagDeg);
        q.addBindValue(static_cast<int>(NodeStatus::Unknown));
        if (q.exec()) {
            ++sampleCount_;
            ++rowsWritten_;
        } else {
            lastError_ = q.lastError().text();
        }
    }
    db_.commit();
    lastWrite_ = QDateTime::currentDateTime();
}

QVector<Sample> Database::loadSamples(quint16 deviceId, qint64 sinceMsecs, int limit)
{
    QVector<Sample> out;
    if (!db_.isOpen())
        return out;

    QSqlQuery q(db_);
    q.prepare(QStringLiteral("SELECT ts, moisture, tilt_x, tilt_y, tilt_z, tilt_mag FROM samples"
                             " WHERE device_id = ? AND ts >= ?"
                             " ORDER BY ts DESC LIMIT ?"));
    q.addBindValue(deviceId);
    q.addBindValue(sinceMsecs);
    q.addBindValue(limit);
    if (!q.exec())
        return out;

    while (q.next()) {
        Sample s;
        s.timestamp = QDateTime::fromMSecsSinceEpoch(q.value(0).toLongLong());
        s.moisture = q.value(1).toFloat();
        s.tiltXDeg = q.value(2).toFloat();
        s.tiltYDeg = q.value(3).toFloat();
        s.tiltZDeg = q.value(4).toFloat();
        s.tiltMagDeg = q.value(5).toFloat();
        out.append(s);
    }
    std::reverse(out.begin(), out.end());
    return out;
}

bool Database::hasSamples()
{
    QSqlQuery q(db_);
    return q.exec(QStringLiteral("SELECT 1 FROM samples LIMIT 1")) && q.next();
}

qint64 Database::sampleCount()
{
    return sampleCount_;
}

void Database::pruneOlderThan(int days)
{
    if (!db_.isOpen())
        return;
    const qint64 cutoff = QDateTime::currentDateTime().addDays(-days).toMSecsSinceEpoch();
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("DELETE FROM samples WHERE ts < ?"));
    q.addBindValue(cutoff);
    if (q.exec())
        sampleCount_ = qMax<qint64>(0, sampleCount_ - q.numRowsAffected());
}

qint64 Database::insertAlert(const AlertRecord &alert)
{
    if (!db_.isOpen())
        return -1;

    QSqlQuery q(db_);
    q.prepare(QStringLiteral("INSERT INTO alerts (ts, device_id, zone, event_type, trigger_value, acknowledged)"
                             " VALUES (?, ?, ?, ?, ?, 0)"));
    q.addBindValue(alert.timestamp.toMSecsSinceEpoch());
    q.addBindValue(alert.deviceId);
    q.addBindValue(alert.zone);
    q.addBindValue(static_cast<int>(alert.type));
    q.addBindValue(alert.triggerValue);
    if (!q.exec()) {
        lastError_ = q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toLongLong();
}

QVector<AlertRecord> Database::loadAlerts(int limit)
{
    QVector<AlertRecord> out;
    if (!db_.isOpen())
        return out;

    QSqlQuery q(db_);
    q.prepare(QStringLiteral("SELECT id, ts, device_id, zone, event_type, trigger_value, acknowledged"
                             " FROM alerts ORDER BY ts DESC, id DESC LIMIT ?"));
    q.addBindValue(limit);
    if (!q.exec())
        return out;

    while (q.next()) {
        AlertRecord a;
        a.id = q.value(0).toLongLong();
        a.timestamp = QDateTime::fromMSecsSinceEpoch(q.value(1).toLongLong());
        a.deviceId = static_cast<quint16>(q.value(2).toUInt());
        a.zone = q.value(3).toString();
        a.type = static_cast<EventType>(q.value(4).toInt());
        a.triggerValue = q.value(5).toString();
        a.acknowledged = q.value(6).toInt() != 0;
        out.append(a);
    }
    return out;
}

void Database::setAlertAcknowledged(qint64 alertId, const QString &by)
{
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("UPDATE alerts SET acknowledged = 1, acknowledged_by = ?, acknowledged_at = ?"
                             " WHERE id = ?"));
    q.addBindValue(by);
    q.addBindValue(QDateTime::currentMSecsSinceEpoch());
    q.addBindValue(alertId);
    q.exec();
}

void Database::acknowledgeDeviceAlerts(quint16 deviceId, const QString &by)
{
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("UPDATE alerts SET acknowledged = 1, acknowledged_by = ?, acknowledged_at = ?"
                             " WHERE device_id = ? AND acknowledged = 0"));
    q.addBindValue(by);
    q.addBindValue(QDateTime::currentMSecsSinceEpoch());
    q.addBindValue(deviceId);
    q.exec();
}

void Database::clearAlerts()
{
    QSqlQuery q(db_);
    q.exec(QStringLiteral("DELETE FROM alerts"));
}

void Database::audit(const QString &username, const QString &action, const QString &detail)
{
    if (!db_.isOpen())
        return;
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("INSERT INTO audit_log (ts, username, action, detail) VALUES (?, ?, ?, ?)"));
    q.addBindValue(QDateTime::currentMSecsSinceEpoch());
    q.addBindValue(username);
    q.addBindValue(action);
    q.addBindValue(detail);
    q.exec();
}

QVector<QVariantMap> Database::recentAudit(int limit)
{
    QVector<QVariantMap> out;
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("SELECT ts, username, action, detail FROM audit_log"
                             " ORDER BY ts DESC LIMIT ?"));
    q.addBindValue(limit);
    if (!q.exec())
        return out;
    while (q.next()) {
        QVariantMap m;
        m[QStringLiteral("timestamp")] =
            QDateTime::fromMSecsSinceEpoch(q.value(0).toLongLong())
                .toString(QStringLiteral("yyyy-MM-dd  HH:mm:ss"));
        m[QStringLiteral("username")] = q.value(1).toString();
        m[QStringLiteral("action")] = q.value(2).toString();
        m[QStringLiteral("detail")] = q.value(3).toString();
        out.append(m);
    }
    return out;
}

} // namespace Slf
