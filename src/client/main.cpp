#include <QApplication>
#include <QFile>
#include <QIcon>
#include "main_window.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("LogMind");
    app.setOrganizationName("logmind");
    app.setApplicationVersion("0.1.0");

    QFile qss(":/styles/resources/style.qss");
    if (qss.open(QFile::ReadOnly | QFile::Text)) {
        app.setStyleSheet(qss.readAll());
        qss.close();
    }

    MainWindow w;
    w.setWindowTitle("LogMind \u667a\u80fd\u65e5\u5fd7\u5206\u6790\u5e73\u53f0");
    w.resize(1400, 900);
    w.show();
    w.raise();
    w.activateWindow();

    return app.exec();
}
