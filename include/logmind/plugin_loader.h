#pragma once
#include <logmind/plugin.h>
#include <logmind/collector.h>
#include <logmind/analyzer.h>
#include <logmind/alert.h>
#include <string>
#include <vector>
#include <memory>
#include <dlfcn.h>

namespace logmind {

class PluginLoader {
public:
    PluginLoader();
    ~PluginLoader();

    PluginLoader(const PluginLoader&) = delete;
    PluginLoader& operator=(const PluginLoader&) = delete;

    bool load_directory(const std::string& path);
    bool load_plugin(const std::string& so_path);
    void unload_plugin(const std::string& name);
    void unload_all();

    std::vector<ICollector*> collectors() const;
    std::vector<IAnalyzer*>  analyzers() const;
    std::vector<IAlert*>     alerts() const;

    IPlugin* find(const std::string& name) const;
    std::vector<PluginInfo> list_plugins() const;
    bool set_enabled(const std::string& name, bool enabled);

    int  loaded_count()  const { return static_cast<int>(m_plugins.size()); }
    bool is_loaded()     const { return !m_plugins.empty(); }

private:
    struct PluginHandle {
        void*       handle    = nullptr;
        IPlugin*    instance  = nullptr;
        std::string so_path;
        bool        enabled   = true;
        std::string error;
    };

    std::vector<PluginHandle> m_plugins;

    IPlugin* instantiate_plugin(void* handle);
    PluginHandle* find_handle(const std::string& name);
};

} // namespace logmind
