#include "status_bar_widget.h"
#include <QHBoxLayout>

StatusBarWidget::StatusBarWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->setSpacing(16);

    m_connection_label = new QLabel("\u26aa \u672a\u8fde\u63a5", this);
    m_plugin_label     = new QLabel("\u63d2\u4ef6: --", this);
    m_memory_label     = new QLabel("\u5185\u5b58: --", this);

    m_plugin_label->setStyleSheet("color: #8B949E;");
    m_memory_label->setStyleSheet("color: #8B949E;");

    layout->addWidget(m_connection_label);
    layout->addWidget(m_plugin_label);
    layout->addWidget(m_memory_label);
    layout->addStretch();
}

void StatusBarWidget::setConnected(bool connected) {
    if (connected) {
        m_connection_label->setText("\U0001F7E2 \u5df2\u8fde\u63a5");
    } else {
        m_connection_label->setText("\U0001F534 \u672a\u8fde\u63a5");
    }
}

void StatusBarWidget::onStatusReceived(const QVariantMap& status) {
    QString state = status.value("state").toString();
    int plugins    = status.value("plugin_count").toInt();
    double memory  = status.value("memory_mb").toDouble();

    if (state == "RUNNING") {
        m_connection_label->setText("\U0001F7E2 \u8fd0\u884c\u4e2d");
    } else if (state == "RELOADING") {
        m_connection_label->setText("\U0001F7E1 \u91cd\u8f7d\u4e2d");
    } else if (state == "FAILED") {
        m_connection_label->setText("\U0001F534 \u9519\u8bef");
    }

    m_plugin_label->setText(QString("\u63d2\u4ef6: %1").arg(plugins));
    m_memory_label->setText(QString("\u5185\u5b58: %1 MB").arg(memory, 0, 'f', 1));
}
