// OPS-OBS-02 pinned on the mechanism itself: the hook writes its named FATAL
// line using only async-signal-safe work, and the process still dies exactly
// as it did before — death by the original signal, so exit statuses and core
// dumps are unchanged (SIGABRT still measured 134 end to end on the built
// binary). POSIX-only because the proof is fork/waitpid.
#ifndef _WIN32

#include "TestSupport.hpp"

#include "core/CrashHook.hpp"

#include <catch2/catch_test_macros.hpp>

#include <csignal>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

using namespace df;

namespace {

std::string slurp(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream os;
    os << in.rdbuf();
    return os.str();
}

/// Installs the hook in a forked child, runs one of the death paths there,
/// and returns what the child's own log file holds once it is dead. The
/// child must die by signal — the handler re-raises the default disposition.
std::string run_crashed_child(const std::filesystem::path& dir, const std::string& how, int expectSignal) {
    const auto log = dir / (how + ".log");
    const pid_t pid = fork();
    REQUIRE(pid >= 0);
    if (pid == 0) {
        df::install_crash_hook(log);
        if (how == "abort") {
            raise(SIGABRT);
        } else if (how == "segv") {
            raise(SIGSEGV);
        } else {
            std::terminate();
        }
        _exit(0); // the hook never returns
    }
    int status = 0;
    REQUIRE(waitpid(pid, &status, 0) == pid);
    REQUIRE(WIFSIGNALED(status));
    REQUIRE(WTERMSIG(status) == expectSignal);
    return slurp(log);
}

} // namespace

TEST_CASE("core/CrashHook writes the named fatal line, then dies as before", "[core][crash]") {
    const auto dir = dft::temp_dir("crashhook");
    CHECK(run_crashed_child(dir, "abort", SIGABRT) == "FATAL [crash] signal 6 (SIGABRT)\n");
    CHECK(run_crashed_child(dir, "segv", SIGSEGV) == "FATAL [crash] signal 11 (SIGSEGV)\n");
    CHECK(run_crashed_child(dir, "terminate", SIGABRT) == "FATAL [crash] std::terminate called\n");
}

#endif // _WIN32
