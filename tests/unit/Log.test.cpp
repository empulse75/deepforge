// dispatch() is the single choke point every recorded line passes through,
// so the console, every sink and the debug-overlay ring must all see the same
// escaped single line (SEC-07). Pin it so the escape path cannot rot silently.
#include "TestSupport.hpp"

#include "core/Log.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using namespace df;

namespace {

class CaptureSink final : public LogSink {
public:
    std::vector<std::string> categories;
    std::vector<std::string> messages;

    void write(LogLevel level, std::string_view category, std::string_view message) override {
        static_cast<void>(level);
        categories.push_back(std::string(category));
        messages.push_back(std::string(message));
    }
};

} // namespace

TEST_CASE("core/Log escapes control bytes before any consumer sees them", "[core][log]") {
    CaptureSink sink;
    clear_log_sinks();
    add_log_sink(&sink);

    // One dispatch must stay one record: the embedded newline cannot split it.
    DF_LOG_INFO("test", "{}", std::string("forged\n[INFO] [test] also forged\x1b[0m"));
    DF_LOG_INFO("test", "clean stays clean");

    clear_log_sinks();

    REQUIRE(sink.messages.size() == 2);
    CHECK(sink.categories[0] == "test");
    CHECK(sink.messages[0] == "forged\\n[INFO] [test] also forged\\x1b[0m");
    CHECK(sink.categories[1] == "test");
    CHECK(sink.messages[1] == "clean stays clean");

    std::size_t count = 0;
    const std::string* lines = recent_log_lines(count);
    REQUIRE(count == 2); // one process per case; only this test logs
    CHECK(lines[0] == "[INFO] [test] forged\\n[INFO] [test] also forged\\x1b[0m");
    CHECK(lines[1] == "[INFO] [test] clean stays clean");
}
