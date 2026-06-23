#pragma once
#include <QDBusAbstractAdaptor>
#include <QDBusVariant>
#include <QDBusMessage>
#include <QSet>
#include <logmind/log_entry.h>

namespace logmind {
class Pipeline;
class PluginLoader;
class LogBuffer;
}

QVariantMap log_entry_to_varmap(const logmind::LogEntry& entry);
logmind::LogEntry varmap_to_log_entry(const QVariantMap& map);

class DaemonAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.logmind.Daemon1")
public:
    DaemonAdaptor(logmind::Pipeline* pipeline,
                  logmind::PluginLoader* loader,
                  logmind::LogBuffer* buffer,
                  const std::string& plugins_dir,
                  QObject* parent);

public slots:
    QList<QVariantMap> QueryLogs(const QString& query, quint32 limit);
    QList<QVariantMap> QueryLogsByTime(quint64 start_ns, quint64 end_ns, quint32 limit);
    void InjectLog(const QString& source, const QString& raw_line);
    QList<QVariantMap> AnalyzeLogs(const QStringList& log_ids);
    QString GetAnalysisReport(const QString& log_id);
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
    bool check_permission(const QDBusMessage& msg);

    logmind::Pipeline*     m_pipeline;
    logmind::PluginLoader* m_loader;
    logmind::LogBuffer*    m_buffer;
    std::string            m_plugins_dir;
};

bool is_member_of_group(uid_t uid, const char* group_name);
