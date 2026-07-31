#pragma once
#include <logmind/plugin.h>
#include <logmind/collector.h>
#include <logmind/analyzer.h>
#include <logmind/alert.h>
#include <dlfcn.h>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <vector>

namespace logmind {

// dlopen 出来的一个插件，析构时按 on_unload → destroy_plugin → dlclose 收尾。
//
// 之所以用 shared_ptr 管理：只要还有人（比如正在跑的流水线）持有引用就不会
// dlclose，从而杜绝「D-Bus 线程重载插件时把采集线程正在执行的代码段抽走」
// 这类 use-after-free。
class LoadedPlugin {
public:
    LoadedPlugin(void* handle, IPlugin* instance, std::string so_path);
    ~LoadedPlugin();

    LoadedPlugin(const LoadedPlugin&)            = delete;
    LoadedPlugin& operator=(const LoadedPlugin&) = delete;

    IPlugin*           instance() const { return m_instance; }
    const std::string& so_path()  const { return m_so_path; }

private:
    void*       m_handle   = nullptr;
    IPlugin*    m_instance = nullptr;
    std::string m_so_path;
};

using PluginPtr = std::shared_ptr<LoadedPlugin>;

// 插件集合的不可变快照。`all` 持有 keep-alive 引用，所以只要快照还活着，
// 后面三个裸指针数组就是安全的。
struct PluginSet {
    std::vector<PluginPtr>   all;
    std::vector<ICollector*> collectors;
    std::vector<IAnalyzer*>  analyzers;
    std::vector<IAlert*>     alerts;
};
using PluginSetPtr = std::shared_ptr<const PluginSet>;

class PluginLoader {
public:
    PluginLoader();
    ~PluginLoader();

    PluginLoader(const PluginLoader&) = delete;
    PluginLoader& operator=(const PluginLoader&) = delete;

    // 插件加载时用它拿到自己那段配置，传给 on_load()。
    using ConfigProvider = std::function<nlohmann::json(const std::string& plugin_name)>;
    void set_config_provider(ConfigProvider provider);

    bool load_directory(const std::string& path);
    bool load_plugin(const std::string& so_path);
    void unload_plugin(const std::string& name);
    void unload_all();

    // 热路径用这个：一次加锁拿到全部三类插件，遍历期间插件不会被卸载。
    PluginSetPtr snapshot() const;

    // 便捷封装，每次都会拷贝一份 vector——不要在逐行处理的路径上调用。
    std::vector<ICollector*> collectors() const;
    std::vector<IAnalyzer*>  analyzers() const;
    std::vector<IAlert*>     alerts() const;

    IPlugin* find(const std::string& name) const;
    std::vector<PluginInfo> list_plugins() const;
    bool set_enabled(const std::string& name, bool enabled);

    int  loaded_count() const;
    bool is_loaded()    const;

private:
    struct Record {
        PluginPtr   plugin;          // 为空表示这个 .so 加载失败，仅用于上报错误
        std::string so_path;
        std::string error;
        bool        enabled = true;
    };

    static IPlugin* instantiate_plugin(void* handle, const nlohmann::json& config);
    void rebuild_snapshot_locked();
    void record_failure(const std::string& so_path, std::string error);

    mutable std::shared_mutex m_mutex;
    std::vector<Record>       m_records;
    PluginSetPtr              m_snapshot;
    ConfigProvider            m_config_provider;
};

} // namespace logmind
