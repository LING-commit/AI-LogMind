#include "main_window.h"
#include "dbus_client.h"
#include "models/log_model.h"
#include "models/log_filter_model.h"
#include "widgets/log_view.h"
#include "widgets/search_bar.h"
#include "widgets/log_detail_panel.h"
#include "widgets/plugin_panel.h"
#include "widgets/alert_panel.h"
#include "widgets/dashboard_widget.h"
#include "widgets/status_bar_widget.h"

#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QApplication>
#include <QMessageBox>
#include <QFile>
#include <QSettings>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_dbus(new DBusClient(this))
    , m_log_model(new LogModel(this))
    , m_filter_model(new LogFilterModel(this))
    , m_stats_timer(new QTimer(this))
{
    m_filter_model->setSourceModel(m_log_model);
    m_filter_model->setSortRole(TimestampRole);
    m_filter_model->sort(0, Qt::DescendingOrder);

    setupUI();
    setupConnections();
    initDaemonConnection();

    QSettings settings;
    m_dark_theme = settings.value("theme/dark", true).toBool();
    applyTheme();

    m_stats_timer->setInterval(5000);
    connect(m_stats_timer, &QTimer::timeout, m_dbus, &DBusClient::fetchStats);
    m_stats_timer->start();

    setWindowTitle("LogMind \u667a\u80fd\u65e5\u5fd7\u5206\u6790\u5e73\u53f0 - \u672a\u8fde\u63a5");
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI() {
    m_log_view      = new LogView(this);
    m_log_view->setFilterModel(m_filter_model);

    m_search_bar    = new SearchBar(this);
    m_detail_panel  = new LogDetailPanel(this);
    m_plugin_panel  = new PluginPanel(m_dbus, this);
    m_plugin_panel->setMinimumWidth(260);
    m_alert_panel   = new AlertPanel(this);
    m_alert_panel->setMinimumWidth(260);
    m_dashboard     = new DashboardWidget(this);

    m_status_widget = new StatusBarWidget(this);
    statusBar()->addPermanentWidget(m_status_widget, 1);

    auto* logContainer = new QWidget;
    auto* logLayout = new QVBoxLayout(logContainer);
    logLayout->setContentsMargins(0, 0, 0, 0);
    logLayout->setSpacing(0);
    logLayout->addWidget(m_search_bar);
    logLayout->addWidget(m_log_view, 1);
    logLayout->addWidget(m_detail_panel);

    auto* rightTab = new QTabWidget(this);
    rightTab->setTabPosition(QTabWidget::North);
    rightTab->addTab(m_dashboard,     "\u4eea\u8868\u76d8");
    rightTab->addTab(m_plugin_panel,  "\u63d2\u4ef6");
    rightTab->addTab(m_alert_panel,   "\u544a\u8b66");
    rightTab->setMinimumWidth(280);

    m_right_splitter = new QSplitter(Qt::Vertical, this);
    m_right_splitter->addWidget(logContainer);
    m_right_splitter->addWidget(rightTab);
    m_right_splitter->setStretchFactor(0, 3);
    m_right_splitter->setStretchFactor(1, 2);

    setCentralWidget(m_right_splitter);

    auto* fileMenu = menuBar()->addMenu("\u6587\u4ef6(&F)");
    fileMenu->addAction("\u91cd\u65b0\u8fde\u63a5 Daemon", this, &MainWindow::initDaemonConnection);
    fileMenu->addSeparator();
    fileMenu->addAction("\u9000\u51fa(&Q)", QKeySequence::Quit, qApp, &QApplication::quit);

    auto* viewMenu = menuBar()->addMenu("\u89c6\u56fe(&V)");
    m_theme_action = viewMenu->addAction("\u6df1\u8272/\u6d45\u8272\u5207\u6362");
    connect(m_theme_action, &QAction::triggered, this, &MainWindow::onThemeToggled);
    viewMenu->addAction("\u6e05\u7a7a\u65e5\u5fd7", m_log_model, &LogModel::clear);
    viewMenu->addSeparator();
    viewMenu->addAction("\u63d2\u4ef6\u7ba1\u7406", [this]{
        auto* tab = findChild<QTabWidget*>();
        if (tab) tab->setCurrentIndex(1);
    });

    auto* helpMenu = menuBar()->addMenu("\u5e2e\u52a9(&H)");
    helpMenu->addAction("\u5173\u4e8e LogMind", [this]{
        QMessageBox::about(this, "\u5173\u4e8e LogMind",
            "LogMind v0.1.0\n\u667a\u80fd\u65e5\u5fd7\u5206\u6790\u5e73\u53f0");
    });
}

void MainWindow::setupConnections() {
    connect(m_dbus, &DBusClient::connected,         this, &MainWindow::onConnected);
    connect(m_dbus, &DBusClient::disconnected,      this, &MainWindow::onDisconnected);
    connect(m_dbus, &DBusClient::connectionFailed,  this, [this](const QString& err) {
        statusBar()->showMessage("\u8fde\u63a5\u5931\u8d25: " + err, 5000);
    });

    connect(m_dbus, &DBusClient::logEntryReceived,   this, &MainWindow::onLogEntryReceived);
    connect(m_dbus, &DBusClient::logsQueried,         this, [this](QVector<QVariantMap> entries) {
        QVector<LogItem> items;
        for (const auto& m : entries) {
            LogItem li;
            li.id           = m.value("id").toString();
            li.source       = m.value("source").toString();
            li.raw          = m.value("raw").toString();
            li.timestamp_ns = static_cast<qint64>(m.value("timestamp").toULongLong());
            li.level        = m.value("level").toString();
            li.message      = m.value("message").toString();
            li.module       = m.value("module").toString();
            li.fields       = m.value("fields").toMap();
            li.tags         = m.value("tags").toMap();
            li.meta         = m.value("meta").toMap();
            items.append(li);
            m_search_bar->addSource(li.source);
        }
        m_log_model->appendLogs(items);
    });

    connect(m_dbus, &DBusClient::alertTriggered,     this, &MainWindow::onAlertTriggered);
    connect(m_dbus, &DBusClient::pluginsListed,      m_plugin_panel, &PluginPanel::onPluginsListed);
    connect(m_dbus, &DBusClient::pluginToggled,       m_plugin_panel, &PluginPanel::onPluginToggled);
    connect(m_dbus, &DBusClient::pluginsReloaded,     m_plugin_panel, &PluginPanel::onPluginsReloaded);
    connect(m_dbus, &DBusClient::statusReceived,      m_status_widget, &StatusBarWidget::onStatusReceived);
    connect(m_dbus, &DBusClient::statsReceived,       m_dashboard, &DashboardWidget::onStatsReceived);
    connect(m_dbus, &DBusClient::analysisComplete,    this, &MainWindow::onAnalysisComplete);
    connect(m_dbus, &DBusClient::analysisFailed,      this, [this](const QString& err) {
        statusBar()->showMessage("AI \u5206\u6790\u5931\u8d25: " + err, 5000);
    });

    connect(m_search_bar, &SearchBar::filterChanged,      m_filter_model, &LogFilterModel::setFilterText);
    connect(m_search_bar, &SearchBar::levelFilterChanged, m_filter_model, &LogFilterModel::setFilterLevel);
    connect(m_search_bar, &SearchBar::sourceFilterChanged, m_filter_model, &LogFilterModel::setFilterSource);
    connect(m_search_bar, &SearchBar::timeRangeChanged,   m_filter_model, &LogFilterModel::setTimeRange);
    connect(m_log_view, &LogView::analysisRequested, this, &MainWindow::onAnalysisRequested);
    connect(m_log_view, &LogView::logDoubleClicked, this, [this](const QString&) {
        auto idx = m_log_view->currentIndex();
        if (!idx.isValid()) return;
        auto item = m_filter_model->sourceItem(idx.row());
        QVariantMap entry;
        entry["id"]         = item.id;
        entry["source"]     = item.source;
        entry["raw"]        = item.raw;
        entry["timestamp"]  = static_cast<qulonglong>(item.timestamp_ns);
        entry["level"]      = item.level;
        entry["message"]    = item.message;
        entry["module"]     = item.module;
        entry["ai_summary"] = item.ai_summary;
        m_detail_panel->showLog(entry);
    });
}

void MainWindow::initDaemonConnection() {
    statusBar()->showMessage("\u6b63\u5728\u8fde\u63a5 LogMind Daemon...");
    m_dbus->connectToDaemon();
}

void MainWindow::onConnected() {
    setWindowTitle("LogMind \u667a\u80fd\u65e5\u5fd7\u5206\u6790\u5e73\u53f0 - \u5df2\u8fde\u63a5");
    statusBar()->showMessage("\u5df2\u8fde\u63a5\u5230 LogMind Daemon");
    m_status_widget->setConnected(true);
}

void MainWindow::onDisconnected() {
    setWindowTitle("LogMind \u667a\u80fd\u65e5\u5fd7\u5206\u6790\u5e73\u53f0 - \u672a\u8fde\u63a5");
    statusBar()->showMessage("Daemon \u5df2\u65ad\u5f00\uff0c\u6b63\u5728\u91cd\u8fde...");
    m_status_widget->setConnected(false);
}

void MainWindow::onLogEntryReceived(QVariantMap entry) {
    LogItem item;
    item.id          = entry.value("id").toString();
    item.source      = entry.value("source").toString();
    item.raw         = entry.value("raw").toString();
    item.timestamp_ns= static_cast<qint64>(entry.value("timestamp").toULongLong());
    item.level       = entry.value("level").toString();
    item.message     = entry.value("message").toString();
    item.module      = entry.value("module").toString();
    item.fields      = entry.value("fields").toMap();
    item.tags        = entry.value("tags").toMap();
    item.meta        = entry.value("meta").toMap();

    m_log_model->appendLog(item);
    m_search_bar->addSource(item.source);
}

void MainWindow::onAlertTriggered(QVariantMap alert) {
    m_alert_panel->addAlert(alert);
    statusBar()->showMessage(
        "\u544a\u8b66: " + alert.value("summary").toString(), 8000);
}

void MainWindow::onAnalysisRequested(const QStringList& logIds) {
    m_dbus->analyzeLogs(logIds);
    statusBar()->showMessage("AI \u5206\u6790\u5df2\u8bf7\u6c42...");
}

void MainWindow::onAnalysisComplete(QVariantMap result) {
    QString summary = result.value("summary").toString();
    statusBar()->showMessage("AI \u5206\u6790\u5b8c\u6210: " + summary);
}

void MainWindow::onThemeToggled() {
    m_dark_theme = !m_dark_theme;
    QSettings settings;
    settings.setValue("theme/dark", m_dark_theme);
    applyTheme();
}

void MainWindow::applyTheme() {
    QFile qss(m_dark_theme ? ":/styles/resources/style.qss" : ":/styles/resources/style_light.qss");
    if (qss.open(QFile::ReadOnly | QFile::Text)) {
        qApp->setStyleSheet(qss.readAll());
        qss.close();
    }
}
