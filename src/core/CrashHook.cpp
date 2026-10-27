#include "core/CrashHook.hpp"

#include <csignal>
#include <cstddef>
#include <string>

// Plain POSIX names everywhere except MSVC, which insists on the underscore
// spellings; MinGW (the Windows cross build) uses the POSIX ones.
#ifdef _MSC_VER
#include <fcntl.h>
#include <io.h>
#define DF_OPEN _open
#define DF_WRITE _write
#define DF_CLOSE _close
#define DF_OPEN_FLAGS (_O_WRONLY | _O_APPEND | _O_CREAT | _O_NOFOLLOW)
#else
#include <fcntl.h>
#include <unistd.h>
#define DF_OPEN open
#define DF_WRITE write
#define DF_CLOSE close
#define DF_OPEN_FLAGS (O_WRONLY | O_APPEND | O_CREAT | O_NOFOLLOW)
#endif

namespace df {
namespace {

// The handler may call almost nothing, so the path is copied into fixed
// static storage at install time and every line is assembled by hand.
constexpr std::size_t kPathCapacity = 4096; // covers PATH_MAX and MAX_PATH
char g_path[kPathCapacity];
bool g_pathReady = false;
volatile std::sig_atomic_t g_lineWritten = 0;

void write_all(int fd, const char* data, std::size_t n) {
    while (n > 0) {
        const long written = DF_WRITE(fd, data, n);
        if (written <= 0) {
            return;
        }
        data += written;
        n -= static_cast<std::size_t>(written);
    }
}

/// Writes "FATAL [crash] <body>\n" to the log file and to stderr. Only called
/// once per process (g_lineWritten guards the rest).
void emit_fatal(const char* body) {
    static constexpr char kPrefix[] = "FATAL [crash] ";
    static char line[kPathCapacity + 64]; // prefix + body (a path) + newline + NUL
    std::size_t n = 0;
    for (const char* p = kPrefix; *p; ++p) {
        line[n++] = *p;
    }
    for (const char* p = body; *p && n + 2 < sizeof(line); ++p) {
        line[n++] = *p;
    }
    line[n++] = '\n';
    line[n] = '\0';
    if (g_pathReady) {
        const int fd = DF_OPEN(g_path, DF_OPEN_FLAGS, 0600);
        if (fd >= 0) {
            write_all(fd, line, n);
            DF_CLOSE(fd);
        }
    }
    write_all(2, line, n); // stderr
}

constexpr const char* sig_name(int sig) {
    if (sig == SIGSEGV) {
        return "SIGSEGV";
    }
    if (sig == SIGABRT) {
        return "SIGABRT";
    }
    return "?";
}

void append_text(char*& p, const char* text) {
    for (; *text; ++text) {
        *p++ = *text;
    }
}

void append_signal_number(char*& p, int sig) {
    if (sig >= 100) {
        *p++ = static_cast<char>('0' + sig / 100);
        sig %= 100;
    }
    if (sig >= 10) {
        *p++ = static_cast<char>('0' + sig / 10);
        sig %= 10;
    }
    *p++ = static_cast<char>('0' + sig);
}

void fatal_handler(int sig) {
    if (!g_lineWritten) {
        g_lineWritten = 1;
        char body[64];
        char* p = body;
        append_text(p, "signal ");
        append_signal_number(p, sig);
        append_text(p, " (");
        append_text(p, sig_name(sig));
        *p++ = ')';
        *p = '\0';
        emit_fatal(body);
    }
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

void terminate_handler() {
    if (!g_lineWritten) {
        g_lineWritten = 1;
        emit_fatal("std::terminate called");
    }
    std::signal(SIGABRT, SIG_DFL); // abort() must die, not re-enter the hook
    std::abort();                  // the exit status stays the one tests pin (134)
}

} // namespace

void install_crash_hook(const std::filesystem::path& logFile) {
    const std::string path = logFile.string();
    // An over-long path is truncated; the stderr line is written either way,
    // and a user-data path that long is not a case worth machinery for.
    const std::size_t n = path.size() < kPathCapacity ? path.size() : kPathCapacity - 1;
    for (std::size_t i = 0; i < n; ++i) {
        g_path[i] = path[i];
    }
    g_path[n] = '\0';
    g_pathReady = n > 0;
    std::signal(SIGSEGV, fatal_handler);
    std::signal(SIGABRT, fatal_handler);
    std::set_terminate(terminate_handler);
}

} // namespace df
