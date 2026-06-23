#include "daemon_core.h"
#include "dbus_service.h"
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDebug>

static VectorStoreConfig build_vector_config(const ConfigMgr& config);

DaemonCore::DaemonCore(const ConfigMgr& config, QObject* parent)
    : QObject(parent)
    , m_config(config)
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
}

DaemonCore::~DaemonCore() {
    do_stop();
}

void DaemonCore::start() {
    qInfo() << "DaemonCore starting...";

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

    if (!do_start_watcher()) {
        qWarning() << "File watcher start failed, continuing without file monitoring";
    }

    if (!m_vector_store.open()) {
        qWarning() << "VectorStore open failed, continuing without vector search";
    }

    transition_to(DaemonState::RUNNING);
    qInfo() << "DaemonCore started successfully";

    m_health_timer = new QTimer(this);
    m_health_timer->setInterval(30000);
    connect(m_health_timer, &QTimer::timeout, this, [this]() {
        auto s = m_pipeline.stats();
        Q_UNUSED(s);
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
    m_plugin_loader.unload_all();
    bool ok = do_load_plugins();
    transition_to(ok ? DaemonState::RUNNING : DaemonState::FAILED);
    return ok;
}

bool DaemonCore::reload_config() {
    bool ok = m_config.reload();
    if (ok) {
        qInfo() << "Configuration reloaded";
    }
    return ok;
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
    bool ok = m_plugin_loader.load_directory(plugins_dir);

    auto collectors = m_plugin_loader.collectors();
    auto analyzers  = m_plugin_loader.analyzers();
    auto alerts     = m_plugin_loader.alerts();

    qInfo() << "Plugins loaded:"
            << collectors.size() << "collectors,"
            << analyzers.size() << "analyzers,"
            << alerts.size() << "alerts";

    emit plugin_load_progress(m_plugin_loader.loaded_count(),
                              m_plugin_loader.loaded_count());

    return ok;
}

bool DaemonCore::do_init_pipeline() {
    (void)m_pipeline;  // Already initialized with m_plugin_loader ref
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

    // Connect pipeline output → LogBuffer (completes InjectLog→D-Bus signal chain)
    m_pipeline.set_output_callback([this](std::vector<LogEntry> entries) {
        for (auto& entry : entries) {
            m_log_buffer.push(std::move(entry));
        }
    });

    m_dbus_adaptor = new DaemonAdaptor(&m_pipeline, &m_plugin_loader,
                                        &m_log_buffer,
                                        m_config.plugins_dir(), this);

    if (!bus.registerObject("/com/logmind/Daemon", this)) {
        qCritical() << "Failed to register D-Bus object:"
                     << bus.lastError().message();
        return false;
    }

    qInfo() << "D-Bus service registered: com.logmind.Daemon";
    return true;
}

bool DaemonCore::do_start_watcher() {
    auto collectors = m_plugin_loader.collectors();
    std::vector<std::string> all_patterns;
    for (const auto* c : collectors) {
        auto pats = c->watch_patterns();
        all_patterns.insert(all_patterns.end(), pats.begin(), pats.end());
    }

    // 无插件时从配置文件回退
    if (all_patterns.empty() && m_config.is_loaded()) {
        auto& json = m_config.data();
        if (json.contains("log_dirs") && json["log_dirs"].is_array()) {
            auto patterns = json.value("watch_patterns", nlohmann::json::array());
            for (const auto& dir : json["log_dirs"]) {
                if (patterns.empty()) {
                    all_patterns.push_back(dir.get<std::string>() + "/*.log");
                } else {
                    for (const auto& pat : patterns) {
                        all_patterns.push_back(
                            dir.get<std::string>() + "/" + pat.get<std::string>());
                    }
                }
            }
        }
        qInfo().noquote() << QString("FileWatcher: watching %1 patterns from config")
                            .arg(all_patterns.size());
    }

    if (!all_patterns.empty()) {
        m_file_watcher.add_patterns(all_patterns);
    }
    m_file_watcher.start();
    return m_file_watcher.is_running();
}

void DaemonCore::do_stop() {
    m_file_watcher.stop();
    m_plugin_loader.unload_all();

    if (m_dbus_adaptor) {
        QDBusConnection::sessionBus().unregisterObject("/com/logmind/Daemon");
        QDBusConnection::sessionBus().unregisterService("com.logmind.Daemon");
        m_dbus_adaptor = nullptr;
    }

    if (m_health_timer) {
        m_health_timer->stop();
        delete m_health_timer;
        m_health_timer = nullptr;
    }

    m_vector_store.close();
}

static VectorStoreConfig build_vector_config(const ConfigMgr& config) {
    VectorStoreConfig vc;
    auto vs = config.data().value("vector_store", nlohmann::json::object());
    if (!vs.is_null()) {
        vc.db_path              = vs.value("db_path", vc.db_path);
        vc.hnsw_m               = vs.value("hnsw_m", vc.hnsw_m);
        vc.hnsw_ef_construction = vs.value("hnsw_ef_construction", vc.hnsw_ef_construction);
        vc.max_elements         = vs.value("max_elements", vc.max_elements);
        vc.persist              = vs.value("persist", vc.persist);
    }
    return vc;
}
