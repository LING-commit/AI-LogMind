#include "daemon_core.h"
#include "dbus_service.h"
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDebug>
#include <algorithm>
#include <thread>

static VectorStoreConfig     build_vector_config(const ConfigMgr& config);
static Pipeline::AlertRule   build_alert_rule(const ConfigMgr& config);
static size_t                buffer_capacity_of(const ConfigMgr& config);

DaemonCore::DaemonCore(const ConfigMgr& config, QObject* parent)
    : QObject(parent)
    , m_config(config)
    // 之前这里用的是 LogBuffer 的默认容量，配置里的 log_buffer.capacity 从未生效
    , m_log_buffer(buffer_capacity_of(config))
    , m_pipeline(m_plugin_loader)
    , m_file_watcher(m_pipeline)
    , m_vector_store(build_vector_config(config))
{
    m_log_buffer.set_on_entry_callback([this](const LogEntry& entry) {
        QVariantMap varmap = log_entry_to_varmap(entry);
        QMetaObject::invokeMethod(this, [this, varmap]() {
            if (m_dbus_adaptor)
                emit m_dbus_adaptor->LogEntryReceived(varmap);
        }, Qt::QueuedConnection);
    });

    // 插件加载时通过它拿到 config.json 里 plugins.<插件名> 那一段。
    // 之前 on_load 永远收到一个空对象，插件根本拿不到自己的配置。
    m_plugin_loader.set_config_provider([this](const std::string& name) {
        return m_config.plugin_config(name);
    });
}

DaemonCore::~DaemonCore() {
    do_stop();
}

void DaemonCore::start() {
    qInfo() << "DaemonCore starting...";
    m_started_at = std::chrono::steady_clock::now();

    if (!do_load_plugins()) {
        qWarning() << "No plugins loaded, continuing without plugins";
    }

    if (!do_init_pipeline()) {
        qCritical() << "Pipeline initialization failed";
        transition_to(DaemonState::FAILED);
        return;
    }

    if (!do_register_dbus()) {
        qCritical() << "D-Bus registration failed";
        transition_to(DaemonState::FAILED);
        return;
    }

    // 向量库要在流水线开始投递之前打开
    m_vector_store_ready = m_vector_store.open();
    if (!m_vector_store_ready) {
        qWarning() << "VectorStore open failed, continuing without vector search";
    }

    if (!do_start_watcher()) {
        qWarning() << "File watcher start failed, continuing without file monitoring";
    }

    transition_to(DaemonState::RUNNING);
    qInfo() << "DaemonCore started successfully";

    m_health_timer = new QTimer(this);
    m_health_timer->setInterval(30000);
    connect(m_health_timer, &QTimer::timeout, this, [this]() {
        // 之前这里取了 stats 就 Q_UNUSED 扔掉，等于空转
        const auto s = m_pipeline.stats();
        qInfo().noquote()
            << QString("health: processed=%1 qps=%2 errors=%3 alerts=%4 "
                       "suppressed=%5 dropped=%6 buffer=%7/%8 vectors=%9")
               .arg(s.total_processed)
               .arg(s.qps, 0, 'f', 1)
               .arg(s.total_errors)
               .arg(s.total_alerts)
               .arg(s.total_suppressed)
               .arg(s.total_dropped)
               .arg(m_log_buffer.size())
               .arg(m_log_buffer.capacity())
               .arg(m_vector_store_ready ? m_vector_store.size() : 0);
    });
    m_health_timer->start();
}

void DaemonCore::stop() {
    qInfo() << "DaemonCore stopping...";
    transition_to(DaemonState::STOPPING);
    do_stop();
    transition_to(DaemonState::STOPPED);
    emit finished();
}

bool DaemonCore::reload_plugins() {
    transition_to(DaemonState::RELOADING);
    qInfo() << "Reloading plugins...";

    // 先把在途的行处理完再换插件，避免重载期间的行被丢进空插件集
    m_pipeline.flush();

    m_plugin_loader.unload_all();
    const bool ok = do_load_plugins();

    transition_to(ok ? DaemonState::RUNNING : DaemonState::FAILED);
    return ok;
}

bool DaemonCore::reload_config() {
    if (!m_config.reload()) {
        qWarning() << "Configuration reload failed:" << m_config.last_error().c_str();
        return false;
    }
    // 能热更新的部分立刻生效；容量、线程数这类需要重启
    m_pipeline.set_alert_rule(build_alert_rule(m_config));
    qInfo() << "Configuration reloaded";
    return true;
}

void DaemonCore::transition_to(DaemonState new_state) {
    auto old = m_state;
    m_state = new_state;
    emit state_changed(old, new_state);
}

bool DaemonCore::do_load_plugins() {
    transition_to(DaemonState::LOADING);
    std::string plugins_dir = m_config.plugins_dir();
    if (plugins_dir.empty()) {
        plugins_dir = "/usr/lib/logmind/plugins";
    }

    qInfo() << "Loading plugins from:" << plugins_dir.c_str();
    const bool ok = m_plugin_loader.load_directory(plugins_dir);

    const auto snapshot = m_plugin_loader.snapshot();
    qInfo() << "Plugins loaded:"
            << snapshot->collectors.size() << "collectors,"
            << snapshot->analyzers.size()  << "analyzers,"
            << snapshot->alerts.size()     << "alerts";

    emit plugin_load_progress(m_plugin_loader.loaded_count(),
                              m_plugin_loader.loaded_count());

    return ok;
}

bool DaemonCore::do_init_pipeline() {
    m_pipeline.set_alert_rule(build_alert_rule(m_config));

    // config.json 里的 worker_threads 之前没有任何调用点，流水线一直是
    // 同步跑在采集线程上的。>1 时启用异步分片队列。
    int threads = m_config.worker_threads();
    if (threads < 0) threads = 0;
    if (threads > 64) threads = 64;
    if (threads > 1) {
        const size_t queue_cap = std::max<size_t>(1024, buffer_capacity_of(m_config) / 4);
        m_pipeline.start_workers(static_cast<size_t>(threads), queue_cap);
        qInfo() << "Pipeline running asynchronously with" << threads
                << "workers, queue capacity" << queue_cap;
    } else {
        qInfo() << "Pipeline running synchronously on the collector thread";
    }
    return true;
}

bool DaemonCore::do_register_dbus() {
    transition_to(DaemonState::STARTING);

    auto bus = QDBusConnection::sessionBus();
    if (!bus.registerService("com.logmind.Daemon")) {
        qCritical() << "Failed to register D-Bus service:"
                     << bus.lastError().message();
        return false;
    }

    // 流水线输出 → LogBuffer（+ 有 embedding 的顺带写进向量库）
    m_pipeline.set_output_callback([this](std::vector<LogEntry> entries) {
        for (auto& entry : entries) {
            store_embedding(entry);
            m_log_buffer.push(std::move(entry));
        }
    });

    m_dbus_adaptor = new DaemonAdaptor(&m_pipeline, &m_plugin_loader,
                                        &m_log_buffer, &m_vector_store,
                                        [this] { return reload_plugins(); },
                                        [this] { return m_state; },
                                        m_started_at,
                                        this);

    if (!bus.registerObject("/com/logmind/Daemon", this)) {
        qCritical() << "Failed to register D-Bus object:"
                     << bus.lastError().message();
        return false;
    }

    qInfo() << "D-Bus service registered: com.logmind.Daemon";
    return true;
}

// embedding 由 Analyzer 插件放在 entry.fields["embedding"] 里（数字数组）。
// 守护进程自己不做向量化——那是插件的职责。
void DaemonCore::store_embedding(const LogEntry& entry) {
    if (!m_vector_store_ready) return;
    if (!entry.fields.is_object())   return;

    auto it = entry.fields.find("embedding");
    if (it == entry.fields.end() || !it->is_array() || it->empty()) return;

    std::vector<float> embedding;
    embedding.reserve(it->size());
    for (const auto& v : *it) {
        if (!v.is_number()) return;   // 格式不对就整条跳过，不要写进半个向量
        embedding.push_back(v.get<float>());
    }

    const auto ts_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        entry.timestamp.time_since_epoch()).count();
    const std::string log_id = std::to_string(ts_ns) + "-" +
                               std::to_string(m_entry_counter.fetch_add(1));

    nlohmann::json metadata = {
        {"level",   to_string(entry.level)},
        {"module",  entry.module},
        {"source",  entry.source},
        {"message", entry.message},
    };
    m_vector_store.insert(log_id, embedding, metadata.dump());
}

bool DaemonCore::do_start_watcher() {
    const auto snapshot = m_plugin_loader.snapshot();

    std::vector<std::string> all_patterns;
    for (const auto* c : snapshot->collectors) {
        auto patterns = c->watch_patterns();
        all_patterns.insert(all_patterns.end(), patterns.begin(), patterns.end());
    }
    const bool from_plugins = !all_patterns.empty();

    // 无插件或插件没声明监听目标时，从配置文件回退
    if (all_patterns.empty() && m_config.is_loaded()) {
        const auto& json = m_config.data();
        if (json.contains("log_dirs") && json["log_dirs"].is_array()) {
            const auto patterns = json.value("watch_patterns", nlohmann::json::array());
            for (const auto& dir : json["log_dirs"]) {
                if (!dir.is_string()) continue;
                if (patterns.empty()) {
                    all_patterns.push_back(dir.get<std::string>() + "/*.log");
                } else {
                    for (const auto& pat : patterns) {
                        if (!pat.is_string()) continue;
                        all_patterns.push_back(
                            dir.get<std::string>() + "/" + pat.get<std::string>());
                    }
                }
            }
        }
    }

    if (all_patterns.empty()) {
        qWarning() << "FileWatcher: nothing to watch "
                      "(no collector watch_patterns and no log_dirs in config)";
    } else {
        // 无论走哪条分支都要把实际监控的目标打出来。插件声明的 watch_patterns
        // 会盖掉 log_dirs，不打印的话「明明配了 log_dirs 却什么都没采到」
        // 这种情况完全无从排查。
        qInfo().noquote()
            << QString("FileWatcher: watching %1 pattern(s) from %2:")
               .arg(all_patterns.size())
               .arg(from_plugins ? "collector watch_patterns (overrides log_dirs)"
                                 : "config log_dirs");
        for (const auto& pattern : all_patterns) {
            qInfo().noquote() << QString("  - %1").arg(QString::fromStdString(pattern));
        }
        m_file_watcher.add_patterns(all_patterns);
    }

    m_file_watcher.start();
    return m_file_watcher.is_running();
}

void DaemonCore::do_stop() {
    // 顺序很重要：先停止新数据进入，再排空在途数据，最后才卸插件、关库
    m_file_watcher.stop();
    m_pipeline.flush();
    m_pipeline.stop_workers();

    if (m_health_timer) {
        m_health_timer->stop();
        delete m_health_timer;
        m_health_timer = nullptr;
    }

    if (m_dbus_adaptor) {
        QDBusConnection::sessionBus().unregisterObject("/com/logmind/Daemon");
        QDBusConnection::sessionBus().unregisterService("com.logmind.Daemon");
        delete m_dbus_adaptor;
        m_dbus_adaptor = nullptr;
    }

    m_plugin_loader.unload_all();

    m_vector_store.close();
    m_vector_store_ready = false;
}

static size_t buffer_capacity_of(const ConfigMgr& config) {
    const int capacity = config.log_buffer_capacity();
    return (capacity > 0) ? static_cast<size_t>(capacity) : size_t{50000};
}

static VectorStoreConfig build_vector_config(const ConfigMgr& config) {
    VectorStoreConfig vc;
    const auto vs = config.data().value("vector_store", nlohmann::json::object());
    if (vs.is_object()) {
        vc.db_path              = vs.value("db_path", vc.db_path);
        vc.hnsw_m               = vs.value("hnsw_m", vc.hnsw_m);
        vc.hnsw_ef_construction = vs.value("hnsw_ef_construction", vc.hnsw_ef_construction);
        vc.hnsw_ef_search       = vs.value("hnsw_ef_search", vc.hnsw_ef_search);
        vc.dim                  = vs.value("dim", vc.dim);
        vc.max_elements         = vs.value("max_elements", vc.max_elements);
        vc.persist              = vs.value("persist", vc.persist);
        vc.auto_trim_threshold  = vs.value("auto_trim_threshold", vc.auto_trim_threshold);
    }
    return vc;
}

static Pipeline::AlertRule build_alert_rule(const ConfigMgr& config) {
    Pipeline::AlertRule rule;
    const auto alerts = config.data().value("alerts", nlohmann::json::object());
    if (alerts.is_object()) {
        rule.min_level = log_level_from_string(
            alerts.value("min_level", std::string("ERROR")));
        rule.dedup_window = std::chrono::seconds(
            alerts.value("dedup_window_seconds", int64_t{60}));
        rule.max_alerts_per_minute =
            alerts.value("max_alerts_per_minute", rule.max_alerts_per_minute);
        rule.dedup_cache_size =
            alerts.value("dedup_cache_size", rule.dedup_cache_size);
    }
    return rule;
}
