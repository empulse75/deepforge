#include "core/Log.hpp"

#include <array>
#include <cstdio>
#include <mutex>
#include <vector>

namespace df {
namespace {

constexpr std::size_t kRingCapacity = 256;

struct LogState {
    std::mutex mutex;
    LogLevel level = LogLevel::Info;
    std::vector<LogSink*> sinks;
    std::array<std::string, kRingCapacity> ring;
    std::size_t ringCount = 0;
    std::size_t ringNext = 0;
};

LogState& state() {
    static LogState s;
    return s;
}

constexpr std::string_view level_name(LogLevel l) {
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

} // namespace

void set_log_level(LogLevel level) {
    auto& s = state();
    std::lock_guard lock(s.mutex);
    s.level = level;
}

LogLevel log_level() {
    auto& s = state();
    std::lock_guard lock(s.mutex);
    return s.level;
}

void add_log_sink(LogSink* sink) {
    auto& s = state();
    std::lock_guard lock(s.mutex);
    s.sinks.push_back(sink);
}

void clear_log_sinks() {
    auto& s = state();
    std::lock_guard lock(s.mutex);
    s.sinks.clear();
}

const std::string* recent_log_lines(std::size_t& outCount) {
    auto& s = state();
    outCount = s.ringCount;
    return s.ring.data();
}

namespace log_detail {

bool enabled(LogLevel level) {
    auto& s = state();
    std::lock_guard lock(s.mutex);
    return level >= s.level && s.level != LogLevel::Off;
}

void dispatch(LogLevel level, std::string_view category, std::string message) {
    auto& s = state();
    std::lock_guard lock(s.mutex);
    const std::string line = std::format("[{}] [{}] {}", level_name(level), category, message);
    std::fputs(line.c_str(), level >= LogLevel::Warn ? stderr : stdout);
    std::fputc('\n', level >= LogLevel::Warn ? stderr : stdout);
    s.ring[s.ringNext] = line;
    s.ringNext = (s.ringNext + 1) % kRingCapacity;
    if (s.ringCount < kRingCapacity) {
        ++s.ringCount;
    }
    for (auto* sink : s.sinks) {
        sink->write(level, category, message);
    }
}

} // namespace log_detail
} // namespace df
