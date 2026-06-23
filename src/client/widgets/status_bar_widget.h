#pragma once
#include <QWidget>
#include <QLabel>

class StatusBarWidget : public QWidget {
    Q_OBJECT
public:
    explicit StatusBarWidget(QWidget* parent = nullptr);
    void setConnected(bool connected);

public slots:
    void onStatusReceived(const QVariantMap& status);

private:
    QLabel* m_connection_label;
    QLabel* m_plugin_label;
    QLabel* m_memory_label;
};
