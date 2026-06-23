#pragma once
#include <QWidget>
#include <QListView>
#include <QPushButton>
#include <QLabel>
#include <QVector>
#include <QVariantMap>

class DBusClient;

class PluginPanel : public QWidget {
    Q_OBJECT
public:
    explicit PluginPanel(DBusClient* dbus, QWidget* parent = nullptr);

public slots:
    void onPluginsListed(const QVector<QVariantMap>& plugins);
    void onPluginToggled(const QString& name, bool enabled);
    void onPluginsReloaded(bool success);

private slots:
    void onReloadClicked();

private:
    DBusClient*     m_dbus;
    QListView*      m_list;
    QPushButton*    m_reload_btn;
    QLabel*         m_info_label;

    struct PluginEntry {
        QString name;
        QString version;
        QString description;
        QString type_name;
        bool    loaded;
        bool    enabled;
    };
    QVector<PluginEntry> m_plugins;
};
