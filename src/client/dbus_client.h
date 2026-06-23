#pragma once
#include <QObject>
#include <QDBusInterface>
#include <QDBusConnection>
#include <QDBusReply>
#include <QDBusVariant>
#include <QTimer>
#include <QVariantMap>
#include <QVector>

class LogModel;
class PluginModel;
class AlertPanel;

class DBusClient : public QObject {
    Q_OBJECT
public:
    explicit DBusClient(QObject* parent = nullptr);
    ~DBusClient() override;

    bool isConnected() const { return m_connected; }

    void queryLogs(const QString& query, quint32 limit);
    void queryLogsByTime(quint64 start_ns, quint64 end_ns, quint32 limit);
    void listPlugins();
    void enablePlugin(const QString& name);
    void disablePlugin(const QString& name);
    void reloadPlugins();
    void analyzeLogs(const QStringList& logIds);
    void fetchStatus();
    void fetchStats();

signals:
    void connected();
    void disconnected();
    void connectionFailed(const QString& error);
    void logEntryReceived(QVariantMap entry);
    void logsQueried(QVector<QVariantMap> entries);
    void pluginsListed(QVector<QVariantMap> plugins);
    void pluginToggled(const QString& name, bool enabled);
    void pluginsReloaded(bool success);
    void analysisComplete(QVariantMap result);
    void analysisFailed(const QString& error);
    void statusReceived(QVariantMap status);
    void statsReceived(QVariantMap stats);
    void alertTriggered(QVariantMap alert);

public slots:
    void connectToDaemon();
    void disconnectFromDaemon();

private slots:
    void tryReconnect();
    void onDaemonStatusChanged(const QString& state);
    void onLogEntryReceived(QVariantMap entry);
    void onAlertTriggered(QVariantMap alert);
    void onPluginLoaded(const QString& name);
    void onPluginUnloaded(const QString& name);

private:
    void setupSignals();
    void onConnected();
    void onDisconnected();
    QVariantMap callMethod(const QString& method,
                            const QVariantList& args = {});

    QDBusConnection         m_bus;
    QDBusInterface*         m_iface = nullptr;
    QTimer*                 m_reconnect_timer;
    bool                    m_connected = false;
    bool                    m_destroying = false;
    static constexpr int    kReconnectIntervalMs = 5000;
    static constexpr const char* kService  = "com.logmind.Daemon";
    static constexpr const char* kPath     = "/com/logmind/Daemon";
    static constexpr const char* kIface    = "com.logmind.Daemon1";
};
