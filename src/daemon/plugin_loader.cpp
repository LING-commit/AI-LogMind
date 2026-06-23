#include <logmind/plugin_loader.h>
#include <logmind/version.h>
#include <filesystem>
#include <iostream>
#include <cstring>

namespace fs = std::filesystem;

namespace logmind {

PluginLoader::PluginLoader() = default;

PluginLoader::~PluginLoader() {
    unload_all();
}

bool PluginLoader::load_directory(const std::string& path) {
    if (!fs::exists(path) || !fs::is_directory(path)) {
        std::cerr << "Plugin directory not found: " << path << std::endl;
        return false;
    }

    bool any_loaded = false;

    for (const auto& entry : fs::recursive_directory_iterator(path)) {
        if (entry.path().extension() == ".so") {
            if (load_plugin(entry.path().string())) {
                any_loaded = true;
            }
        }
    }

    return any_loaded;
}

bool PluginLoader::load_plugin(const std::string& so_path) {
    void* handle = dlopen(so_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        PluginHandle ph;
        ph.handle   = nullptr;
        ph.instance = nullptr;
        ph.so_path  = so_path;
        ph.enabled  = false;
        ph.error    = dlerror();
        m_plugins.push_back(std::move(ph));

        std::cerr << "Failed to load plugin " << so_path
                  << ": " << dlerror() << std::endl;
        return false;
    }

    // ── ABI 版本检查 ──
    auto abi_fn = reinterpret_cast<int(*)()>(dlsym(handle, "plugin_abi_version"));
    if (abi_fn && abi_fn() != LOGMIND_ABI_VERSION) {
        std::cerr << "Plugin ABI version mismatch: " << so_path << std::endl;
        dlclose(handle);
        return false;
    }

    // ── 实例化插件 ──
    IPlugin* instance = instantiate_plugin(handle);
    if (!instance) {
        std::cerr << "Failed to instantiate plugin: " << so_path << std::endl;
        dlclose(handle);
        return false;
    }

    PluginHandle ph;
    ph.handle   = handle;
    ph.instance = instance;
    ph.so_path  = so_path;
    ph.enabled  = true;

    // ── 调用 on_load ──
    if (!instance->on_load(nlohmann::json::object())) {
        std::cerr << "Plugin on_load failed: " << instance->name() << std::endl;
        delete instance;
        dlclose(handle);
        return false;
    }

    m_plugins.push_back(std::move(ph));

    std::cout << "Loaded plugin: " << instance->name()
              << " v" << instance->version()
              << " (" << to_string(instance->type()) << ")"
              << std::endl;

    return true;
}

void PluginLoader::unload_plugin(const std::string& name) {
    auto* ph = find_handle(name);
    if (!ph) return;

    if (ph->instance) {
        ph->instance->on_unload();
        auto destroy = reinterpret_cast<void(*)(IPlugin*)>(
            dlsym(ph->handle, "destroy_plugin"));
        if (destroy) {
            destroy(ph->instance);
        } else {
            delete ph->instance;
        }
    }

    if (ph->handle) {
        dlclose(ph->handle);
    }

    m_plugins.erase(
        std::remove_if(m_plugins.begin(), m_plugins.end(),
            [&](const PluginHandle& h) { return h.instance == ph->instance; }),
        m_plugins.end());
}

void PluginLoader::unload_all() {
    while (!m_plugins.empty()) {
        auto& back = m_plugins.back();
        if (!back.instance) {
            // Failed-to-load plugin entry; just remove it
            if (back.handle) dlclose(back.handle);
            m_plugins.pop_back();
            continue;
        }
        unload_plugin(back.instance->name());
    }
}

std::vector<ICollector*> PluginLoader::collectors() const {
    std::vector<ICollector*> result;
    for (const auto& ph : m_plugins) {
        if (ph.instance && ph.enabled) {
            auto* c = dynamic_cast<ICollector*>(ph.instance);
            if (c) result.push_back(c);
        }
    }
    return result;
}

std::vector<IAnalyzer*> PluginLoader::analyzers() const {
    std::vector<IAnalyzer*> result;
    for (const auto& ph : m_plugins) {
        if (ph.instance && ph.enabled) {
            auto* a = dynamic_cast<IAnalyzer*>(ph.instance);
            if (a) result.push_back(a);
        }
    }
    return result;
}

std::vector<IAlert*> PluginLoader::alerts() const {
    std::vector<IAlert*> result;
    for (const auto& ph : m_plugins) {
        if (ph.instance && ph.enabled) {
            auto* a = dynamic_cast<IAlert*>(ph.instance);
            if (a) result.push_back(a);
        }
    }
    return result;
}

IPlugin* PluginLoader::find(const std::string& name) const {
    auto* ph = const_cast<PluginLoader*>(this)->find_handle(name);
    return ph ? ph->instance : nullptr;
}

std::vector<PluginInfo> PluginLoader::list_plugins() const {
    std::vector<PluginInfo> result;
    for (const auto& ph : m_plugins) {
        PluginInfo info;
        if (ph.instance) {
            info.name        = ph.instance->name();
            info.version     = ph.instance->version();
            info.description = ph.instance->description();
            info.author      = ph.instance->manifest().author;
            info.type        = ph.instance->type();
        }
        info.so_path = ph.so_path;
        info.loaded  = ph.instance != nullptr;
        info.enabled = ph.enabled;
        info.error   = ph.error;
        result.push_back(info);
    }
    return result;
}

bool PluginLoader::set_enabled(const std::string& name, bool enabled) {
    auto* ph = find_handle(name);
    if (!ph) return false;
    ph->enabled = enabled;
    return true;
}

IPlugin* PluginLoader::instantiate_plugin(void* handle) {
    auto create_fn = reinterpret_cast<IPlugin*(*)(CreateContext)>(
        dlsym(handle, "create_plugin"));
    if (!create_fn) {
        return nullptr;
    }

    CreateContext ctx;
    return create_fn(ctx);
}

PluginLoader::PluginHandle* PluginLoader::find_handle(const std::string& name) {
    for (auto& ph : m_plugins) {
        if (ph.instance && ph.instance->name() == name) {
            return &ph;
        }
    }
    return nullptr;
}

} // namespace logmind
