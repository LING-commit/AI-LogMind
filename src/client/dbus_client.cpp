#include "dbus_client.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <QDBusMetaType>

static int _register_dbus_types() {
    qDBusRegisterMetaType<QList<QVariantMap>>();
    return 0;
}
static const int _dbus_types_registered = _register_dbus_types();

DBusClient::DBusClient(QObject* parent)
    : QObject(parent)
    , m_bus(QDBusConnection::sessionBus())
    , m_reconnect_timer(new QTimer(this))
{
    m_reconnect_timer->setInterval(kReconnectIntervalMs);
    connect(m_reconnect_timer, &QTimer::timeout, this, &DBusClient::tryReconnect);
}

DBusClient::~DBusClient() {
    m_destroying = true;
    disconnectFromDaemon();
}

void DBusClient::connectToDaemon() {
    if (m_iface) {
        delete m_iface;
        m_iface = nullptr;
    }

    m_iface = new QDBusInterface(kService, kPath, kIface, m_bus, this);

    if (!m_iface->isValid()) {
        qWarning() << "D-Bus connection failed:" << m_iface->lastError().message();
        emit connectionFailed(m_iface->lastError().message());
        m_reconnect_timer->start();
        return;
    }

    onConnected();
}

void DBusClient::onConnected() {
    m_connected = true;
    m_reconnect_timer->stop();
    setupSignals();
    emit connected();

    qInfo() << "Connected to LogMind daemon via D-Bus";
    fetchStatus();
    listPlugins();
    queryLogs("", 100);
}

void DBusClient::disconnectFromDaemon() {
    if (m_iface) {
        m_bus.disconnect(kService, kPath, kIface,
                         "LogEntryReceived", this, SLOT(onLogEntryReceived(QVariantMap)));
        m_bus.disconnect(kService, kPath, kIface,
                         "AlertTriggered", this, SLOT(onAlertTriggered(QVariantMap)));
        delete m_iface;
        m_iface = nullptr;
    }
    if (!m_destroying)
        emit disconnected();
}

void DBusClient::tryReconnect() {
    if (m_connected) {
        m_reconnect_timer->stop();
        return;
    }
    qInfo() << "Attempting reconnect to daemon...";
    connectToDaemon();
}

void DBusClient::setupSignals() {
    m_bus.connect(kService, kPath, kIface,
                  "LogEntryReceived", this,
                  SLOT(onLogEntryReceived(QVariantMap)));

    m_bus.connect(kService, kPath, kIface,
                  "AlertTriggered", this,
                  SLOT(onAlertTriggered(QVariantMap)));

    m_bus.connect(kService, kPath, kIface,
                  "PluginLoaded", this,
                  SLOT(onPluginLoaded(QString)));

    m_bus.connect(kService, kPath, kIface,
                  "PluginUnloaded", this,
                  SLOT(onPluginUnloaded(QString)));

    m_bus.connect(kService, kPath, kIface,
                  "DaemonStatusChanged", this,
                  SLOT(onDaemonStatusChanged(QString)));
}

QVariantMap DBusClient::callMethod(const QString& method,
                                    const QVariantList& args) {
    if (!m_iface || !m_iface->isValid()) {
        return {{"error", "D-Bus interface not available"}};
    }
    QDBusReply<QVariantMap> reply = m_iface->callWithArgumentList(
        QDBus::Block, method, args);
    if (!reply.isValid()) {
        qWarning() << "D-Bus call" << method << "failed:" << reply.error().message();
        return {{"error", reply.error().message()}};
    }
    return reply.value();
}

void DBusClient::queryLogs(const QString& query, quint32 limit) {
    if (!m_iface) return;
    QDBusReply<QList<QVariantMap>> reply = m_iface->call("QueryLogs", query, limit);
    if (!reply.isValid()) {
        qWarning() << "QueryLogs D-Bus error:" << reply.error().message();
        return;
    }
    auto list = reply.value();
    qInfo() << "QueryLogs returned" << list.size() << "entries";
    emit logsQueried(QVector<QVariantMap>(list.begin(), list.end()));
}

void DBusClient::queryLogsByTime(quint64 start_ns, quint64 end_ns, quint32 limit) {
    if (!m_iface) return;
    QDBusReply<QList<QVariantMap>> reply = m_iface->call(
        "QueryLogsByTime", start_ns, end_ns, limit);
    if (reply.isValid()) {
        emit logsQueried(QVector<QVariantMap>::fromList(reply.value()));
    }
}

void DBusClient::listPlugins() {
    if (!m_iface) return;
    QDBusReply<QString> reply = m_iface->call("ListPlugins");
    if (!reply.isValid()) return;

    QJsonDocument doc = QJsonDocument::fromJson(reply.value().toUtf8());
    if (!doc.isArray()) return;

    QVector<QVariantMap> plugins;
    for (const QJsonValue& v : doc.array()) {
        QJsonObject obj = v.toObject();
        QVariantMap info;
        info["name"]   = obj["name"].toString();
        info["path"]   = obj["path"].toString();
        info["loaded"] = obj["loaded"].toBool();
        info["error"]  = obj["error"].toString();
        plugins.append(info);
    }
    emit pluginsListed(plugins);
}

void DBusClient::enablePlugin(const QString& name) {
    if (!m_iface) return;
    QDBusReply<bool> reply = m_iface->call("EnablePlugin", name);
    if (reply.isValid() && reply.value()) {
        emit pluginToggled(name, true);
    }
}

void DBusClient::disablePlugin(const QString& name) {
    if (!m_iface) return;
    QDBusReply<bool> reply = m_iface->call("DisablePlugin", name);
    if (reply.isValid() && reply.value()) {
        emit pluginToggled(name, false);
    }
}

void DBusClient::reloadPlugins() {
    if (!m_iface) return;
    QDBusReply<bool> reply = m_iface->call("ReloadPlugins");
    emit pluginsReloaded(reply.isValid() && reply.value());
}

void DBusClient::analyzeLogs(const QStringList& logIds) {
    if (!m_iface) return;
    QDBusReply<QList<QVariantMap>> reply = m_iface->call("AnalyzeLogs", logIds);
    if (reply.isValid() && !reply.value().isEmpty()) {
        emit analysisComplete(reply.value().first());
    } else {
        emit analysisFailed("No analysis result returned");
    }
}

void DBusClient::fetchStatus() {
    if (!m_iface) return;
    QDBusReply<QVariantMap> reply = m_iface->call("GetStatus");
    if (reply.isValid()) {
        emit statusReceived(reply.value());
    }
}

void DBusClient::fetchStats() {
    if (!m_iface) return;
    QDBusReply<QVariantMap> reply = m_iface->call("GetStats");
    if (reply.isValid()) {
        emit statsReceived(reply.value());
    }
}

void DBusClient::onDaemonStatusChanged(const QString& state) {
    emit statusReceived({{"state", state}});
}

void DBusClient::onDisconnected() {
    m_connected = false;
    emit disconnected();
    m_reconnect_timer->start();
}

void DBusClient::onLogEntryReceived(QVariantMap entry) {
    emit logEntryReceived(entry);
}

void DBusClient::onAlertTriggered(QVariantMap alert) {
    emit alertTriggered(alert);
}

void DBusClient::onPluginLoaded(const QString& name) {
    qInfo() << "Plugin loaded:" << name;
}

void DBusClient::onPluginUnloaded(const QString& name) {
    qInfo() << "Plugin unloaded:" << name;
}
