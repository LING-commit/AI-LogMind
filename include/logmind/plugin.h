#pragma once
#include <logmind/log_entry.h>
#include <string>
#include <vector>

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

    const std::string& name()        const { static std::string s; return s = manifest().name; }
    const std::string& version()     const { static std::string s; return s = manifest().version; }
    const std::string& description() const { static std::string s; return s = manifest().description; }
    PluginType          type()       const { return manifest().type; }
};

} // namespace logmind

extern "C" {
    logmind::IPlugin* create_plugin(logmind::CreateContext ctx);
    void destroy_plugin(logmind::IPlugin* plugin);
    int  plugin_abi_version();
}
