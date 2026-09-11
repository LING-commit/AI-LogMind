#include "dbus_service.h"
#include <logmind/pipeline.h>
#include <logmind/plugin_loader.h>
#include <logmind/log_buffer.h>
#include <logmind/vector_store.h>
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
    // 纳秒时间戳要按纳秒还原。原先走 from_time_t(ns) 把纳秒当成秒，
    // 时间戳会被放大 10^9 倍。
    const auto ts_ns = static_cast<qint64>(map.value("timestamp").toULongLong());
    entry.timestamp = std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::nanoseconds(ts_ns)));
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
                              logmind::VectorStore* vector_store,
                              ReloadFn reload_fn,
                              StateFn state_fn,
                              std::chrono::steady_clock::time_point started_at,
                              QObject* parent)
    : QDBusAbstractAdaptor(parent)
    , m_pipeline(pipeline)
    , m_loader(loader)
    , m_buffer(buffer)
    , m_vector_store(vector_store)
    , m_reload_fn(std::move(reload_fn))
    , m_state_fn(std::move(state_fn))
    , m_started_at(started_at)
{
}

bool DaemonAdaptor::check_permission(const QDBusMessage& msg) {
    static const QSet<QString> public_methods = {
        "QueryLogs", "QueryLogsByTime", "SearchSimilar",
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

QString DaemonAdaptor::state() const {
    return m_state_fn ? QString::fromStdString(to_string(m_state_fn()))
                      : QStringLiteral("UNKNOWN");
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
    result.reserve(static_cast<qsizetype>(entries.size()));
    for (const auto& e : entries) {
        result.append(log_entry_to_varmap(e));
    }
    return result;
}

QList<QVariantMap> DaemonAdaptor::QueryLogsByTime(
    quint64 start_ns, quint64 end_ns, quint32 limit)
{
    if (!m_buffer) return {};

    // 入参是纳秒。原先用 from_time_t(ns / 1e9) 会把亚秒精度整个丢掉，
    // 窗口小于 1 秒的查询直接失效。
    using Clock = std::chrono::system_clock;
    const auto to_tp = [](quint64 ns) {
        return Clock::time_point(std::chrono::duration_cast<Clock::duration>(
            std::chrono::nanoseconds(static_cast<qint64>(ns))));
    };

    auto entries = m_buffer->query_by_time(to_tp(start_ns), to_tp(end_ns), limit);
    QList<QVariantMap> result;
    result.reserve(static_cast<qsizetype>(entries.size()));
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

QList<QVariantMap> DaemonAdaptor::SearchSimilar(const QList<double>& embedding,
                                                quint32 top_k) {
    QList<QVariantMap> result;
    if (!m_vector_store || embedding.isEmpty() || top_k == 0) return result;

    std::vector<float> query;
    query.reserve(static_cast<size_t>(embedding.size()));
    for (const double v : embedding) query.push_back(static_cast<float>(v));

    const auto hits = m_vector_store->search(query, top_k);
    result.reserve(static_cast<qsizetype>(hits.size()));
    for (const auto& hit : hits) {
        QVariantMap map;
        map["log_id"]     = QString::fromStdString(hit.log_id);
        map["metadata"]   = QString::fromStdString(hit.metadata_json);
        map["distance"]   = static_cast<double>(hit.distance);
        map["similarity"] = static_cast<double>(hit.similarity);
        result.append(map);
    }
    return result;
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
    // 走 DaemonCore::reload_plugins()，这样状态机、在途数据排空和
    // 插件配置下发都不会被绕过。原先这里直接对 loader 做 unload_all +
    // load_directory，三者全被跳过。
    return m_reload_fn ? m_reload_fn() : false;
}

QVariantMap DaemonAdaptor::GetStats() {
    QVariantMap stats;
    if (m_pipeline) {
        auto s = m_pipeline->stats();
        stats["total_processed"]  = QVariant::fromValue(static_cast<qlonglong>(s.total_processed));
        stats["total_errors"]     = QVariant::fromValue(static_cast<qlonglong>(s.total_errors));
        stats["total_alerts"]     = QVariant::fromValue(static_cast<qlonglong>(s.total_alerts));
        stats["total_suppressed"] = QVariant::fromValue(static_cast<qlonglong>(s.total_suppressed));
        stats["total_dropped"]    = QVariant::fromValue(static_cast<qlonglong>(s.total_dropped));
        stats["qps"]              = s.qps;
    }
    if (m_loader) {
        stats["plugin_count"] = QVariant::fromValue(static_cast<qlonglong>(m_loader->loaded_count()));
    }
    if (m_buffer) {
        stats["buffer_size"]      = QVariant::fromValue(static_cast<qlonglong>(m_buffer->size()));
        stats["buffer_discarded"] = QVariant::fromValue(static_cast<qlonglong>(m_buffer->total_discarded()));
    }
    if (m_vector_store) {
        stats["vector_count"] = QVariant::fromValue(static_cast<qlonglong>(m_vector_store->size()));
    }
    return stats;
}

QVariantMap DaemonAdaptor::GetStatus() {
    QVariantMap status;
    // 原先这三个字段是写死的 "RUNNING" / 0 / 0.0
    status["state"]   = state();
    status["version"] = QStringLiteral("0.1.0");

    const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - m_started_at).count();
    status["uptime_sec"] = QVariant::fromValue(static_cast<qlonglong>(uptime));

    if (m_loader) {
        status["plugin_count"] = QVariant::fromValue(static_cast<qlonglong>(m_loader->loaded_count()));
    }

    if (m_buffer) {
        status["buffer_size"]  = QVariant::fromValue(static_cast<qlonglong>(m_buffer->size()));
        status["buffer_total"] = QVariant::fromValue(static_cast<qlonglong>(m_buffer->total_pushed()));
    }

    return status;
}
