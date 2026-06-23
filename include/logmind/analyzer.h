#pragma once
#include <logmind/plugin.h>
#include <functional>

namespace logmind {

class IAnalyzer : public IPlugin {
public:
    ~IAnalyzer() override;
    PluginManifest manifest() const override = 0;

    virtual std::vector<LogEntry> analyze(std::vector<LogEntry> entries) = 0;

    struct AIAnalysis {
        std::string log_id;
        std::string summary;
        std::vector<std::string> root_causes;
        std::vector<std::string> suggestions;
        double confidence = 0.0;
        std::string raw_response;
    };

    virtual bool supports_ai_analysis() const { return false; }

    virtual AIAnalysis ai_analyze(const LogEntry& entry) {
        (void)entry;
        return {};
    }

    using AnalyzeCallback = std::function<void(std::vector<LogEntry>)>;

    virtual void analyze_stream(
        std::vector<LogEntry> entries,
        AnalyzeCallback callback)
    {
        auto result = analyze(std::move(entries));
        if (!result.empty()) {
            callback(std::move(result));
        }
    }
};

} // namespace logmind
