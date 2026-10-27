#include "core/Log.hpp"

#include "core/Sanitize.hpp"

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
    // SEC-07: the single choke point. Every line that leaves the process —
    // console, ring buffer, sinks — carries escaped text, so a control byte
    // from a data file, a save, argv or env can no longer restyle the output
    // or forge a record boundary. \n stays reserved as the terminator.
    const std::string safeCategory = escape_control(category);
    const std::string safeMessage = escape_control(message);
    const std::string line = std::format("[{}] [{}] {}", level_name(level), safeCategory, safeMessage);
    std::fputs(line.c_str(), level >= LogLevel::Warn ? stderr : stdout);
    std::fputc('\n', level >= LogLevel::Warn ? stderr : stdout);
    s.ring[s.ringNext] = line;
    s.ringNext = (s.ringNext + 1) % kRingCapacity;
    if (s.ringCount < kRingCapacity) {
        ++s.ringCount;
    }
    for (auto* sink : s.sinks) {
        sink->write(level, safeCategory, safeMessage);
    }
}

} // namespace log_detail
} // namespace df
