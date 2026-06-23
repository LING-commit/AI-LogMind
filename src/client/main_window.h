#pragma once
#include <QMainWindow>
#include <QSplitter>
#include <QTimer>
#include <QSettings>

class DBusClient;
class LogModel;
class LogFilterModel;
class LogView;
class SearchBar;
class LogDetailPanel;
class PluginPanel;
class AlertPanel;
class DashboardWidget;
class StatusBarWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onConnected();
    void onDisconnected();
    void onLogEntryReceived(QVariantMap entry);
    void onAlertTriggered(QVariantMap alert);
    void onAnalysisRequested(const QStringList& logIds);
    void onAnalysisComplete(QVariantMap result);
    void onThemeToggled();

private:
    void setupUI();
    void setupConnections();
    void applyTheme();
    void initDaemonConnection();

    DBusClient*         m_dbus;
    LogModel*           m_log_model;
    LogFilterModel*     m_filter_model;

    QSplitter*          m_right_splitter;
    LogView*            m_log_view;
    SearchBar*          m_search_bar;
    LogDetailPanel*     m_detail_panel;
    PluginPanel*        m_plugin_panel;
    AlertPanel*         m_alert_panel;
    DashboardWidget*    m_dashboard;
    StatusBarWidget*    m_status_widget;

    QAction*            m_theme_action;
    QTimer*             m_stats_timer;
    bool                m_dark_theme = true;
};
