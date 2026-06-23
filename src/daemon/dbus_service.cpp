#include "dbus_service.h"
#include <logmind/pipeline.h>
#include <logmind/plugin_loader.h>
#include <logmind/log_buffer.h>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <QDebug>
#include <grp.h>
#include <unistd.h>
#include <chrono>

static int _register_dbus_types() {
    qDBusRegisterMetaType<QList<QVariantMap>>();
    return 0;
}
static const int _dbus_types_registered = _register_dbus_types();

QVariantMap log_entry_to_varmap(const logmind::LogEntry& entry) {
    QVariantMap map;
    map["source"]    = QString::fromStdString(entry.source);
    map["raw"]       = QString::fromStdString(entry.raw);
    auto ts_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        entry.timestamp.time_since_epoch()).count();
    map["timestamp"] = QVariant::fromValue(static_cast<quint64>(ts_ns));
    map["level"]     = QString::fromStdString(to_string(entry.level));
    map["message"]   = QString::fromStdString(entry.message);
    map["module"]    = QString::fromStdString(entry.module);
    map["fields"]    = QString::fromStdString(entry.fields.dump());
    map["tags"]      = QString::fromStdString(entry.tags.dump());
    map["meta"]      = QString::fromStdString(entry.meta.dump());
    return map;
}

logmind::LogEntry varmap_to_log_entry(const QVariantMap& map) {
    logmind::LogEntry entry;
    entry.source    = map.value("source").toString().toStdString();
    entry.raw       = map.value("raw").toString().toStdString();
    entry.timestamp = std::chrono::system_clock::from_time_t(
        static_cast<std::time_t>(
            static_cast<qint64>(map.value("timestamp").toULongLong())));
    entry.level     = logmind::log_level_from_string(
        map.value("level").toString().toStdString());
    entry.message   = map.value("message").toString().toStdString();
    entry.module    = map.value("module").toString().toStdString();
    try {
        entry.fields = nlohmann::json::parse(
            map.value("fields").toString().toStdString());
        entry.tags   = nlohmann::json::parse(
            map.value("tags").toString().toStdString());
        entry.meta   = nlohmann::json::parse(
            map.value("meta").toString().toStdString());
    } catch (...) {}
    return entry;
}

bool is_member_of_group(uid_t uid, const char* group_name) {
    (void)uid, (void)group_name;
    // 生产环境应当检查 uid 是否属于指定用户组
    return true;
}

DaemonAdaptor::DaemonAdaptor(logmind::Pipeline* pipeline,
                              logmind::PluginLoader* loader,
                              logmind::LogBuffer* buffer,
                              const std::string& plugins_dir,
                              QObject* parent)
    : QDBusAbstractAdaptor(parent)
    , m_pipeline(pipeline)
    , m_loader(loader)
    , m_buffer(buffer)
    , m_plugins_dir(plugins_dir)
{
}

bool DaemonAdaptor::check_permission(const QDBusMessage& msg) {
    static const QSet<QString> public_methods = {
        "QueryLogs", "QueryLogsByTime",
        "ListPlugins", "GetPluginInfo",
        "GetStats", "GetStatus"
    };

    QString method = msg.member();
    if (public_methods.contains(method)) return true;

    uid_t uid = msg.service().isEmpty() ? getuid()
                : static_cast<uid_t>(msg.service().toInt());
    return is_member_of_group(uid, "logmind-adm")
        || is_member_of_group(uid, "logmind-dev");
}

void DaemonAdaptor::InjectLog(const QString& source, const QString& raw_line) {
    if (m_pipeline) {
        m_pipeline->process_raw(source.toStdString(), raw_line.toStdString());
    }
}

QList<QVariantMap> DaemonAdaptor::QueryLogs(const QString& query, quint32 limit) {
    if (!m_buffer) return {};

    auto entries = m_buffer->search(query.toStdString(), limit);
    QList<QVariantMap> result;
    for (const auto& e : entries) {
        result.append(log_entry_to_varmap(e));
    }
    return result;
}

QList<QVariantMap> DaemonAdaptor::QueryLogsByTime(
    quint64 start_ns, quint64 end_ns, quint32 limit)
{
    if (!m_buffer) return {};

    auto start = std::chrono::system_clock::from_time_t(
        static_cast<std::time_t>(start_ns / 1000000000ULL));
    auto end   = std::chrono::system_clock::from_time_t(
        static_cast<std::time_t>(end_ns / 1000000000ULL));

    auto entries = m_buffer->query_by_time(start, end, limit);
    QList<QVariantMap> result;
    for (const auto& e : entries) {
        result.append(log_entry_to_varmap(e));
    }
    return result;
}

QList<QVariantMap> DaemonAdaptor::AnalyzeLogs(const QStringList& log_ids) {
    QList<QVariantMap> result;
    for (const auto& id : log_ids) {
        QVariantMap r;
        r["log_id"]  = id;
        r["summary"] = "Analysis requested";
        r["confidence"] = 0.0;
        result.append(r);
    }
    return result;
}

QString DaemonAdaptor::GetAnalysisReport(const QString& log_id) {
    nlohmann::json report;
    report["log_id"]    = log_id.toStdString();
    report["summary"]   = "No analysis available";
    report["confidence"] = 0.0;
    return QString::fromStdString(report.dump(2));
}

QString DaemonAdaptor::ListPlugins() {
    nlohmann::json arr = nlohmann::json::array();
    if (!m_loader) return QString::fromStdString(arr.dump());

    auto plugins = m_loader->list_plugins();
    for (const auto& p : plugins) {
        nlohmann::json info;
        info["name"]        = p.name;
        info["version"]     = p.version;
        info["description"] = p.description;
        info["author"]      = p.author;
        info["type"]        = to_string(p.type);
        info["loaded"]      = p.loaded;
        info["enabled"]     = p.enabled;
        info["so_path"]     = p.so_path;
        info["error"]       = p.error;
        arr.push_back(std::move(info));
    }
    return QString::fromStdString(arr.dump(2));
}

QVariantMap DaemonAdaptor::GetPluginInfo(const QString& name) {
    if (!m_loader) return {{"error", "Loader not available"}};

    auto* plugin = m_loader->find(name.toStdString());
    if (!plugin) return {{"error", "Plugin not found"}};

    QVariantMap info;
    info["name"]        = QString::fromStdString(plugin->name());
    info["version"]     = QString::fromStdString(plugin->version());
    info["description"] = QString::fromStdString(plugin->description());
    info["type"]        = QString::fromStdString(to_string(plugin->type()));
    return info;
}

bool DaemonAdaptor::EnablePlugin(const QString& name) {
    return m_loader && m_loader->set_enabled(name.toStdString(), true);
}

bool DaemonAdaptor::DisablePlugin(const QString& name) {
    return m_loader && m_loader->set_enabled(name.toStdString(), false);
}

bool DaemonAdaptor::ReloadPlugins() {
    if (!m_loader) return false;

    m_loader->unload_all();
    return m_loader->load_directory(m_plugins_dir);
}

QVariantMap DaemonAdaptor::GetStats() {
    QVariantMap stats;
    if (m_pipeline) {
        auto s = m_pipeline->stats();
        stats["total_processed"] = QVariant::fromValue(static_cast<qlonglong>(s.total_processed));
        stats["total_errors"]    = QVariant::fromValue(static_cast<qlonglong>(s.total_errors));
        stats["total_alerts"]    = QVariant::fromValue(static_cast<qlonglong>(s.total_alerts));
        stats["qps"]             = static_cast<double>(s.qps);
    }
    if (m_loader) {
        stats["plugin_count"] = QVariant::fromValue(static_cast<qlonglong>(m_loader->loaded_count()));
    }
    return stats;
}

QVariantMap DaemonAdaptor::GetStatus() {
    QVariantMap status;
    status["state"]        = "RUNNING";
    status["version"]      = "0.1.0";
    status["uptime_sec"]   = QVariant::fromValue(static_cast<qlonglong>(0));
    status["memory_mb"]    = 0.0;

    if (m_loader) {
        status["plugin_count"] = QVariant::fromValue(static_cast<qlonglong>(m_loader->loaded_count()));
    }

    if (m_buffer) {
        status["buffer_size"]  = QVariant::fromValue(static_cast<qlonglong>(m_buffer->size()));
        status["buffer_total"] = QVariant::fromValue(static_cast<qlonglong>(m_buffer->total_pushed()));
    }

    return status;
}
