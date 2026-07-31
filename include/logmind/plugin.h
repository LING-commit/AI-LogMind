#pragma once
#include <logmind/log_entry.h>
#include <mutex>
#include <string>
#include <vector>

// 插件的三个入口必须对 dlsym 可见。插件通常用 -fvisibility=hidden 编译
// （避免多个 .so 之间符号互相污染），所以入口点要显式标成 default。
#if defined(_WIN32)
#  define LOGMIND_PLUGIN_EXPORT __declspec(dllexport)
#else
#  define LOGMIND_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

namespace logmind {

class IPluginLifecycle {
public:
    virtual ~IPluginLifecycle() = default;

    virtual bool on_load(const nlohmann::json& config) { (void)config; return true; }
    virtual void on_unload() {}
    virtual bool on_reload_config(const nlohmann::json& new_config) { (void)new_config; return true; }
};

class IPlugin : public IPluginLifecycle {
public:
    ~IPlugin() override = default;

    virtual PluginManifest manifest() const = 0;

    // 每实例缓存一次 manifest，之后返回稳定引用。
    //
    // 不要退回到函数内 static：那样所有插件实例会共享同一个 std::string，
    // 既是数据竞争（采集线程每行都会调 name()，D-Bus 线程同时在 list_plugins()
    // 里调），又会让 a->name() == b->name() 这类比较恒真——unload_all() 曾因此
    // 永远只卸载第一个插件。
    const std::string& name()        const { return cached_manifest().name; }
    const std::string& version()     const { return cached_manifest().version; }
    const std::string& description() const { return cached_manifest().description; }
    PluginType         type()        const { return cached_manifest().type; }

private:
    const PluginManifest& cached_manifest() const {
        // manifest() 是纯虚函数，只能在构造完成后调用，所以走惰性初始化。
        std::call_once(m_manifest_once, [this] { m_manifest = manifest(); });
        return m_manifest;
    }

    mutable std::once_flag  m_manifest_once;
    mutable PluginManifest  m_manifest;
};

} // namespace logmind

extern "C" {
    LOGMIND_PLUGIN_EXPORT logmind::IPlugin* create_plugin(logmind::CreateContext ctx);
    LOGMIND_PLUGIN_EXPORT void destroy_plugin(logmind::IPlugin* plugin);
    LOGMIND_PLUGIN_EXPORT int  plugin_abi_version();
}
