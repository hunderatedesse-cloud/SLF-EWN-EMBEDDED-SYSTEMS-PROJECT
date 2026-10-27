#ifndef AUTHCONTROLLER_H
#define AUTHCONTROLLER_H

#include "model/Database.h"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>

namespace Slf {

// Session gate for the console. Admins may change configuration and
// acknowledge alarms; guests get a read-only view of the same live data.
class AuthController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool authenticated READ authenticated NOTIFY sessionChanged)
    Q_PROPERTY(QString username READ username NOTIFY sessionChanged)
    Q_PROPERTY(int role READ role NOTIFY sessionChanged)
    Q_PROPERTY(bool isAdmin READ isAdmin NOTIFY sessionChanged)
    Q_PROPERTY(bool isGuest READ isGuest NOTIFY sessionChanged)
    Q_PROPERTY(QString roleLabel READ roleLabel NOTIFY sessionChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorChanged)
    Q_PROPERTY(int lockRemaining READ lockRemaining NOTIFY lockChanged)
    Q_PROPERTY(bool locked READ locked NOTIFY lockChanged)
    Q_PROPERTY(bool defaultAdminPassword READ defaultAdminPassword NOTIFY usersChanged)
    Q_PROPERTY(QString storePath READ storePath CONSTANT)

public:
    explicit AuthController(Database *db, QObject *parent = nullptr);

    bool authenticated() const { return role_ != Role::None; }
    QString username() const { return username_; }
    int role() const { return static_cast<int>(role_); }
    bool isAdmin() const { return role_ == Role::Admin; }
    bool isGuest() const { return role_ == Role::Guest; }
    QString roleLabel() const;
    QString errorMessage() const { return error_; }
    int lockRemaining() const { return lockRemaining_; }
    bool locked() const { return lockRemaining_ > 0; }
    bool defaultAdminPassword() const;
    QString storePath() const { return db_ ? db_->path() : QString(); }

    Q_INVOKABLE bool login(const QString &username, const QString &password);
    Q_INVOKABLE bool loginAsGuest();
    Q_INVOKABLE bool loginAsRole(const QString &name);
    Q_INVOKABLE void logout();
    Q_INVOKABLE void clearError();

    Q_INVOKABLE bool changeOwnPassword(const QString &current, const QString &next, const QString &confirm);
    Q_INVOKABLE bool createUser(const QString &username, const QString &password, int role);
    Q_INVOKABLE bool removeUser(const QString &username);
    Q_INVOKABLE bool resetPassword(const QString &username, const QString &password);
    Q_INVOKABLE QVariantList userList() const;
    Q_INVOKABLE QVariantList auditList(int limit = 40) const;

signals:
    void sessionChanged();
    void errorChanged();
    void lockChanged();
    void usersChanged();
    void loginRejected();
    void loggedIn(QString username, int role);
    void loggedOut();

private:
    void setError(const QString &message);
    void beginSession(const QString &username, Role role);
    void registerFailure();

    Database *db_ = nullptr;
    QString username_;
    Role role_ = Role::None;
    QString error_;
    int failedAttempts_ = 0;
    int lockRemaining_ = 0;
    QTimer lockTimer_;

    static constexpr int kMaxAttempts = 5;
    static constexpr int kLockSeconds = 30;
};

} // namespace Slf

#endif
