/****************************************************************************
** Meta object code from reading C++ file 'dbus_service.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/daemon/dbus_service.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'dbus_service.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN13DaemonAdaptorE_t {};
} // unnamed namespace

template <> constexpr inline auto DaemonAdaptor::qt_create_metaobjectdata<qt_meta_tag_ZN13DaemonAdaptorE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "DaemonAdaptor",
        "D-Bus Interface",
        "com.logmind.Daemon1",
        "LogEntryReceived",
        "",
        "QVariantMap",
        "entry",
        "AlertTriggered",
        "alert",
        "PluginLoaded",
        "name",
        "PluginUnloaded",
        "DaemonStatusChanged",
        "state",
        "QueryLogs",
        "QList<QVariantMap>",
        "query",
        "limit",
        "QueryLogsByTime",
        "start_ns",
        "end_ns",
        "InjectLog",
        "source",
        "raw_line",
        "AnalyzeLogs",
        "log_ids",
        "GetAnalysisReport",
        "log_id",
        "ListPlugins",
        "GetPluginInfo",
        "EnablePlugin",
        "DisablePlugin",
        "ReloadPlugins",
        "GetStats",
        "GetStatus"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'LogEntryReceived'
        QtMocHelpers::SignalData<void(QVariantMap)>(3, 4, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 5, 6 },
        }}),
        // Signal 'AlertTriggered'
        QtMocHelpers::SignalData<void(QVariantMap)>(7, 4, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 5, 8 },
        }}),
        // Signal 'PluginLoaded'
        QtMocHelpers::SignalData<void(const QString &)>(9, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 10 },
        }}),
        // Signal 'PluginUnloaded'
        QtMocHelpers::SignalData<void(const QString &)>(11, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 10 },
        }}),
        // Signal 'DaemonStatusChanged'
        QtMocHelpers::SignalData<void(const QString &)>(12, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 13 },
        }}),
        // Slot 'QueryLogs'
        QtMocHelpers::SlotData<QList<QVariantMap>(const QString &, quint32)>(14, 4, QMC::AccessPublic, 0x80000000 | 15, {{
            { QMetaType::QString, 16 }, { QMetaType::UInt, 17 },
        }}),
        // Slot 'QueryLogsByTime'
        QtMocHelpers::SlotData<QList<QVariantMap>(quint64, quint64, quint32)>(18, 4, QMC::AccessPublic, 0x80000000 | 15, {{
            { QMetaType::ULongLong, 19 }, { QMetaType::ULongLong, 20 }, { QMetaType::UInt, 17 },
        }}),
        // Slot 'InjectLog'
        QtMocHelpers::SlotData<void(const QString &, const QString &)>(21, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 22 }, { QMetaType::QString, 23 },
        }}),
        // Slot 'AnalyzeLogs'
        QtMocHelpers::SlotData<QList<QVariantMap>(const QStringList &)>(24, 4, QMC::AccessPublic, 0x80000000 | 15, {{
            { QMetaType::QStringList, 25 },
        }}),
        // Slot 'GetAnalysisReport'
        QtMocHelpers::SlotData<QString(const QString &)>(26, 4, QMC::AccessPublic, QMetaType::QString, {{
            { QMetaType::QString, 27 },
        }}),
        // Slot 'ListPlugins'
        QtMocHelpers::SlotData<QString()>(28, 4, QMC::AccessPublic, QMetaType::QString),
        // Slot 'GetPluginInfo'
        QtMocHelpers::SlotData<QVariantMap(const QString &)>(29, 4, QMC::AccessPublic, 0x80000000 | 5, {{
            { QMetaType::QString, 10 },
        }}),
        // Slot 'EnablePlugin'
        QtMocHelpers::SlotData<bool(const QString &)>(30, 4, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 10 },
        }}),
        // Slot 'DisablePlugin'
        QtMocHelpers::SlotData<bool(const QString &)>(31, 4, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 10 },
        }}),
        // Slot 'ReloadPlugins'
        QtMocHelpers::SlotData<bool()>(32, 4, QMC::AccessPublic, QMetaType::Bool),
        // Slot 'GetStats'
        QtMocHelpers::SlotData<QVariantMap()>(33, 4, QMC::AccessPublic, 0x80000000 | 5),
        // Slot 'GetStatus'
        QtMocHelpers::SlotData<QVariantMap()>(34, 4, QMC::AccessPublic, 0x80000000 | 5),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    QtMocHelpers::UintData qt_constructors {};
    QtMocHelpers::ClassInfos qt_classinfo({
            {    1,    2 },
    });
    return QtMocHelpers::metaObjectData<DaemonAdaptor, qt_meta_tag_ZN13DaemonAdaptorE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums, qt_constructors, qt_classinfo);
}
Q_CONSTINIT const QMetaObject DaemonAdaptor::staticMetaObject = { {
    QMetaObject::SuperData::link<QDBusAbstractAdaptor::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13DaemonAdaptorE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13DaemonAdaptorE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13DaemonAdaptorE_t>.metaTypes,
    nullptr
} };

void DaemonAdaptor::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<DaemonAdaptor *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->LogEntryReceived((*reinterpret_cast<std::add_pointer_t<QVariantMap>>(_a[1]))); break;
        case 1: _t->AlertTriggered((*reinterpret_cast<std::add_pointer_t<QVariantMap>>(_a[1]))); break;
        case 2: _t->PluginLoaded((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->PluginUnloaded((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->DaemonStatusChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: { QList<QVariantMap> _r = _t->QueryLogs((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<quint32>>(_a[2])));
            if (_a[0]) *reinterpret_cast<QList<QVariantMap>*>(_a[0]) = std::move(_r); }  break;
        case 6: { QList<QVariantMap> _r = _t->QueryLogsByTime((*reinterpret_cast<std::add_pointer_t<quint64>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<quint64>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<quint32>>(_a[3])));
            if (_a[0]) *reinterpret_cast<QList<QVariantMap>*>(_a[0]) = std::move(_r); }  break;
        case 7: _t->InjectLog((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 8: { QList<QVariantMap> _r = _t->AnalyzeLogs((*reinterpret_cast<std::add_pointer_t<QStringList>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QList<QVariantMap>*>(_a[0]) = std::move(_r); }  break;
        case 9: { QString _r = _t->GetAnalysisReport((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 10: { QString _r = _t->ListPlugins();
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 11: { QVariantMap _r = _t->GetPluginInfo((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 12: { bool _r = _t->EnablePlugin((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 13: { bool _r = _t->DisablePlugin((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 14: { bool _r = _t->ReloadPlugins();
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 15: { QVariantMap _r = _t->GetStats();
            if (_a[0]) *reinterpret_cast<QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 16: { QVariantMap _r = _t->GetStatus();
            if (_a[0]) *reinterpret_cast<QVariantMap*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (DaemonAdaptor::*)(QVariantMap )>(_a, &DaemonAdaptor::LogEntryReceived, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (DaemonAdaptor::*)(QVariantMap )>(_a, &DaemonAdaptor::AlertTriggered, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (DaemonAdaptor::*)(const QString & )>(_a, &DaemonAdaptor::PluginLoaded, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (DaemonAdaptor::*)(const QString & )>(_a, &DaemonAdaptor::PluginUnloaded, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (DaemonAdaptor::*)(const QString & )>(_a, &DaemonAdaptor::DaemonStatusChanged, 4))
            return;
    }
}

const QMetaObject *DaemonAdaptor::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *DaemonAdaptor::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13DaemonAdaptorE_t>.strings))
        return static_cast<void*>(this);
    return QDBusAbstractAdaptor::qt_metacast(_clname);
}

int DaemonAdaptor::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDBusAbstractAdaptor::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 17)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 17;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 17)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 17;
    }
    return _id;
}

// SIGNAL 0
void DaemonAdaptor::LogEntryReceived(QVariantMap _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void DaemonAdaptor::AlertTriggered(QVariantMap _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void DaemonAdaptor::PluginLoaded(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void DaemonAdaptor::PluginUnloaded(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void DaemonAdaptor::DaemonStatusChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}
QT_WARNING_POP
