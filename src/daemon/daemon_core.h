#pragma once
#include <QObject>
#include <QTimer>
#include <atomic>
#include <cstdint>
#include <logmind/version.h>
#include <logmind/plugin_loader.h>
#include <logmind/pipeline.h>
#include <logmind/file_watcher.h>
#include <logmind/log_buffer.h>
#include <logmind/vector_store.h>
#include <logmind/config_mgr.h>

using namespace logmind;

class DaemonAdaptor;
class DaemonCore : public QObject {
    Q_OBJECT
public:
    explicit DaemonCore(const ConfigMgr& config, QObject* parent = nullptr);
    ~DaemonCore() override;

    void start();
    void stop();
    bool reload_plugins();
    bool reload_config();

    DaemonState state() const { return m_state; }

    // 供 D-Bus 适配器做相似日志检索
    VectorStore& vector_store() { return m_vector_store; }

signals:
    void state_changed(DaemonState old_state, DaemonState new_state);
    void plugin_load_progress(int loaded, int total);
    void finished();

private:
    void transition_to(DaemonState new_state);
    bool do_load_plugins();
    bool do_init_pipeline();
    bool do_register_dbus();
    bool do_start_watcher();
    void do_stop();

    // 把带 embedding 的条目写进向量库。embedding 由 Analyzer 插件通过
    // entry.fields["embedding"]（数字数组）提供——守护进程本身不做向量化。
    void store_embedding(const LogEntry& entry);

    DaemonState     m_state = DaemonState::INIT;
    ConfigMgr       m_config;
    PluginLoader    m_plugin_loader;
    LogBuffer       m_log_buffer;
    Pipeline        m_pipeline;
    FileWatcher     m_file_watcher;
    VectorStore     m_vector_store;
    DaemonAdaptor*  m_dbus_adaptor = nullptr;
    QTimer*         m_health_timer = nullptr;

    bool                  m_vector_store_ready = false;
    std::atomic<uint64_t> m_entry_counter{0};
    std::chrono::steady_clock::time_point m_started_at{};
};
