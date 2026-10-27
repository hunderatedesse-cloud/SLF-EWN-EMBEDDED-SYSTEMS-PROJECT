#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include "core/Types.h"
#include "model/AlertListModel.h"
#include "model/Database.h"
#include "model/NodeListModel.h"
#include "model/SerialPortReader.h"
#include "model/SimulatedTelemetrySource.h"
#include "controller/AuthController.h"
#include "controller/NodeManager.h"

#include <QDateTime>
#include <QObject>
#include <QSerialPortInfo>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

namespace Slf {

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int page READ page WRITE setPage NOTIFY pageChanged)
    Q_PROPERTY(int connectionState READ connectionState NOTIFY connectionChanged)
    Q_PROPERTY(QString portName READ portName WRITE setPortName NOTIFY connectionChanged)
    Q_PROPERTY(QStringList availablePorts READ availablePorts NOTIFY portsChanged)
    Q_PROPERTY(QStringList availablePortLabels READ availablePortLabels NOTIFY portsChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY connectionChanged)
    Q_PROPERTY(bool simulationEnabled READ simulationEnabled WRITE setSimulationEnabled NOTIFY simulationChanged)
    Q_PROPERTY(int nodeCount READ nodeCount NOTIFY countsChanged)
    Q_PROPERTY(int liveNodeCount READ liveNodeCount NOTIFY countsChanged)
    Q_PROPERTY(int alertCount READ alertCount NOTIFY countsChanged)
    Q_PROPERTY(QString clock READ clock NOTIFY clockChanged)
    Q_PROPERTY(QString uiFont READ uiFont CONSTANT)
    Q_PROPERTY(QString monoFont READ monoFont CONSTANT)
    Q_PROPERTY(NodeListModel *nodes READ nodes CONSTANT)
    Q_PROPERTY(AlertListModel *alerts READ alerts CONSTANT)
    Q_PROPERTY(int selectedId READ selectedId NOTIFY selectedChanged)
    Q_PROPERTY(bool detailOpen READ detailOpen NOTIFY selectedChanged)
    Q_PROPERTY(QVariantMap selectedNode READ selectedNode NOTIFY selectedChanged)
    Q_PROPERTY(QVariantList mapNodes READ mapNodes NOTIFY mapNodesChanged)
    Q_PROPERTY(QVariantList zones READ zones NOTIFY mapNodesChanged)
    Q_PROPERTY(QStringList zoneNames READ zoneNames NOTIFY mapNodesChanged)
    Q_PROPERTY(float moistureWarning READ moistureWarning WRITE setMoistureWarning NOTIFY thresholdsChanged)
    Q_PROPERTY(float moistureCritical READ moistureCritical WRITE setMoistureCritical NOTIFY thresholdsChanged)
    Q_PROPERTY(float tiltWarning READ tiltWarning WRITE setTiltWarning NOTIFY thresholdsChanged)
    Q_PROPERTY(float tiltCritical READ tiltCritical WRITE setTiltCritical NOTIFY thresholdsChanged)
    Q_PROPERTY(int baudRate READ baudRate CONSTANT)
    Q_PROPERTY(AuthController *auth READ auth CONSTANT)
    Q_PROPERTY(QString storePath READ storePath CONSTANT)
    Q_PROPERTY(QString storeStatus READ storeStatus NOTIFY storeChanged)
    Q_PROPERTY(int storedSamples READ storedSamples NOTIFY storeChanged)
    Q_PROPERTY(int storedAlerts READ storedAlerts NOTIFY storeChanged)
    Q_PROPERTY(QString storeLastWrite READ storeLastWrite NOTIFY storeChanged)
    Q_PROPERTY(int storePending READ storePending NOTIFY storeChanged)
    Q_PROPERTY(qint64 storeRows READ storeRows NOTIFY storeChanged)
    Q_PROPERTY(QString storeSize READ storeSize NOTIFY storeChanged)
    // Live link telemetry, so a silent or mis-framed feed is diagnosable in-app.
    Q_PROPERTY(qint64 rxBytes READ rxBytes NOTIFY linkStatsChanged)
    Q_PROPERTY(qint64 rxFrames READ rxFrames NOTIFY linkStatsChanged)
    Q_PROPERTY(qint64 rxDiscarded READ rxDiscarded NOTIFY linkStatsChanged)
    Q_PROPERTY(QString rxLastFrame READ rxLastFrame NOTIFY linkStatsChanged)
    Q_PROPERTY(QString rxTail READ rxTail NOTIFY linkStatsChanged)
    Q_PROPERTY(QString linkDiagnosis READ linkDiagnosis NOTIFY linkStatsChanged)
    Q_PROPERTY(QString sourceLabel READ sourceLabel NOTIFY connectionChanged)
    Q_PROPERTY(bool dumpBusy READ dumpBusy NOTIFY dumpStatsChanged)
    Q_PROPERTY(bool fullDumpBusy READ fullDumpBusy NOTIFY dumpStatsChanged)
    Q_PROPERTY(int lastRecordId READ lastRecordId NOTIFY dumpStatsChanged)
    Q_PROPERTY(bool hasLastRecordId READ hasLastRecordId NOTIFY dumpStatsChanged)
    Q_PROPERTY(int receiverWriteCount READ receiverWriteCount NOTIFY dumpStatsChanged)
    Q_PROPERTY(int receiverFreeRecords READ receiverFreeRecords NOTIFY dumpStatsChanged)
    Q_PROPERTY(QString dumpPhase READ dumpPhase NOTIFY dumpStatsChanged)
    Q_PROPERTY(QString dumpStatus READ dumpStatus NOTIFY dumpStatsChanged)
    Q_PROPERTY(QString serialTrace READ serialTrace NOTIFY dumpStatsChanged)
    Q_PROPERTY(int dumpPercent READ dumpPercent NOTIFY dumpStatsChanged)

public:
    explicit AppController(QObject *parent = nullptr);

    int page() const { return page_; }
    void setPage(int p);
    int connectionState() const { return static_cast<int>(connection_); }
    QString portName() const { return portName_; }
    void setPortName(const QString &p);
    QStringList availablePorts() const { return ports_; }
    QStringList availablePortLabels() const { return portLabels_; }
    QString lastError() const { return lastError_; }
    bool simulationEnabled() const { return simulationEnabled_; }
    void setSimulationEnabled(bool on);
    int nodeCount() const;
    int liveNodeCount() const { return nodes_ ? nodes_->rowCount() : 0; }
    int alertCount() const;
    QString clock() const { return clock_; }
    QString uiFont() const { return uiFont_; }
    QString monoFont() const { return monoFont_; }
    NodeListModel *nodes() const { return nodes_; }
    AlertListModel *alerts() const { return alerts_; }
    int selectedId() const { return selectedId_; }
    bool detailOpen() const { return detailOpen_; }
    QVariantMap selectedNode() const;
    QVariantList mapNodes() const;
    QVariantList zones() const;
    QStringList zoneNames() const;
    float moistureWarning() const;
    float moistureCritical() const;
    float tiltWarning() const;
    float tiltCritical() const;
    void setMoistureWarning(float v);
    void setMoistureCritical(float v);
    void setTiltWarning(float v);
    void setTiltCritical(float v);
    int baudRate() const { return static_cast<int>(kBaudRate); }
    AuthController *auth() const { return auth_; }
    QString storePath() const { return db_.path(); }
    QString storeStatus() const { return storeStatus_; }
    int storedSamples() const { return storedSamples_; }
    int storedAlerts() const { return storedAlerts_; }
    QString storeLastWrite() const;
    int storePending() const { return db_.pendingWrites(); }
    qint64 storeRows() const { return db_.rowsWrittenThisSession(); }
    QString storeSize() const;
    qint64 rxBytes() const { return static_cast<qint64>(serial_.bytesReceived()); }
    qint64 rxFrames() const { return static_cast<qint64>(serial_.framesDecoded()); }
    qint64 rxDiscarded() const { return static_cast<qint64>(serial_.discardedBytes()); }
    QString rxLastFrame() const;
    QString rxTail() const { return serial_.rawTailHex(); }
    QString linkDiagnosis() const;
    QString sourceLabel() const;
    bool dumpBusy() const { return serial_.dumpBusy(); }
    bool fullDumpBusy() const { return serial_.fullDumpBusy(); }
    int lastRecordId() const { return serial_.hasLastRecordId() ? int(serial_.lastRecordId()) : -1; }
    bool hasLastRecordId() const { return serial_.hasLastRecordId(); }
    int receiverWriteCount() const { return int(serial_.writeCount()); }
    int receiverFreeRecords() const { return int(serial_.freeRecords()); }
    QString dumpPhase() const { return serial_.dumpPhase(); }
    QString dumpStatus() const { return serial_.dumpStatus(); }
    QString serialTrace() const { return serial_.serialTrace(); }
    int dumpPercent() const { return serial_.dumpPercent(); }

    Q_INVOKABLE void refreshPorts();
    Q_INVOKABLE void refreshFromReceiver();
    Q_INVOKABLE void clearReceiverLog();
    Q_INVOKABLE void clearSerialTrace();
    Q_INVOKABLE bool addNodeAt(int deviceId, double lat, double lon);
    Q_INVOKABLE int parseDeviceId(const QString &text) const;
    Q_INVOKABLE void flushStore();
    Q_INVOKABLE void copyToClipboard(const QString &text);
    Q_INVOKABLE void connectSerial();
    Q_INVOKABLE void disconnectSerial();
    Q_INVOKABLE void openDetail(int deviceId);
    Q_INVOKABLE void closeDetail();
    Q_INVOKABLE void acknowledgeSelected();
    Q_INVOKABLE void acknowledgeAlert(int alertId);
    Q_INVOKABLE void acknowledgeNode(int deviceId);
    Q_INVOKABLE void clearAlerts();
    Q_INVOKABLE bool exportAlerts(const QUrl &url);
    Q_INVOKABLE void setZoneLabel(int deviceId, const QString &label);
    Q_INVOKABLE void assignNode(int deviceId, const QString &placeId);
    Q_INVOKABLE void deleteNode(int deviceId);
    Q_INVOKABLE void setNodeGeo(int deviceId, double lat, double lon, double alt);
    Q_INVOKABLE void moveNodeOnMap(int deviceId, double lat, double lon, const QString &placeId);
    Q_INVOKABLE void updateNodeMeta(int deviceId, const QString &locationName, const QString &description);
    Q_INVOKABLE QString createPlaceCoverage(const QString &name, const QString &notes,
                                            double lat1, double lon1, double lat2, double lon2);
    Q_INVOKABLE bool updatePlaceInfo(const QString &id, const QString &name, const QString &notes);
    Q_INVOKABLE void setNodeThresholds(int deviceId, bool custom, float moistureWarning, float moistureCritical, float tiltWarning, float tiltCritical);
    Q_INVOKABLE QString createPlace(const QString &name, const QString &notes, float x, float y, float w, float h,
                                    double lat1, double lon1, double alt1, double lat2, double lon2, double alt2);
    Q_INVOKABLE bool updatePlace(const QString &id, const QString &name, const QString &notes, float x, float y, float w, float h,
                                 double lat1, double lon1, double alt1, double lat2, double lon2, double alt2);
    Q_INVOKABLE bool deletePlace(const QString &id);
    Q_INVOKABLE void setNodeMapPos(int deviceId, float x, float y);
    Q_INVOKABLE QVariantList moistureSeries(int range) const;
    Q_INVOKABLE QVariantList tiltSeries(int range) const;
    Q_INVOKABLE QString statusLabel(int status) const;
    Q_INVOKABLE QString statusColor(int status) const;
    Q_INVOKABLE int probeStatus(float moisture, float tilt) const;

signals:
    void pageChanged();
    void connectionChanged();
    void portsChanged();
    void simulationChanged();
    void countsChanged();
    void clockChanged();
    void selectedChanged();
    void mapNodesChanged();
    void thresholdsChanged();
    void serialShake();
    void chartNeedsReload();
    void storeChanged();
    void linkStatsChanged();
    void dumpStatsChanged();
    void permissionDenied();

private:
    bool requireAdmin();
    void refreshStoreStats();
    void handleSerialState(ConnectionState s);
    void applyConnectionState(ConnectionState s);
    void startSimulation();
    void stopSimulation();
    void seedHistory();
    void openSerialNow(bool userInitiated = false);
    void tryAutoConnect();
    void beginReceiverSession();
    void persistLastRid();
    static int portPreference(const QSerialPortInfo &info);
    TimeRange toRange(int range) const;
    QVariantList seriesOf(quint16 id, int range, int field) const;
    void detectFonts();

    Database db_;
    NodeManager manager_;
    SerialPortReader serial_;
    SimulatedTelemetrySource sim_;
    AuthController *auth_ = nullptr;
    NodeListModel *nodes_ = nullptr;
    AlertListModel *alerts_ = nullptr;
    QTimer clockTimer_;
    QTimer storeStatsTimer_;
    QTimer portScanTimer_;
    QTimer linkStatsTimer_;
    QTimer dumpPollTimer_;
    QString storeStatus_;
    int storedSamples_ = 0;
    int storedAlerts_ = 0;
    bool linkStatsDirty_ = false;
    bool handshakePending_ = false;
    QDateTime lastDumpAt_;

    int page_ = 0;
    ConnectionState connection_ = ConnectionState::Disconnected;
    QString portName_;
    QStringList ports_;
    QStringList portLabels_;
    QString lastError_;
    bool simulationEnabled_ = false;
    bool userHeldDisconnect_ = false;
    QString clock_;
    QString uiFont_;
    QString monoFont_;
    int selectedId_ = 0;
    bool detailOpen_ = false;
};

} // namespace Slf

#endif
