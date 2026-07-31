#include <logmind/plugin_loader.h>
#include <logmind/version.h>
#include <algorithm>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace logmind {

namespace {

// 插件对象是在插件自己的编译单元里 new 出来的，必须交回它自己的
// destroy_plugin 释放；只有在符号缺失时才退化成 delete。
void destroy_instance(void* handle, IPlugin* instance) {
    if (!instance) return;
    auto destroy = reinterpret_cast<void (*)(IPlugin*)>(dlsym(handle, "destroy_plugin"));
    if (destroy) destroy(instance);
    else         delete instance;
}

} // namespace

// ── LoadedPlugin ──
LoadedPlugin::LoadedPlugin(void* handle, IPlugin* instance, std::string so_path)
    : m_handle(handle)
    , m_instance(instance)
    , m_so_path(std::move(so_path))
{
}

LoadedPlugin::~LoadedPlugin() {
    if (m_instance) {
        m_instance->on_unload();
        destroy_instance(m_handle, m_instance);
        m_instance = nullptr;
    }
    if (m_handle) {
        dlclose(m_handle);
        m_handle = nullptr;
    }
}

// ── PluginLoader ──
PluginLoader::PluginLoader()
    : m_snapshot(std::make_shared<const PluginSet>())
{
}

PluginLoader::~PluginLoader() {
    unload_all();
}

void PluginLoader::set_config_provider(ConfigProvider provider) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_config_provider = std::move(provider);
}

void PluginLoader::record_failure(const std::string& so_path, std::string error) {
    Record rec;
    rec.so_path = so_path;
    rec.error   = std::move(error);
    rec.enabled = false;

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_records.push_back(std::move(rec));
}

bool PluginLoader::load_directory(const std::string& path) {
    std::error_code ec;
    if (!fs::is_directory(path, ec)) {
        std::cerr << "Plugin directory not found: " << path << std::endl;
        return false;
    }

    bool any_loaded = false;
    for (const auto& entry : fs::recursive_directory_iterator(path, ec)) {
        if (entry.path().extension() == ".so" && load_plugin(entry.path().string())) {
            any_loaded = true;
        }
    }
    return any_loaded;
}

bool PluginLoader::load_plugin(const std::string& so_path) {
    // 同一个 .so 不重复加载
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        for (const auto& r : m_records) {
            if (r.plugin && r.so_path == so_path) return true;
        }
    }

    dlerror();  // 清掉可能残留的旧错误
    void* handle = dlopen(so_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        // dlerror() 是一次性的：取出来必须先存住。原先这里连调两次，
        // 第二次返回 nullptr，拿去构造 std::string / 送进 ostream 都是 UB。
        const char* raw = dlerror();
        std::string message = raw ? raw : "unknown dlopen error";
        std::cerr << "Failed to load plugin " << so_path << ": " << message << std::endl;
        record_failure(so_path, std::move(message));
        return false;
    }

    auto reject = [&](std::string why) {
        std::cerr << "Plugin rejected (" << why << "): " << so_path << std::endl;
        dlclose(handle);
        record_failure(so_path, std::move(why));
        return false;
    };

    // ── ABI 版本检查 ──
    // 符号缺失同样要拒绝。原先写的是 `if (abi_fn && abi_fn() != ...)`，
    // 于是任何没导出该符号的 .so 都被无条件放行，校验形同虚设。
    auto abi_fn = reinterpret_cast<int (*)()>(dlsym(handle, "plugin_abi_version"));
    if (!abi_fn) {
        return reject("missing plugin_abi_version symbol");
    }
    const int abi = abi_fn();
    if (abi != LOGMIND_ABI_VERSION) {
        return reject("ABI mismatch: plugin=" + std::to_string(abi) +
                      ", daemon=" + std::to_string(LOGMIND_ABI_VERSION));
    }

    IPlugin* instance = instantiate_plugin(handle, nlohmann::json::object());
    if (!instance) {
        return reject("create_plugin missing or returned null");
    }

    // 插件名要等实例化之后才知道，所以配置是通过 on_load 下发而不是 CreateContext。
    ConfigProvider provider;
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        provider = m_config_provider;
    }
    nlohmann::json plugin_cfg = nlohmann::json::object();
    if (provider) {
        plugin_cfg = provider(instance->name());
    }

    if (!instance->on_load(plugin_cfg)) {
        std::string why = "on_load returned false";
        std::cerr << "Plugin " << instance->name() << " rejected: " << why << std::endl;
        // on_load 失败时不调 on_unload，直接销毁
        destroy_instance(handle, instance);
        dlclose(handle);
        record_failure(so_path, std::move(why));
        return false;
    }

    Record rec;
    rec.plugin  = std::make_shared<LoadedPlugin>(handle, instance, so_path);
    rec.so_path = so_path;
    rec.enabled = true;

    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_records.push_back(std::move(rec));
        rebuild_snapshot_locked();
    }

    std::cout << "Loaded plugin: " << instance->name()
              << " v" << instance->version()
              << " (" << to_string(instance->type()) << ")"
              << std::endl;
    return true;
}

void PluginLoader::unload_plugin(const std::string& name) {
    // 在锁外让 shared_ptr 归零：on_unload / dlclose 可能耗时甚至回调回来。
    PluginPtr dropped;
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        auto it = std::find_if(m_records.begin(), m_records.end(),
            [&](const Record& r) {
                return r.plugin && r.plugin->instance()->name() == name;
            });
        if (it == m_records.end()) return;

        dropped = std::move(it->plugin);
        m_records.erase(it);
        rebuild_snapshot_locked();
    }
    // dropped 在此归零。若流水线仍持有旧快照，真正的 dlclose 会推迟到它释放之后。
}

void PluginLoader::unload_all() {
    std::vector<PluginPtr> dropped;
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        dropped.reserve(m_records.size());
        for (auto& r : m_records) {
            if (r.plugin) dropped.push_back(std::move(r.plugin));
        }
        m_records.clear();
        rebuild_snapshot_locked();
    }
}

void PluginLoader::rebuild_snapshot_locked() {
    auto set = std::make_shared<PluginSet>();
    set->all.reserve(m_records.size());

    for (const auto& r : m_records) {
        if (!r.plugin) continue;
        set->all.push_back(r.plugin);       // keep-alive，禁用的插件也要留住
        if (!r.enabled) continue;

        IPlugin* p = r.plugin->instance();
        // 一个插件同时实现多个接口是允许的，所以这里是并列的 if 而非 else-if
        if (auto* c  = dynamic_cast<ICollector*>(p)) set->collectors.push_back(c);
        if (auto* an = dynamic_cast<IAnalyzer*>(p))  set->analyzers.push_back(an);
        if (auto* al = dynamic_cast<IAlert*>(p))     set->alerts.push_back(al);
    }
    m_snapshot = std::move(set);
}

PluginSetPtr PluginLoader::snapshot() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_snapshot;
}

std::vector<ICollector*> PluginLoader::collectors() const { return snapshot()->collectors; }
std::vector<IAnalyzer*>  PluginLoader::analyzers()  const { return snapshot()->analyzers;  }
std::vector<IAlert*>     PluginLoader::alerts()     const { return snapshot()->alerts;     }

IPlugin* PluginLoader::find(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    for (const auto& r : m_records) {
        if (r.plugin && r.plugin->instance()->name() == name) {
            return r.plugin->instance();
        }
    }
    return nullptr;
}

std::vector<PluginInfo> PluginLoader::list_plugins() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    std::vector<PluginInfo> result;
    result.reserve(m_records.size());
    for (const auto& r : m_records) {
        PluginInfo info;
        if (r.plugin) {
            const PluginManifest m = r.plugin->instance()->manifest();
            info.name        = m.name;
            info.version     = m.version;
            info.description = m.description;
            info.author      = m.author;
            info.type        = m.type;
        }
        info.so_path = r.so_path;
        info.loaded  = (r.plugin != nullptr);
        info.enabled = r.enabled;
        info.error   = r.error;
        result.push_back(std::move(info));
    }
    return result;
}

bool PluginLoader::set_enabled(const std::string& name, bool enabled) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    for (auto& r : m_records) {
        if (r.plugin && r.plugin->instance()->name() == name) {
            r.enabled = enabled;
            rebuild_snapshot_locked();
            return true;
        }
    }
    return false;
}

int PluginLoader::loaded_count() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return static_cast<int>(std::count_if(m_records.begin(), m_records.end(),
        [](const Record& r) { return r.plugin != nullptr; }));
}

bool PluginLoader::is_loaded() const {
    return loaded_count() > 0;
}

IPlugin* PluginLoader::instantiate_plugin(void* handle, const nlohmann::json& config) {
    auto create_fn = reinterpret_cast<IPlugin* (*)(CreateContext)>(
        dlsym(handle, "create_plugin"));
    if (!create_fn) return nullptr;

    CreateContext ctx;
    ctx.config = config;
    return create_fn(ctx);
}

} // namespace logmind
