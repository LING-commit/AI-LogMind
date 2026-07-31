#pragma once
#include <logmind/plugin.h>
#include <istream>
#include <string>
#include <vector>

namespace logmind {

class ICollector : public IPlugin {
public:
    ~ICollector() override;
    PluginManifest manifest() const override = 0;

    struct ParseResult {
        std::vector<LogEntry> entries;
        size_t lines_read     = 0;
        size_t lines_parsed   = 0;
        size_t lines_skipped  = 0;
        std::string error;
    };

    virtual int match(const std::string& filename) const = 0;
    virtual ParseResult parse(std::istream& stream) = 0;
    virtual ParseResult parse_file(const std::string& path);

    // 逐行处理路径的入口。默认实现把单行包进 istringstream 转给 parse()，
    // 高吞吐的采集器应当覆写它，省掉每行一次流对象构造。
    virtual ParseResult parse_line(const std::string& line);

    virtual std::vector<std::string> watch_patterns() const { return {}; }
    virtual bool supports_incremental() const { return false; }
};

} // namespace logmind
