#include "AppController.h"

#include "core/PacketParser.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QLocale>
#include <QRectF>
#include <QSerialPortInfo>
#include <QTextStream>
#include <QUrl>

#include <algorithm>

namespace Slf {

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    detectFonts();

    QString dbError;
    if (db_.open(&dbError)) {
        storeStatus_ = QStringLiteral("SQLite · connected");
        db_.pruneOlderThan(30);
        manager_.attachDatabase(&db_);
    } else {
        storeStatus_ = QStringLiteral("SQLite unavailable — %1").arg(dbError);
    }

    auth_ = new AuthController(&db_, this);
    connect(auth_, &AuthController::loggedIn, this, [this](const QString &user, int) {
        manager_.setOperator(user);
        page_ = 0;
        emit pageChanged();
    });
    connect(auth_, &AuthController::loggedOut, this, [this]() {
        manager_.setOperator(QString());
        closeDetail();
    });

    nodes_ = new NodeListModel(&manager_, this);
    alerts_ = new AlertListModel(&manager_, this);

    // Replayed nodes already have history, so publish them before the first packet.
    for (quint16 id : manager_.nodeIds())
        nodes_->handleNodeUpdated(id);

    // Live UART is the default. An older default stored simulation=1; flip it
    // once so this build starts on the receiver instead of generated packets.
    if (db_.setting(QStringLiteral("app.liveFirst"), QStringLiteral("0")) != QLatin1String("1")) {
        db_.setSetting(QStringLiteral("app.liveFirst"), QStringLiteral("1"));
        db_.setSetting(QStringLiteral("app.simulation"), QStringLiteral("0"));
    }
    simulationEnabled_ = db_.setting(QStringLiteral("app.simulation"), QStringLiteral("0")) == QLatin1String("1");
    portName_ = db_.setting(QStringLiteral("app.port"));
    {
        const QString rid = db_.setting(QStringLiteral("rx.lastRid"));
        bool ok = false;
        const quint32 v = rid.toUInt(&ok);
        if (ok && !rid.isEmpty())
            serial_.setLastRecordId(v, true);
        lastDumpAt_ = QDateTime::fromString(db_.setting(QStringLiteral("rx.lastDumpAt")), Qt::ISODate);
    }

    connect(&sim_, &SimulatedTelemetrySource::packetReceived, &manager_, &NodeManager::ingest);
    connect(&serial_, &SerialPortReader::packetReceived, &manager_, &NodeManager::ingest);

    connect(&manager_, &NodeManager::nodeUpdated, this, [this](quint16 id, const NodeData &) {
        nodes_->handleNodeUpdated(id);
        emit countsChanged();
        emit mapNodesChanged();
        if (detailOpen_ && selectedId_ == int(id)) {
            emit selectedChanged();
            emit chartNeedsReload();
        }
    });
    connect(&manager_, &NodeManager::nodeRemoved, this, [this](quint16 id) {
        nodes_->handleNodeRemoved(id);
        if (selectedId_ == int(id))
            closeDetail();
        emit countsChanged();
        emit mapNodesChanged();
    });
    connect(&manager_, &NodeManager::alertRaised, this, [this](const AlertRecord &a) {
        alerts_->handleAlertRaised(a);
        emit countsChanged();
    });
    connect(&manager_, &NodeManager::alertsChanged, this, [this]() {
        alerts_->reload();
        nodes_->refreshAll();
        emit countsChanged();
        if (detailOpen_)
            emit selectedChanged();
    });
    connect(&manager_, &NodeManager::profilesChanged, this, [this]() {
        nodes_->refreshAll();
        emit mapNodesChanged();
        if (detailOpen_)
            emit selectedChanged();
    });
    connect(&manager_, &NodeManager::thresholdsChanged, this, [this](const Thresholds &) {
        emit thresholdsChanged();
        nodes_->refreshAll();
        emit mapNodesChanged();
    });
    connect(&manager_, &NodeManager::emergencyTriggered, this, [this](quint16 id) {
        Q_UNUSED(id);
        emit countsChanged();
    });

    connect(&serial_, &SerialPortReader::errorOccurred, this, [this](const QString &msg) {
        lastError_ = msg;
        emit connectionChanged();
    });
    // The port itself is the authority on whether the link is alive; a pulled
    // cable must not leave the dashboard showing a healthy connection.
    connect(&serial_, &SerialPortReader::connectionStateChanged, this, &AppController::handleSerialState);
    connect(&serial_, &SerialPortReader::statsChanged, this, [this]() { linkStatsDirty_ = true; });
    connect(&serial_, &SerialPortReader::dumpBusyChanged, this, [this]() { emit dumpStatsChanged(); });
    connect(&serial_, &SerialPortReader::traceChanged, this, [this]() {
        emit dumpStatsChanged();
        linkStatsDirty_ = true;
    });
    connect(&serial_, &SerialPortReader::statusReceived, this, [this](quint32 writeCount, quint32 freeRecords) {
        Q_UNUSED(freeRecords);
        emit dumpStatsChanged();
        emit linkStatsChanged();
        if (!handshakePending_)
            return;
        handshakePending_ = false;
        if (writeCount == 0)
            return;
        const bool longOffline = !lastDumpAt_.isValid()
                                 || lastDumpAt_.secsTo(QDateTime::currentDateTime()) > kLongOfflineSecs;
        if (!serial_.hasLastRecordId() || longOffline || serial_.lastRecordId() + 1 > writeCount)
            serial_.requestFullDump();
        else
            serial_.requestIncremental();
    });
    connect(&serial_, &SerialPortReader::dumpFinished, this, [this](int, quint32) {
        persistLastRid();
        emit dumpStatsChanged();
        emit linkStatsChanged();
    });
    connect(&serial_, &SerialPortReader::protocolError, this, [this](quint8) {
        if (handshakePending_) {
            handshakePending_ = false;
            if (serial_.isOpen())
                serial_.requestFullDump();
        }
        emit dumpStatsChanged();
    });

    // Ports appear and disappear while the app runs; rescan so a receiver
    // plugged in after launch shows up without restarting.
    portScanTimer_.setInterval(1500);
    connect(&portScanTimer_, &QTimer::timeout, this, &AppController::refreshPorts);
    portScanTimer_.start();

    // Byte-level stats change far faster than the UI needs; coalesce them.
    linkStatsTimer_.setInterval(400);
    connect(&linkStatsTimer_, &QTimer::timeout, this, [this]() {
        if (!linkStatsDirty_)
            return;
        linkStatsDirty_ = false;
        emit linkStatsChanged();
    });
    linkStatsTimer_.start();

    dumpPollTimer_.setInterval(4000);
    connect(&dumpPollTimer_, &QTimer::timeout, this, [this]() {
        if (!serial_.isOpen() || serial_.dumpBusy() || handshakePending_)
            return;
        if (connection_ != ConnectionState::Connected)
            return;
        serial_.requestIncremental();
    });
    dumpPollTimer_.start();

    // Buffered telemetry must reach disk even on a clean shutdown that does not
    // run this object's destructor first.
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
        db_.flush();
        db_.close();
    });

    clockTimer_.setInterval(1000);
    connect(&clockTimer_, &QTimer::timeout, this, [this]() {
        const QDateTime now = QDateTime::currentDateTime();
        clock_ = now.toString(QStringLiteral("yyyy-MM-dd   HH:mm:ss"));
        nodes_->setNow(now);
        emit clockChanged();
    });
    clockTimer_.start();
    clock_ = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd   HH:mm:ss"));

    storeStatsTimer_.setInterval(2000);
    connect(&storeStatsTimer_, &QTimer::timeout, this, &AppController::refreshStoreStats);
    storeStatsTimer_.start();

    seedHistory();
    refreshStoreStats();
    refreshPorts();
    if (simulationEnabled_ && !serial_.isOpen())
        startSimulation();
    else if (!serial_.isOpen())
        applyConnectionState(ConnectionState::Disconnected);
}

void AppController::setPage(int p)
{
    if (page_ == p)
        return;
    page_ = qBound(0, p, 3);
    emit pageChanged();
}

void AppController::setPortName(const QString &p)
{
    if (portName_ == p)
        return;
    portName_ = p;
    db_.setSetting(QStringLiteral("app.port"), p);
    emit connectionChanged();
    if (!simulationEnabled_ && !serial_.isOpen() && !userHeldDisconnect_)
        tryAutoConnect();
}

void AppController::setSimulationEnabled(bool on)
{
    if (simulationEnabled_ == on)
        return;
    if (!requireAdmin())
        return;
    simulationEnabled_ = on;
    db_.setSetting(QStringLiteral("app.simulation"), on ? QStringLiteral("1") : QStringLiteral("0"));

    if (on) {
        // Simulation is an explicit demo mode — it replaces the live feed so
        // generated packets cannot mix with receiver data.
        if (serial_.isOpen())
            serial_.close();
        startSimulation();
    } else {
        stopSimulation();
        userHeldDisconnect_ = false;
        applyConnectionState(ConnectionState::Disconnected);
        tryAutoConnect();
    }
    emit simulationChanged();
    emit connectionChanged();
}

int AppController::nodeCount() const
{
    return manager_.nodeCount();
}

int AppController::alertCount() const
{
    return manager_.unacknowledgedCount();
}

QVariantMap AppController::selectedNode() const
{
    QVariantMap m;
    if (!detailOpen_ || selectedId_ <= 0)
        return m;
    const NodeState n = manager_.node(static_cast<quint16>(selectedId_));
    m["deviceId"] = int(n.profile.deviceId);
    m["idString"] = deviceIdString(n.profile.deviceId);
    m["zone"] = n.profile.zoneLabel;
    m["status"] = int(n.status);
    m["acknowledged"] = n.acknowledged;
    m["moisture"] = n.latest.moisturePercent();
    m["tiltX"] = n.latest.tiltXDeg();
    m["tiltY"] = n.latest.tiltYDeg();
    m["tiltZ"] = n.latest.tiltZDeg();
    m["tiltMag"] = n.latest.tiltMagnitudeDeg();
    m["relativeTime"] = relativeTime(n.latest.timestamp, QDateTime::currentDateTime());
    m["everSeen"] = n.everSeen;
    m["hasGeo"] = n.profile.hasGeo();
    m["latitude"] = n.profile.latitude;
    m["longitude"] = n.profile.longitude;
    m["altitude"] = n.profile.altitude;
    const Thresholds th = manager_.thresholdsFor(n.profile.deviceId);
    m["customThresholds"] = n.profile.useCustomThresholds;
    m["moistureWarning"] = th.moistureWarning;
    m["moistureCritical"] = th.moistureCritical;
    m["tiltWarning"] = th.tiltWarningDeg;
    m["tiltCritical"] = th.tiltCriticalDeg;
    m["autoAssigned"] = n.profile.autoAssigned;
    m["zoneId"] = n.profile.zoneId;
    m["locationName"] = n.profile.locationName;
    m["description"] = n.profile.description;
    return m;
}

QVariantList AppController::mapNodes() const
{
    QVariantList out;
    for (quint16 id : manager_.nodeIds()) {
        const NodeState n = manager_.node(id);
        QVariantMap m;
        m["deviceId"] = int(id);
        m["idString"] = deviceIdString(id);
        m["zone"] = n.profile.zoneLabel;
        m["status"] = int(n.everSeen ? n.status : NodeStatus::Unknown);
        m["acknowledged"] = n.acknowledged;
        m["x"] = n.profile.mapPos.x();
        m["y"] = n.profile.mapPos.y();
        m["zoneId"] = n.profile.zoneId;
        m["everSeen"] = n.everSeen;
        m["moisture"] = n.latest.moisturePercent();
        m["tiltMag"] = n.latest.tiltMagnitudeDeg();
        m["assigned"] = !n.profile.zoneId.isEmpty() && n.profile.zoneId != QLatin1String("none");
        m["hasGeo"] = n.profile.hasGeo();
        m["latitude"] = n.profile.latitude;
        m["longitude"] = n.profile.longitude;
        m["altitude"] = n.profile.altitude;
        const Thresholds th = manager_.thresholdsFor(id);
        m["customThresholds"] = n.profile.useCustomThresholds;
        m["moistureWarning"] = th.moistureWarning;
        m["moistureCritical"] = th.moistureCritical;
        m["tiltWarning"] = th.tiltWarningDeg;
        m["tiltCritical"] = th.tiltCriticalDeg;
        m["autoAssigned"] = n.profile.autoAssigned;
        m["locationName"] = n.profile.locationName;
        m["description"] = n.profile.description;
        out.append(m);
    }
    return out;
}

QVariantList AppController::zones() const
{
    QVariantList out;
    for (const ZoneDef &z : manager_.zones()) {
        QVariantMap m;
        m["id"] = z.id;
        m["name"] = z.name;
        m["notes"] = z.notes;
        m["x"] = z.bounds.x();
        m["y"] = z.bounds.y();
        m["w"] = z.bounds.width();
        m["h"] = z.bounds.height();
        m["color"] = z.color.isEmpty() ? QStringLiteral("#5C6573") : z.color;
        m["lat1"] = z.lat1;
        m["lon1"] = z.lon1;
        m["alt1"] = z.alt1;
        m["lat2"] = z.lat2;
        m["lon2"] = z.lon2;
        m["alt2"] = z.alt2;
        m["hasCoverage"] = z.hasCoverage();
        m["coverage"] = z.coverageLabel();
        out.append(m);
    }
    return out;
}

QStringList AppController::zoneNames() const
{
    QStringList names;
    names.append(QString());
    for (const ZoneDef &z : manager_.zones())
        names.append(z.name);
    return names;
}

float AppController::moistureWarning() const { return manager_.thresholds().moistureWarning; }
float AppController::moistureCritical() const { return manager_.thresholds().moistureCritical; }
float AppController::tiltWarning() const { return manager_.thresholds().tiltWarningDeg; }
float AppController::tiltCritical() const { return manager_.thresholds().tiltCriticalDeg; }

void AppController::setMoistureWarning(float v)
{
    if (!requireAdmin())
        return;
    Thresholds t = manager_.thresholds();
    t.moistureWarning = v;
    manager_.setThresholds(t);
}
void AppController::setMoistureCritical(float v)
{
    if (!requireAdmin())
        return;
    Thresholds t = manager_.thresholds();
    t.moistureCritical = v;
    manager_.setThresholds(t);
}
void AppController::setTiltWarning(float v)
{
    if (!requireAdmin())
        return;
    Thresholds t = manager_.thresholds();
    t.tiltWarningDeg = v;
    manager_.setThresholds(t);
}
void AppController::setTiltCritical(float v)
{
    if (!requireAdmin())
        return;
    Thresholds t = manager_.thresholds();
    t.tiltCriticalDeg = v;
    manager_.setThresholds(t);
}

bool AppController::requireAdmin()
{
    if (auth_ && auth_->isAdmin())
        return true;
    emit permissionDenied();
    return false;
}

void AppController::refreshStoreStats()
{
    if (!db_.isOpen())
        return;
    storedSamples_ = static_cast<int>(db_.sampleCount());
    storedAlerts_ = manager_.alerts().size();
    emit storeChanged();
}

void AppController::flushStore()
{
    db_.flush();
    refreshStoreStats();
}

void AppController::copyToClipboard(const QString &text)
{
    if (QClipboard *cb = QGuiApplication::clipboard())
        cb->setText(text);
}

QString AppController::storeLastWrite() const
{
    const QDateTime t = db_.lastWrite();
    if (!t.isValid())
        return QStringLiteral("no writes yet");
    return relativeTime(t, QDateTime::currentDateTime());
}

QString AppController::storeSize() const
{
    const QFileInfo fi(db_.path());
    if (!fi.exists())
        return QStringLiteral("—");
    return QLocale().formattedDataSize(fi.size(), 1, QLocale::DataSizeTraditionalFormat);
}

QString AppController::rxLastFrame() const
{
    const QDateTime t = serial_.lastFrameAt();
    if (!t.isValid())
        return QStringLiteral("never");
    return relativeTime(t, QDateTime::currentDateTime());
}

QString AppController::sourceLabel() const
{
    if (connection_ == ConnectionState::Connected)
        return QStringLiteral("Live serial · %1 · real-time").arg(portName_);
    if (connection_ == ConnectionState::Connecting)
        return QStringLiteral("Opening %1…").arg(portName_);
    if (connection_ == ConnectionState::Simulated)
        return QStringLiteral("Simulated feed — not hardware data");
    if (connection_ == ConnectionState::Failed)
        return lastError_.isEmpty() ? QStringLiteral("Live link failed") : lastError_;
    return QStringLiteral("Waiting for receiver on USB-UART");
}

// Turns the raw counters into the one sentence an operator actually needs.
QString AppController::linkDiagnosis() const
{
    if (connection_ == ConnectionState::Simulated)
        return QStringLiteral("Showing generated test data. Connect a port to read the receiver.");
    if (connection_ != ConnectionState::Connected)
        return lastError_.isEmpty() ? QStringLiteral("Port closed.") : lastError_;

    if (serial_.dumpBusy()) {
        const QString s = serial_.dumpStatus();
        return s.isEmpty() ? QStringLiteral("Waiting for a framed reply from the receiver…") : s;
    }
    if (serial_.bytesReceived() == 0)
        return QStringLiteral("Port open but no bytes received — check wiring, baud rate and that the receiver is transmitting.");
    if (serial_.framesDecoded() == 0)
        return QStringLiteral("Receiving bytes but no 0xAA-framed reply yet — press Refresh to send FULL_DUMP (0x44). "
                              "The receiver only answers commands.");
    if (serial_.discardedBytes() > serial_.framesDecoded() * kPacketSize)
        return QStringLiteral("Frames decoding, but many bytes are being dropped — check for noise or a mismatched baud rate.");
    return QStringLiteral("Link healthy — frames decoding and being stored.");
}

void AppController::refreshPorts()
{
    QList<QSerialPortInfo> infos = QSerialPortInfo::availablePorts();
    std::sort(infos.begin(), infos.end(), [](const QSerialPortInfo &a, const QSerialPortInfo &b) {
        const int sa = portPreference(a);
        const int sb = portPreference(b);
        if (sa != sb)
            return sa > sb;
        return a.portName() < b.portName();
    });

    QStringList found;
    QStringList labels;
    found.reserve(infos.size());
    labels.reserve(infos.size());
    for (const QSerialPortInfo &info : infos) {
        found.append(info.portName());
        const QString desc = info.description().trimmed();
        labels.append(desc.isEmpty() ? info.portName()
                                     : QStringLiteral("%1  ·  %2").arg(info.portName(), desc));
    }

    const bool changed = (found != ports_);
    ports_ = found;
    portLabels_ = labels;

    if (!portName_.isEmpty() && !ports_.contains(portName_) && !ports_.isEmpty()) {
        // Previously selected port disappeared; pick the highest-ranked remaining one.
        portName_ = ports_.first();
        db_.setSetting(QStringLiteral("app.port"), portName_);
        emit connectionChanged();
    } else if (portName_.isEmpty() && !ports_.isEmpty()) {
        portName_ = ports_.first();
        db_.setSetting(QStringLiteral("app.port"), portName_);
        emit connectionChanged();
    }

    if (changed)
        emit portsChanged();

    if (simulationEnabled_ || serial_.isOpen() || connection_ == ConnectionState::Connecting)
        return;
    if (userHeldDisconnect_ && !changed)
        return;
    if (ports_.isEmpty())
        return;
    if (connection_ == ConnectionState::Failed && !changed)
        return;
    tryAutoConnect();
}

int AppController::portPreference(const QSerialPortInfo &info)
{
    const QString hay = (info.description() + QLatin1Char(' ') + info.manufacturer()).toLower();
    int score = 0;
    if (hay.contains(QLatin1String("ch340")) || hay.contains(QLatin1String("ch341"))
        || hay.contains(QLatin1String("cp210")) || hay.contains(QLatin1String("ft232"))
        || hay.contains(QLatin1String("ftdi")) || hay.contains(QLatin1String("wch"))
        || hay.contains(QLatin1String("silicon labs")) || hay.contains(QLatin1String("arduino"))
        || hay.contains(QLatin1String("usb serial")) || hay.contains(QLatin1String("usb-serial"))
        || hay.contains(QLatin1String("usb uart")))
        score += 100;
    if (hay.contains(QLatin1String("bluetooth")) || hay.contains(QLatin1String("standard serial")))
        score -= 40;
    return score;
}

void AppController::openSerialNow(bool userInitiated)
{
    if (portName_.isEmpty() && !ports_.isEmpty())
        setPortName(ports_.first());
    if (portName_.isEmpty()) {
        lastError_ = QStringLiteral("No serial port available — plug in the receiver, then press Rescan");
        applyConnectionState(ConnectionState::Failed);
        if (userInitiated)
            emit serialShake();
        emit linkStatsChanged();
        return;
    }

    stopSimulation();
    applyConnectionState(ConnectionState::Connecting);

    if (serial_.open(portName_)) {
        lastError_.clear();
        userHeldDisconnect_ = false;
        applyConnectionState(ConnectionState::Connected);
    } else {
        lastError_ = serial_.lastError();
        applyConnectionState(ConnectionState::Failed);
        if (userInitiated)
            emit serialShake();
        if (simulationEnabled_)
            startSimulation();
    }
    emit linkStatsChanged();
}

void AppController::tryAutoConnect()
{
    if (simulationEnabled_ || serial_.isOpen() || userHeldDisconnect_)
        return;
    if (connection_ == ConnectionState::Connecting)
        return;
    if (ports_.isEmpty())
        return;
    if (!portName_.isEmpty() && !ports_.contains(portName_))
        setPortName(ports_.first());
    openSerialNow();
}

void AppController::connectSerial()
{
    if (!requireAdmin())
        return;
    userHeldDisconnect_ = false;
    refreshPorts();
    openSerialNow(true);
}

void AppController::disconnectSerial()
{
    if (!requireAdmin())
        return;
    userHeldDisconnect_ = true;
    serial_.close();
    if (simulationEnabled_)
        startSimulation();
    else
        applyConnectionState(ConnectionState::Disconnected);
    emit linkStatsChanged();
}

// Reacts to the port dropping out on its own (unplug, driver reset, read error).
void AppController::beginReceiverSession()
{
    if (!serial_.isOpen())
        return;
    handshakePending_ = true;
    serial_.requestStatus();
}

void AppController::persistLastRid()
{
    if (serial_.hasLastRecordId()) {
        lastDumpAt_ = QDateTime::currentDateTime();
        db_.setSetting(QStringLiteral("rx.lastRid"), QString::number(serial_.lastRecordId()));
        db_.setSetting(QStringLiteral("rx.lastDumpAt"), lastDumpAt_.toString(Qt::ISODate));
    } else {
        lastDumpAt_ = QDateTime();
        db_.setSetting(QStringLiteral("rx.lastRid"), QString());
        db_.setSetting(QStringLiteral("rx.lastDumpAt"), QString());
    }
}

void AppController::refreshFromReceiver()
{
    if (!serial_.isOpen()) {
        lastError_ = QStringLiteral("Connect the receiver before refreshing");
        emit connectionChanged();
        emit serialShake();
        return;
    }
    handshakePending_ = false;
    serial_.requestFullDump();
    emit dumpStatsChanged();
}

void AppController::clearSerialTrace()
{
    serial_.clearTrace();
    emit dumpStatsChanged();
}

void AppController::clearReceiverLog()
{
    if (!requireAdmin())
        return;
    if (!serial_.isOpen()) {
        lastError_ = QStringLiteral("Connect the receiver before clearing its log");
        emit connectionChanged();
        emit serialShake();
        return;
    }
    handshakePending_ = false;
    serial_.requestClear();
    emit dumpStatsChanged();
}

int AppController::parseDeviceId(const QString &text) const
{
    QString s = text.trimmed().toUpper();
    if (s.startsWith(QLatin1String("NODE")))
        s = s.mid(4);
    if (s.startsWith(QLatin1String("0X")))
        s = s.mid(2);
    bool ok = false;
    uint v = s.toUInt(&ok, 10);
    if (!ok)
        v = s.toUInt(&ok, 16);
    if (!ok || v == 0 || v > 0xFFFF)
        return 0;
    return int(v);
}

bool AppController::addNodeAt(int deviceId, double lat, double lon)
{
    if (!requireAdmin())
        return false;
    if (deviceId <= 0 || deviceId > 0xFFFF || !geoValid(lat, lon))
        return false;
    manager_.setNodeGeo(static_cast<quint16>(deviceId), lat, lon, qQNaN());
    return true;
}

void AppController::handleSerialState(ConnectionState s)
{
    if (s == ConnectionState::Connected) {
        applyConnectionState(s);
        QTimer::singleShot(2000, this, [this]() {
            if (serial_.isOpen() && connection_ == ConnectionState::Connected)
                beginReceiverSession();
        });
        return;
    }
    if (s != ConnectionState::Failed && s != ConnectionState::Disconnected)
        return;
    handshakePending_ = false;
    if (connection_ != ConnectionState::Connected && connection_ != ConnectionState::Connecting)
        return;

    applyConnectionState(s);
    if (simulationEnabled_)
        startSimulation();
    emit linkStatsChanged();
}

void AppController::openDetail(int deviceId)
{
    selectedId_ = deviceId;
    detailOpen_ = true;
    emit selectedChanged();
    emit chartNeedsReload();
}

void AppController::closeDetail()
{
    detailOpen_ = false;
    emit selectedChanged();
}

void AppController::acknowledgeSelected()
{
    if (!requireAdmin())
        return;
    if (selectedId_ > 0)
        manager_.acknowledgeNode(static_cast<quint16>(selectedId_));
}

void AppController::acknowledgeAlert(int alertId)
{
    if (!requireAdmin())
        return;
    manager_.acknowledgeAlert(alertId);
}

void AppController::acknowledgeNode(int deviceId)
{
    if (!requireAdmin())
        return;
    manager_.acknowledgeNode(static_cast<quint16>(deviceId));
}

void AppController::clearAlerts()
{
    if (!requireAdmin())
        return;
    manager_.clearAlerts();
    refreshStoreStats();
}

bool AppController::exportAlerts(const QUrl &url)
{
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream ts(&f);
    if (db_.isOpen())
        db_.audit(auth_ ? auth_->username() : QString(), QStringLiteral("export-csv"), path);

    ts << "Timestamp,DeviceID,Zone,EventType,Value,Acknowledged\n";
    for (const AlertRecord &a : manager_.alerts()) {
        QString zone = a.zone;
        zone.replace(QLatin1Char('"'), QStringLiteral("''"));
        ts << a.timestamp.toString(Qt::ISODate) << ','
           << deviceIdString(a.deviceId) << ','
           << '"' << zone << '"' << ','
           << eventTypeLabel(a.type) << ','
           << '"' << a.triggerValue << '"' << ','
           << (a.acknowledged ? "yes" : "no") << '\n';
    }
    return true;
}

void AppController::setZoneLabel(int deviceId, const QString &label)
{
    if (!requireAdmin())
        return;
    manager_.setZoneLabel(static_cast<quint16>(deviceId), label);
}

void AppController::assignNode(int deviceId, const QString &placeId)
{
    if (!requireAdmin())
        return;
    manager_.assignNode(static_cast<quint16>(deviceId), placeId, true);
}

void AppController::deleteNode(int deviceId)
{
    if (!requireAdmin())
        return;
    if (selectedId_ == deviceId)
        closeDetail();
    manager_.removeNode(static_cast<quint16>(deviceId));
}

void AppController::setNodeGeo(int deviceId, double lat, double lon, double alt)
{
    if (!requireAdmin())
        return;
    manager_.setNodeGeo(static_cast<quint16>(deviceId), lat, lon, alt);
}

void AppController::moveNodeOnMap(int deviceId, double lat, double lon, const QString &placeId)
{
    if (!requireAdmin())
        return;
    if (deviceId <= 0 || deviceId > 0xFFFF || !geoValid(lat, lon))
        return;
    manager_.moveNodeOnMap(static_cast<quint16>(deviceId), lat, lon, placeId);
}

void AppController::updateNodeMeta(int deviceId, const QString &locationName, const QString &description)
{
    if (!requireAdmin())
        return;
    manager_.setNodeMeta(static_cast<quint16>(deviceId), locationName, description);
    if (selectedId_ == deviceId)
        emit selectedChanged();
}

QString AppController::createPlaceCoverage(const QString &name, const QString &notes,
                                           double lat1, double lon1, double lat2, double lon2)
{
    return createPlace(name, notes, 0.08f, 0.08f, 0.28f, 0.24f,
                       lat1, lon1, qQNaN(), lat2, lon2, qQNaN());
}

bool AppController::updatePlaceInfo(const QString &id, const QString &name, const QString &notes)
{
    if (!requireAdmin())
        return false;
    ZoneDef z = manager_.place(id);
    if (z.id.isEmpty())
        return false;
    z.name = name;
    z.notes = notes;
    return manager_.upsertPlace(z);
}

void AppController::setNodeThresholds(int deviceId, bool custom, float moistureWarning, float moistureCritical,
                                      float tiltWarning, float tiltCritical)
{
    if (!requireAdmin())
        return;
    Thresholds t;
    t.moistureWarning = moistureWarning;
    t.moistureCritical = moistureCritical;
    t.tiltWarningDeg = tiltWarning;
    t.tiltCriticalDeg = tiltCritical;
    manager_.setNodeThresholds(static_cast<quint16>(deviceId), custom, t);
}

QString AppController::createPlace(const QString &name, const QString &notes, float x, float y, float w, float h,
                                   double lat1, double lon1, double alt1, double lat2, double lon2, double alt2)
{
    if (!requireAdmin())
        return {};
    ZoneDef z;
    z.id = QStringLiteral("place-%1").arg(QDateTime::currentMSecsSinceEpoch());
    z.name = name;
    z.notes = notes;
    z.bounds = QRectF(x, y, w, h);
    z.color = QStringLiteral("#4A9EFF");
    z.lat1 = lat1;
    z.lon1 = lon1;
    z.alt1 = alt1;
    z.lat2 = lat2;
    z.lon2 = lon2;
    z.alt2 = alt2;
    if (!manager_.upsertPlace(z))
        return {};
    return z.id;
}

bool AppController::updatePlace(const QString &id, const QString &name, const QString &notes, float x, float y, float w, float h,
                                double lat1, double lon1, double alt1, double lat2, double lon2, double alt2)
{
    if (!requireAdmin())
        return false;
    ZoneDef z = manager_.place(id);
    if (z.id.isEmpty())
        z.id = id;
    z.name = name;
    z.notes = notes;
    z.bounds = QRectF(x, y, w, h);
    z.lat1 = lat1;
    z.lon1 = lon1;
    z.alt1 = alt1;
    z.lat2 = lat2;
    z.lon2 = lon2;
    z.alt2 = alt2;
    return manager_.upsertPlace(z);
}

bool AppController::deletePlace(const QString &id)
{
    if (!requireAdmin())
        return false;
    return manager_.removePlace(id);
}

void AppController::setNodeMapPos(int deviceId, float x, float y)
{
    if (!requireAdmin())
        return;
    manager_.setMapPos(static_cast<quint16>(deviceId), QPointF(x, y));
}

QVariantList AppController::moistureSeries(int range) const
{
    return seriesOf(static_cast<quint16>(selectedId_), range, 0);
}

QVariantList AppController::tiltSeries(int range) const
{
    return seriesOf(static_cast<quint16>(selectedId_), range, 1);
}

QString AppController::statusLabel(int status) const
{
    return Slf::statusLabel(static_cast<NodeStatus>(status));
}

QString AppController::statusColor(int status) const
{
    switch (static_cast<NodeStatus>(status)) {
    case NodeStatus::Safe: return QStringLiteral("#3DDC97");
    case NodeStatus::Warning: return QStringLiteral("#F0A63A");
    case NodeStatus::Critical: return QStringLiteral("#E4483C");
    default: return QStringLiteral("#5C6573");
    }
}

int AppController::probeStatus(float moisture, float tilt) const
{
    const Thresholds t = manager_.thresholds();
    if (moisture >= t.moistureCritical || tilt >= t.tiltCriticalDeg)
        return int(NodeStatus::Critical);
    if (moisture >= t.moistureWarning || tilt >= t.tiltWarningDeg)
        return int(NodeStatus::Warning);
    return int(NodeStatus::Safe);
}

void AppController::applyConnectionState(ConnectionState s)
{
    connection_ = s;
    emit connectionChanged();
}

void AppController::startSimulation()
{
    sim_.start();
    applyConnectionState(ConnectionState::Simulated);
}

void AppController::stopSimulation()
{
    sim_.stop();
}

void AppController::seedHistory()
{
    // Real deployments replay from SQLite. Only a first run with no stored
    // telemetry gets a synthetic 24 h backfill, which is then persisted so the
    // charts stay continuous across restarts.
    if (db_.isOpen() && db_.hasSamples())
        return;
    if (!simulationEnabled_)
        return;

    const QDateTime now = QDateTime::currentDateTime();
    for (quint16 id : manager_.nodeIds()) {
        const QVector<Sample> history = sim_.makeHistory(id, now, 60 * 24);
        manager_.seedHistory(id, history);
        if (db_.isOpen())
            db_.insertSamples(id, history);
    }
}

TimeRange AppController::toRange(int range) const
{
    if (range == 1)
        return TimeRange::Days7;
    if (range == 2)
        return TimeRange::Days30;
    return TimeRange::Hours24;
}

QVariantList AppController::seriesOf(quint16 id, int range, int field) const
{
    QVariantList out;
    if (id == 0)
        return out;
    const QVector<Sample> samples = manager_.samplesInRange(id, toRange(range));
    const int step = qMax(1, samples.size() / 700);
    out.reserve(samples.size() / step + 4);
    for (int i = 0; i < samples.size(); i += step) {
        const Sample &s = samples.at(i);
        QVariantMap p;
        p["t"] = s.timestamp.toMSecsSinceEpoch();
        if (field == 0) {
            p["v"] = s.moisture;
        } else {
            p["x"] = s.tiltXDeg;
            p["y"] = s.tiltYDeg;
            p["z"] = s.tiltZDeg;
            p["m"] = s.tiltMagDeg;
        }
        out.append(p);
    }
    return out;
}

void AppController::detectFonts()
{
    const QStringList families = QFontDatabase::families();
    auto pick = [&](const QStringList &wanted, const QString &fallback) {
        for (const QString &w : wanted) {
            for (const QString &s : families) {
                if (s.compare(w, Qt::CaseInsensitive) == 0 || s.startsWith(w, Qt::CaseInsensitive))
                    return s;
            }
        }
        return fallback;
    };
    uiFont_ = pick({QStringLiteral("Inter"), QStringLiteral("Geist"), QStringLiteral("Geist Sans"),
                    QStringLiteral("Segoe UI Variable"), QStringLiteral("Segoe UI")},
                   QStringLiteral("Segoe UI"));
    monoFont_ = pick({QStringLiteral("JetBrains Mono"), QStringLiteral("IBM Plex Mono"),
                      QStringLiteral("Cascadia Mono"), QStringLiteral("Consolas")},
                     QStringLiteral("Consolas"));
}

} // namespace Slf
