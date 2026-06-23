#include "dashboard_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>

DashboardWidget::DashboardWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    auto* group = new QGroupBox("\u5b9e\u65f6\u7edf\u8ba1", this);
    auto* form  = new QFormLayout(group);

    m_qps_label       = new QLabel("--", this);
    m_processed_label = new QLabel("--", this);
    m_errors_label    = new QLabel("--", this);
    m_alerts_label    = new QLabel("--", this);

    form->addRow("\u5904\u7406\u901f\u5ea6:", m_qps_label);
    form->addRow("\u5df2\u5904\u7406:", m_processed_label);
    form->addRow("\u9519\u8bef:", m_errors_label);
    form->addRow("\u544a\u8b66:", m_alerts_label);

    layout->addWidget(group);
    layout->addStretch();
}

void DashboardWidget::setErrorCount(int count) {
    m_errors_label->setText(QString::number(count));
}

void DashboardWidget::onStatsReceived(const QVariantMap& stats) {
    m_qps_label->setText(
        QString("%1 \u6761/s").arg(stats.value("qps").toDouble(), 0, 'f', 1));
    m_processed_label->setText(
        QString::number(stats.value("total_processed").toLongLong()));
    // 错误数来自 LogModel::errorCountChanged，不由 daemon stats 覆盖
    m_alerts_label->setText(
        QString::number(stats.value("total_alerts").toLongLong()));
}
