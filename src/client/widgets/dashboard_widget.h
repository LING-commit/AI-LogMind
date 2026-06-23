#pragma once
#include <QWidget>
#include <QLabel>

class DashboardWidget : public QWidget {
    Q_OBJECT
public:
    explicit DashboardWidget(QWidget* parent = nullptr);

public slots:
    void onStatsReceived(const QVariantMap& stats);
    void setErrorCount(int count);

private:
    QLabel* m_qps_label;
    QLabel* m_processed_label;
    QLabel* m_errors_label;
    QLabel* m_alerts_label;
};
