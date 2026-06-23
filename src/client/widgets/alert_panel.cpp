#include "alert_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QDateTime>
#include <QMainWindow>
#include <QStatusBar>

AlertPanel::AlertPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    auto* header = new QHBoxLayout;
    header->addWidget(new QLabel("<b>\u544a\u8b66\u5386\u53f2</b>", this));
    header->addStretch();
    auto* clearBtn = new QPushButton("\u6e05\u9664", this);
    clearBtn->setFixedHeight(24);
    connect(clearBtn, &QPushButton::clicked, this, &AlertPanel::clear);
    header->addWidget(clearBtn);
    layout->addLayout(header);

    m_list = new QListWidget(this);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);

    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        int idx = m_list->row(item);
        if (idx >= 0 && idx < m_alerts.size()) {
            auto& alert = m_alerts[idx];
            auto* mainWin = qobject_cast<QMainWindow*>(window());
            if (mainWin) {
                mainWin->statusBar()->showMessage(
                    QString("\u544a\u8b66: %1 [%2]").arg(alert.summary, alert.severity), 10000);
            }
        }
    });

    layout->addWidget(m_list, 1);
}

void AlertPanel::addAlert(const QVariantMap& alert) {
    AlertEntry entry;
    entry.id        = alert.value("id").toString();
    entry.rule_name = alert.value("rule_name").toString();
    entry.severity  = alert.value("severity").toString();
    entry.summary   = alert.value("summary").toString();
    entry.timeStr   = QDateTime::currentDateTime().toString("HH:mm:ss");

    m_alerts.prepend(entry);
    if (m_alerts.size() > 200) m_alerts.removeLast();

    QColor bgColor;
    if (entry.severity == "critical") bgColor = QColor("#3D1A1A");
    else if (entry.severity == "warning") bgColor = QColor("#3D2E1A");
    else bgColor = QColor("#1A2D3D");

    auto* item = new QListWidgetItem(
        QString("\u26a0  [%1] %2").arg(entry.timeStr, entry.summary));
    item->setData(Qt::UserRole, entry.id);
    item->setBackground(bgColor);

    m_list->insertItem(0, item);

    if (m_list->count() > 200) {
        delete m_list->takeItem(m_list->count() - 1);
    }
}

void AlertPanel::clear() {
    m_list->clear();
    m_alerts.clear();
}
