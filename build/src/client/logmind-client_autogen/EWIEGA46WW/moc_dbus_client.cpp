/****************************************************************************
** Meta object code from reading C++ file 'dbus_client.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../src/client/dbus_client.h"
#include <QtCore/qmetatype.h>
#include <QtCore/QList>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'dbus_client.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.10.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN10DBusClientE_t {};
} // unnamed namespace

template <> constexpr inline auto DBusClient::qt_create_metaobjectdata<qt_meta_tag_ZN10DBusClientE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "DBusClient",
        "connected",
        "",
        "disconnected",
        "connectionFailed",
        "error",
        "logEntryReceived",
        "QVariantMap",
        "entry",
        "logsQueried",
        "QList<QVariantMap>",
        "entries",
        "pluginsListed",
        "plugins",
        "pluginToggled",
        "name",
        "enabled",
        "pluginsReloaded",
        "success",
        "analysisComplete",
        "result",
        "analysisFailed",
        "statusReceived",
        "status",
        "statsReceived",
        "stats",
        "alertTriggered",
        "alert",
        "connectToDaemon",
        "disconnectFromDaemon",
        "tryReconnect",
        "onDaemonStatusChanged",
        "state",
        "onLogEntryReceived",
        "onAlertTriggered",
        "onPluginLoaded",
        "onPluginUnloaded"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'connected'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'disconnected'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'connectionFailed'
        QtMocHelpers::SignalData<void(const QString &)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 5 },
        }}),
        // Signal 'logEntryReceived'
        QtMocHelpers::SignalData<void(QVariantMap)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 7, 8 },
        }}),
        // Signal 'logsQueried'
        QtMocHelpers::SignalData<void(QVector<QVariantMap>)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 10, 11 },
        }}),
        // Signal 'pluginsListed'
        QtMocHelpers::SignalData<void(QVector<QVariantMap>)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 10, 13 },
        }}),
        // Signal 'pluginToggled'
        QtMocHelpers::SignalData<void(const QString &, bool)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 15 }, { QMetaType::Bool, 16 },
        }}),
        // Signal 'pluginsReloaded'
        QtMocHelpers::SignalData<void(bool)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 18 },
        }}),
        // Signal 'analysisComplete'
        QtMocHelpers::SignalData<void(QVariantMap)>(19, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 7, 20 },
        }}),
        // Signal 'analysisFailed'
        QtMocHelpers::SignalData<void(const QString &)>(21, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 5 },
        }}),
        // Signal 'statusReceived'
        QtMocHelpers::SignalData<void(QVariantMap)>(22, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 7, 23 },
        }}),
        // Signal 'statsReceived'
        QtMocHelpers::SignalData<void(QVariantMap)>(24, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 7, 25 },
        }}),
        // Signal 'alertTriggered'
        QtMocHelpers::SignalData<void(QVariantMap)>(26, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 7, 27 },
        }}),
        // Slot 'connectToDaemon'
        QtMocHelpers::SlotData<void()>(28, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'disconnectFromDaemon'
        QtMocHelpers::SlotData<void()>(29, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'tryReconnect'
        QtMocHelpers::SlotData<void()>(30, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onDaemonStatusChanged'
        QtMocHelpers::SlotData<void(const QString &)>(31, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 32 },
        }}),
        // Slot 'onLogEntryReceived'
        QtMocHelpers::SlotData<void(QVariantMap)>(33, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 7, 8 },
        }}),
        // Slot 'onAlertTriggered'
        QtMocHelpers::SlotData<void(QVariantMap)>(34, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 7, 27 },
        }}),
        // Slot 'onPluginLoaded'
        QtMocHelpers::SlotData<void(const QString &)>(35, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 15 },
        }}),
        // Slot 'onPluginUnloaded'
        QtMocHelpers::SlotData<void(const QString &)>(36, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 15 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<DBusClient, qt_meta_tag_ZN10DBusClientE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject DBusClient::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10DBusClientE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10DBusClientE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN10DBusClientE_t>.metaTypes,
    nullptr
} };

void DBusClient::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<DBusClient *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->connected(); break;
        case 1: _t->disconnected(); break;
        case 2: _t->connectionFailed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->logEntryReceived((*reinterpret_cast<std::add_pointer_t<QVariantMap>>(_a[1]))); break;
        case 4: _t->logsQueried((*reinterpret_cast<std::add_pointer_t<QList<QVariantMap>>>(_a[1]))); break;
        case 5: _t->pluginsListed((*reinterpret_cast<std::add_pointer_t<QList<QVariantMap>>>(_a[1]))); break;
        case 6: _t->pluginToggled((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2]))); break;
        case 7: _t->pluginsReloaded((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 8: _t->analysisComplete((*reinterpret_cast<std::add_pointer_t<QVariantMap>>(_a[1]))); break;
        case 9: _t->analysisFailed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 10: _t->statusReceived((*reinterpret_cast<std::add_pointer_t<QVariantMap>>(_a[1]))); break;
        case 11: _t->statsReceived((*reinterpret_cast<std::add_pointer_t<QVariantMap>>(_a[1]))); break;
        case 12: _t->alertTriggered((*reinterpret_cast<std::add_pointer_t<QVariantMap>>(_a[1]))); break;
        case 13: _t->connectToDaemon(); break;
        case 14: _t->disconnectFromDaemon(); break;
        case 15: _t->tryReconnect(); break;
        case 16: _t->onDaemonStatusChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 17: _t->onLogEntryReceived((*reinterpret_cast<std::add_pointer_t<QVariantMap>>(_a[1]))); break;
        case 18: _t->onAlertTriggered((*reinterpret_cast<std::add_pointer_t<QVariantMap>>(_a[1]))); break;
        case 19: _t->onPluginLoaded((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 20: _t->onPluginUnloaded((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (DBusClient::*)()>(_a, &DBusClient::connected, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (DBusClient::*)()>(_a, &DBusClient::disconnected, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (DBusClient::*)(const QString & )>(_a, &DBusClient::connectionFailed, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (DBusClient::*)(QVariantMap )>(_a, &DBusClient::logEntryReceived, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (DBusClient::*)(QVector<QVariantMap> )>(_a, &DBusClient::logsQueried, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (DBusClient::*)(QVector<QVariantMap> )>(_a, &DBusClient::pluginsListed, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (DBusClient::*)(const QString & , bool )>(_a, &DBusClient::pluginToggled, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (DBusClient::*)(bool )>(_a, &DBusClient::pluginsReloaded, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (DBusClient::*)(QVariantMap )>(_a, &DBusClient::analysisComplete, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (DBusClient::*)(const QString & )>(_a, &DBusClient::analysisFailed, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (DBusClient::*)(QVariantMap )>(_a, &DBusClient::statusReceived, 10))
            return;
        if (QtMocHelpers::indexOfMethod<void (DBusClient::*)(QVariantMap )>(_a, &DBusClient::statsReceived, 11))
            return;
        if (QtMocHelpers::indexOfMethod<void (DBusClient::*)(QVariantMap )>(_a, &DBusClient::alertTriggered, 12))
            return;
    }
}

const QMetaObject *DBusClient::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *DBusClient::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10DBusClientE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int DBusClient::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 21)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 21;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 21)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 21;
    }
    return _id;
}

// SIGNAL 0
void DBusClient::connected()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void DBusClient::disconnected()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void DBusClient::connectionFailed(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void DBusClient::logEntryReceived(QVariantMap _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void DBusClient::logsQueried(QVector<QVariantMap> _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void DBusClient::pluginsListed(QVector<QVariantMap> _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void DBusClient::pluginToggled(const QString & _t1, bool _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1, _t2);
}

// SIGNAL 7
void DBusClient::pluginsReloaded(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1);
}

// SIGNAL 8
void DBusClient::analysisComplete(QVariantMap _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1);
}

// SIGNAL 9
void DBusClient::analysisFailed(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 9, nullptr, _t1);
}

// SIGNAL 10
void DBusClient::statusReceived(QVariantMap _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 10, nullptr, _t1);
}

// SIGNAL 11
void DBusClient::statsReceived(QVariantMap _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 11, nullptr, _t1);
}

// SIGNAL 12
void DBusClient::alertTriggered(QVariantMap _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 12, nullptr, _t1);
}
QT_WARNING_POP
