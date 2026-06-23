#pragma once
#include <QWidget>
#include <QListWidget>
#include <QVector>
#include <QVariantMap>

class AlertPanel : public QWidget {
    Q_OBJECT
public:
    explicit AlertPanel(QWidget* parent = nullptr);

    void addAlert(const QVariantMap& alert);
    void clear();

private:
    struct AlertEntry {
        QString id;
        QString rule_name;
        QString severity;
        QString summary;
        QString timeStr;
    };

    QListWidget* m_list;
    QVector<AlertEntry> m_alerts;
};
