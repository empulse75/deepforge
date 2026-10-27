// Minimal structured logger. Kept project-owned so the simulation layer has a
// dependency-free logging seam and builds stay fast (see docs/decisions).
#pragma once

#include <format>
#include <string>
#include <string_view>

namespace df {

enum class LogLevel { Trace, Debug, Info, Warn, Error, Off };

/// The name every line format prints for a level; shared by the console
/// dispatch and the file sink so the two stop diverging (`[LEVEL]` vs `[N]`).
inline constexpr std::string_view level_name(LogLevel l) {
    switch (l) {
        case LogLevel::Trace:
            return "TRACE";
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warn:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
        case LogLevel::Off:
            return "OFF";
    }
    return "?";
}

/// Sink interface; the platform layer installs a file sink at startup.
/// Sinks receive category and message already escaped by dispatch.
class LogSink {
public:
    virtual ~LogSink() = default;

    virtual void write(LogLevel level, std::string_view category, std::string_view message) = 0;
};

namespace log_detail {
void dispatch(LogLevel level, std::string_view category, std::string message);
bool enabled(LogLevel level);
} // namespace log_detail

void set_log_level(LogLevel level);
LogLevel log_level();

/// Adds a sink. Sinks are owned for the lifetime of the process.
void add_log_sink(LogSink* sink);
void clear_log_sinks();

/// Enables an in-memory ring buffer of recent lines for the debug overlay.
const std::string* recent_log_lines(std::size_t& outCount);

template <typename... Args>
void log(LogLevel level, std::string_view category, std::format_string<Args...> fmt, Args&&... args) {
    if (!log_detail::enabled(level)) {
        return;
    }
    log_detail::dispatch(level, category, std::format(fmt, std::forward<Args>(args)...));
}

#define DF_LOG_TRACE(cat, ...) ::df::log(::df::LogLevel::Trace, cat, __VA_ARGS__)
#define DF_LOG_DEBUG(cat, ...) ::df::log(::df::LogLevel::Debug, cat, __VA_ARGS__)
#define DF_LOG_INFO(cat, ...) ::df::log(::df::LogLevel::Info, cat, __VA_ARGS__)
#define DF_LOG_WARN(cat, ...) ::df::log(::df::LogLevel::Warn, cat, __VA_ARGS__)
#define DF_LOG_ERROR(cat, ...) ::df::log(::df::LogLevel::Error, cat, __VA_ARGS__)

} // namespace df
