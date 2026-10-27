#include "AuthController.h"

#include <QVariantMap>

namespace Slf {

AuthController::AuthController(Database *db, QObject *parent)
    : QObject(parent)
    , db_(db)
{
    lockTimer_.setInterval(1000);
    connect(&lockTimer_, &QTimer::timeout, this, [this]() {
        if (lockRemaining_ > 0)
            --lockRemaining_;
        if (lockRemaining_ <= 0) {
            lockRemaining_ = 0;
            failedAttempts_ = 0;
            lockTimer_.stop();
            setError(QString());
        }
        emit lockChanged();
    });
}

QString AuthController::roleLabel() const
{
    switch (role_) {
    case Role::Admin: return QStringLiteral("ADMIN");
    case Role::Guest: return QStringLiteral("GUEST · READ ONLY");
    default: return QStringLiteral("SIGNED OUT");
    }
}

bool AuthController::defaultAdminPassword() const
{
    return db_ && db_->usesDefaultPassword(QStringLiteral("admin"));
}

bool AuthController::login(const QString &username, const QString &password)
{
    if (locked()) {
        setError(QStringLiteral("Locked — wait %1 s").arg(lockRemaining_));
        emit loginRejected();
        return false;
    }
    if (!db_ || !db_->isOpen()) {
        setError(QStringLiteral("Local store unavailable"));
        emit loginRejected();
        return false;
    }
    if (username.trimmed().isEmpty()) {
        setError(QStringLiteral("Enter an operator name"));
        emit loginRejected();
        return false;
    }

    Role role = Role::None;
    QString error;
    if (!db_->authenticate(username, password, &role, &error)) {
        registerFailure();
        db_->audit(username.trimmed(), QStringLiteral("login-failed"), error);
        emit loginRejected();
        return false;
    }

    beginSession(username.trimmed(), role);
    return true;
}

bool AuthController::loginAsGuest()
{
    return loginAsRole(QStringLiteral("guest"));
}

bool AuthController::loginAsRole(const QString &name)
{
    if (locked()) {
        setError(QStringLiteral("Locked — wait %1 s").arg(lockRemaining_));
        emit loginRejected();
        return false;
    }

    const QString role = name.trimmed().toLower();
    if (role == QLatin1String("guest")) {
        beginSession(QStringLiteral("guest"), Role::Guest);
        return true;
    }
    if (role == QLatin1String("admin")) {
        beginSession(QStringLiteral("admin"), Role::Admin);
        return true;
    }

    setError(QStringLiteral("Type admin or guest"));
    emit loginRejected();
    return false;
}

void AuthController::logout()
{
    if (db_ && role_ != Role::None)
        db_->audit(username_, QStringLiteral("logout"));
    username_.clear();
    role_ = Role::None;
    setError(QString());
    emit sessionChanged();
    emit loggedOut();
}

void AuthController::clearError()
{
    setError(QString());
}

bool AuthController::changeOwnPassword(const QString &current, const QString &next, const QString &confirm)
{
    if (!db_ || role_ == Role::None) {
        setError(QStringLiteral("No active session"));
        return false;
    }
    if (next != confirm) {
        setError(QStringLiteral("New passwords do not match"));
        return false;
    }

    Role role = Role::None;
    if (!db_->authenticate(username_, current, &role, nullptr)) {
        setError(QStringLiteral("Current password is incorrect"));
        return false;
    }

    QString error;
    if (!db_->changePassword(username_, next, &error)) {
        setError(error);
        return false;
    }

    db_->audit(username_, QStringLiteral("password-changed"));
    setError(QString());
    emit usersChanged();
    return true;
}

bool AuthController::createUser(const QString &username, const QString &password, int role)
{
    if (!isAdmin()) {
        setError(QStringLiteral("Admin rights required"));
        return false;
    }
    if (password.size() < 4) {
        setError(QStringLiteral("Password must be at least 4 characters"));
        return false;
    }

    QString error;
    if (!db_->createUser(username, password, role == 2 ? Role::Admin : Role::Guest, &error)) {
        setError(error);
        return false;
    }

    db_->audit(username_, QStringLiteral("user-created"), username.trimmed());
    setError(QString());
    emit usersChanged();
    return true;
}

bool AuthController::removeUser(const QString &username)
{
    if (!isAdmin()) {
        setError(QStringLiteral("Admin rights required"));
        return false;
    }
    if (username.compare(username_, Qt::CaseInsensitive) == 0) {
        setError(QStringLiteral("You cannot remove the signed-in account"));
        return false;
    }

    QString error;
    if (!db_->deleteUser(username, &error)) {
        setError(error);
        return false;
    }

    db_->audit(username_, QStringLiteral("user-removed"), username.trimmed());
    setError(QString());
    emit usersChanged();
    return true;
}

bool AuthController::resetPassword(const QString &username, const QString &password)
{
    if (!isAdmin()) {
        setError(QStringLiteral("Admin rights required"));
        return false;
    }

    QString error;
    if (!db_->changePassword(username, password, &error)) {
        setError(error);
        return false;
    }

    db_->audit(username_, QStringLiteral("password-reset"), username.trimmed());
    setError(QString());
    emit usersChanged();
    return true;
}

QVariantList AuthController::userList() const
{
    QVariantList out;
    if (!db_)
        return out;
    const QVector<UserRecord> records = db_->users();
    for (const UserRecord &u : records) {
        QVariantMap m;
        m[QStringLiteral("username")] = u.username;
        m[QStringLiteral("role")] = static_cast<int>(u.role);
        m[QStringLiteral("roleLabel")] = u.role == Role::Admin ? QStringLiteral("Admin")
                                                              : QStringLiteral("Guest");
        m[QStringLiteral("lastLogin")] = u.lastLogin.isValid()
                                             ? u.lastLogin.toString(QStringLiteral("yyyy-MM-dd  HH:mm"))
                                             : QStringLiteral("never");
        m[QStringLiteral("defaultPassword")] = u.defaultPassword;
        out.append(m);
    }
    return out;
}

QVariantList AuthController::auditList(int limit) const
{
    QVariantList out;
    if (!db_)
        return out;
    const QVector<QVariantMap> rows = db_->recentAudit(limit);
    for (const QVariantMap &m : rows)
        out.append(m);
    return out;
}

void AuthController::setError(const QString &message)
{
    if (error_ == message)
        return;
    error_ = message;
    emit errorChanged();
}

void AuthController::beginSession(const QString &username, Role role)
{
    username_ = username;
    role_ = role;
    failedAttempts_ = 0;
    lockRemaining_ = 0;
    lockTimer_.stop();
    setError(QString());

    if (db_) {
        db_->touchLogin(username);
        db_->audit(username, QStringLiteral("login"), roleLabel());
    }

    emit lockChanged();
    emit sessionChanged();
    emit loggedIn(username_, static_cast<int>(role_));
}

void AuthController::registerFailure()
{
    ++failedAttempts_;
    if (failedAttempts_ >= kMaxAttempts) {
        lockRemaining_ = kLockSeconds;
        lockTimer_.start();
        setError(QStringLiteral("Too many attempts — locked for %1 s").arg(kLockSeconds));
        emit lockChanged();
        return;
    }
    setError(QStringLiteral("Unknown operator or wrong password (%1 of %2)")
                 .arg(failedAttempts_)
                 .arg(kMaxAttempts));
}

} // namespace Slf
