#include "plugin_panel.h"
#include "../dbus_client.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QScrollArea>
#include <QFrame>

PluginPanel::PluginPanel(DBusClient* dbus, QWidget* parent)
    : QWidget(parent), m_dbus(dbus)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* topBar = new QHBoxLayout;
    auto* title = new QLabel("<b>\u63d2\u4ef6\u7ba1\u7406</b>", this);
    m_reload_btn = new QPushButton("\U0001F504 \u91cd\u8f7d", this);
    m_reload_btn->setFixedHeight(28);
    connect(m_reload_btn, &QPushButton::clicked, this, &PluginPanel::onReloadClicked);

    m_info_label = new QLabel("\u52a0\u8f7d 0/0 \u4e2a\u63d2\u4ef6", this);
    m_info_label->setStyleSheet("color: #8B949E; font-size: 11px;");

    topBar->addWidget(title);
    topBar->addStretch();
    topBar->addWidget(m_info_label);
    topBar->addWidget(m_reload_btn);
    layout->addLayout(topBar);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    m_list = new QListView(this);
    m_list->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(m_list);
    layout->addWidget(scroll, 1);
}

void PluginPanel::onPluginsListed(const QVector<QVariantMap>& plugins) {
    m_plugins.clear();
    for (const auto& p : plugins) {
        m_plugins.append({
            p.value("name").toString(),
            p.value("version").toString(),
            p.value("description").toString(),
            p.value("type").toString(),
            p.value("loaded").toBool(),
            p.value("enabled").toBool()
        });
    }

    auto* scroll = findChild<QScrollArea*>();
    if (!scroll) return;

    auto* container = new QWidget;
    auto* colLayout = new QVBoxLayout(container);
    colLayout->setContentsMargins(0, 0, 0, 0);
    colLayout->setSpacing(6);

    int loaded_count = 0;
    for (const auto& entry : m_plugins) {
        if (entry.loaded) loaded_count++;

        auto* card = new QFrame(container);
        card->setFrameShape(QFrame::StyledPanel);
        card->setStyleSheet(
            "QFrame { background: #161B22; border: 1px solid #30363D;"
            "  border-radius: 6px; padding: 8px; }"
        );

        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setSpacing(4);

        auto* header = new QHBoxLayout;
        auto* nameLabel = new QLabel(
            QString("<b>%1</b>").arg(entry.name), card);
        auto* verLabel = new QLabel(
            QString("<span style='color:#8B949E;font-size:10px;'>%1</span>")
            .arg(entry.version), card);

        auto* typeLabel = new QLabel(entry.type_name, card);
        QString typeColor = (entry.type_name == "Collector") ? "#58A6FF" :
                            (entry.type_name == "Analyzer")  ? "#7EE787" : "#FFA657";
        typeLabel->setStyleSheet(
            QString("background:%1;color:#0D1117;padding:1px 6px;"
                    "border-radius:3px;font-size:10px;font-weight:bold;")
            .arg(typeColor));
        typeLabel->setFixedHeight(18);

        header->addWidget(nameLabel);
        header->addWidget(verLabel);
        header->addWidget(typeLabel);
        header->addStretch();

        auto* descLabel = new QLabel(entry.description, card);
        descLabel->setWordWrap(true);
        descLabel->setStyleSheet("color: #8B949E; font-size: 11px;");

        auto* footer = new QHBoxLayout;
        auto* toggle = new QCheckBox("\u542f\u7528", card);
        toggle->setChecked(entry.enabled);
        toggle->setEnabled(entry.loaded);
        connect(toggle, &QCheckBox::toggled, this, [this, name = entry.name](bool checked) {
            if (checked) m_dbus->enablePlugin(name);
            else         m_dbus->disablePlugin(name);
        });

        auto* statusLabel = new QLabel(
            entry.loaded ? "\U0001F7E2 \u5df2\u52a0\u8f7d"
                         : (entry.enabled ? "\U0001F7E1 \u5f85\u52a0\u8f7d"
                                          : "\U0001F534 \u5df2\u7981\u7528"),
            card);
        statusLabel->setStyleSheet("font-size: 11px;");

        footer->addWidget(toggle);
        footer->addStretch();
        footer->addWidget(statusLabel);

        cardLayout->addLayout(header);
        cardLayout->addWidget(descLabel);
        cardLayout->addLayout(footer);

        colLayout->addWidget(card);
    }

    colLayout->addStretch();
    scroll->setWidget(container);

    m_info_label->setText(QString("\u52a0\u8f7d %1/%2 \u4e2a\u63d2\u4ef6")
                         .arg(loaded_count).arg(m_plugins.size()));
}

void PluginPanel::onPluginToggled(const QString& name, bool enabled) {
    for (auto& p : m_plugins) {
        if (p.name == name) {
            p.enabled = enabled;
            break;
        }
    }
}

void PluginPanel::onPluginsReloaded(bool success) {
    if (success) {
        m_dbus->listPlugins();
        m_info_label->setText("\u63d2\u4ef6\u91cd\u8f7d\u6210\u529f \u2713");
    } else {
        m_info_label->setText("\u63d2\u4ef6\u91cd\u8f7d\u5931\u8d25 \u2717");
    }
}

void PluginPanel::onReloadClicked() {
    m_dbus->reloadPlugins();
}
