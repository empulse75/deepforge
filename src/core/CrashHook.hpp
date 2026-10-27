// Crash diagnostics (OPS-OBS-02): a named FATAL line for the fatal signals
// and for std::terminate, written while the process can still write at all.
// Install before anything loads content so the line lands in the log.
#pragma once

#include <filesystem>

namespace df {

/// Installs handlers for SIGSEGV, SIGABRT and std::terminate. Each handler
/// appends its FATAL line to the given log file and to stderr using only
/// async-signal-safe calls (open/write/close, signal/raise — the Windows CRT
/// makes no such promise, but it has no POSIX signal semantics either), then
/// re-raises with the default disposition so exit statuses and core dumps
/// are untouched: SIGABRT still dies 134, SIGSEGV still 139. Install once,
/// before threads that may crash exist.
void install_crash_hook(const std::filesystem::path& logFile);

} // namespace df
