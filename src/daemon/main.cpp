#include <QCoreApplication>
#include <QCommandLineParser>
#include <QLoggingCategory>
#include <QDebug>
#include <signal.h>
#include "daemon_core.h"
#include <logmind/config_mgr.h>

static DaemonCore* g_daemon = nullptr;

static void signal_handler(int sig) {
    if (g_daemon) {
        qInfo() << "Received signal" << sig << ", shutting down...";
        g_daemon->stop();
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("logmind-daemon");
    app.setApplicationVersion("0.1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("LogMind Intelligent Log Analysis Daemon");
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addOption({
        {"c", "config"},
        "Configuration file path",
        "path",
        "/etc/logmind/config.json"
    });
    parser.addOption({
        {"p", "plugins-dir"},
        "Plugins directory",
        "path",
        "/usr/lib/logmind/plugins"
    });
    parser.addOption({
        {"V", "verbose"},
        "Verbose logging (repeat for debug level)"
    });
    parser.process(app);

    // ── 日志级别 ──
    if (parser.isSet("verbose")) {
        QLoggingCategory::setFilterRules("logmind.*.debug=true");
    }

    // ── 配置 ──
    ConfigMgr config;
    if (!config.load(parser.value("config").toStdString())) {
        qWarning() << "Failed to load config, using defaults:"
                    << config.last_error().c_str();
        config.set_defaults();
    }

    // ── 安装信号处理 ──
    signal(SIGTERM, signal_handler);
    signal(SIGINT,  signal_handler);
    signal(SIGHUP,  [](int) {
        if (g_daemon) {
            qInfo() << "SIGHUP received, reloading...";
            g_daemon->reload_plugins();
        }
    });

    // ── 启动 daemon ──
    DaemonCore daemon(config);
    g_daemon = &daemon;

    QObject::connect(&daemon, &DaemonCore::finished, &app, &QCoreApplication::quit);

    daemon.start();
    if (daemon.state() == DaemonState::FAILED) {
        qCritical() << "Daemon failed to start";
        return 1;
    }

    int ret = app.exec();

    g_daemon = nullptr;
    return ret;
}
