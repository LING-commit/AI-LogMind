#pragma once
#include <QDBusAbstractAdaptor>
#include <QDBusVariant>
#include <QDBusMessage>
#include <QSet>
#include <chrono>
#include <functional>
#include <logmind/log_entry.h>
#include <logmind/version.h>

namespace logmind {
class Pipeline;
class PluginLoader;
class LogBuffer;
class VectorStore;
}

QVariantMap log_entry_to_varmap(const logmind::LogEntry& entry);
logmind::LogEntry varmap_to_log_entry(const QVariantMap& map);

class DaemonAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.logmind.Daemon1")
    Q_PROPERTY(QString State READ state)
public:
    using ReloadFn = std::function<bool()>;
    using StateFn  = std::function<logmind::DaemonState()>;

    DaemonAdaptor(logmind::Pipeline* pipeline,
                  logmind::PluginLoader* loader,
                  logmind::LogBuffer* buffer,
                  logmind::VectorStore* vector_store,
                  ReloadFn reload_fn,
                  StateFn state_fn,
                  std::chrono::steady_clock::time_point started_at,
                  QObject* parent);

    QString state() const;

public slots:
    QList<QVariantMap> QueryLogs(const QString& query, quint32 limit);
    QList<QVariantMap> QueryLogsByTime(quint64 start_ns, quint64 end_ns, quint32 limit);
    void InjectLog(const QString& source, const QString& raw_line);
    QList<QVariantMap> AnalyzeLogs(const QStringList& log_ids);
    QString GetAnalysisReport(const QString& log_id);
    // 按 embedding 做相似日志检索。embedding 通常由 Analyzer 插件生成，
    // 守护进程本身不做向量化。
    QList<QVariantMap> SearchSimilar(const QList<double>& embedding, quint32 top_k);
    QString ListPlugins();
    QVariantMap GetPluginInfo(const QString& name);
    bool EnablePlugin(const QString& name);
    bool DisablePlugin(const QString& name);
    bool ReloadPlugins();
    QVariantMap GetStats();
    QVariantMap GetStatus();

signals:
    void LogEntryReceived(QVariantMap entry);
    void AlertTriggered(QVariantMap alert);
    void PluginLoaded(const QString& name);
    void PluginUnloaded(const QString& name);
    void DaemonStatusChanged(const QString& state);

private:
    // 注意：这两个目前是**没有接线**的桩（没有任何调用点，且
    // is_member_of_group 恒返回 true）。原样保留，见 README 的已知问题。
    bool check_permission(const QDBusMessage& msg);

    logmind::Pipeline*     m_pipeline;
    logmind::PluginLoader* m_loader;
    logmind::LogBuffer*    m_buffer;
    logmind::VectorStore*  m_vector_store;
    ReloadFn               m_reload_fn;
    StateFn                m_state_fn;
    std::chrono::steady_clock::time_point m_started_at;
};

bool is_member_of_group(uid_t uid, const char* group_name);
