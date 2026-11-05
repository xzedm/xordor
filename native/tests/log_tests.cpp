#include "core/Log.h"

#include <doctest/doctest.h>

#include <ostream>
#include <string>
#include <vector>

using namespace Engine;

namespace {

// Captures messages for one test and restores the logger afterwards (RAII,
// so a failing CHECK can't leave the logger in a strange state for other tests).
struct CapturedLog {
    struct Entry {
        LogLevel level;
        std::string location;
        std::string message;
    };
    std::vector<Entry> entries;
    int sinkId;

    CapturedLog() {
        Log::setConsoleEnabled(false);
        sinkId = Log::addSink([this](const LogRecord& r) {
            entries.push_back({r.level, std::string(r.location), std::string(r.message)});
        });
    }
    ~CapturedLog() {
        Log::removeSink(sinkId);
        Log::setMinLevel(LogLevel::Trace);
        Log::setConsoleEnabled(true);
    }
};

// Counts how often it is written to a stream, to prove skipped messages do no work.
struct CountsStreaming {
    int* count;
};

std::ostream& operator<<(std::ostream& out, const CountsStreaming& value) {
    ++*value.count;
    return out << "x";
}

} // namespace

TEST_SUITE("log") {

TEST_CASE("arguments are streamed into one message") {
    CapturedLog log;
    ENGINE_INFO("Loaded ", 3, " meshes in ", 1.5, " ms");
    REQUIRE(log.entries.size() == 1);
    CHECK(log.entries[0].level == LogLevel::Info);
    CHECK(log.entries[0].message == "Loaded 3 meshes in 1.5 ms");
}

TEST_CASE("the location is the file name and line, without folders") {
    CapturedLog log;
    ENGINE_WARN("careful");
    REQUIRE(log.entries.size() == 1);
    CHECK(log.entries[0].location.rfind("log_tests.cpp:", 0) == 0);
}

TEST_CASE("messages below the minimum level are dropped") {
    CapturedLog log;
    Log::setMinLevel(LogLevel::Warn);
    ENGINE_TRACE("hidden");
    ENGINE_INFO("hidden too");
    ENGINE_ERROR("shown");
    REQUIRE(log.entries.size() == 1);
    CHECK(log.entries[0].message == "shown");
}

TEST_CASE("disabled levels skip the formatting work") {
    CapturedLog log;
    Log::setMinLevel(LogLevel::Error);
    int streamed = 0;
    ENGINE_INFO(CountsStreaming{&streamed}); // below the level: never streamed
    CHECK(streamed == 0);
    CHECK(log.entries.empty());
    ENGINE_ERROR(CountsStreaming{&streamed});
    CHECK(streamed == 1);
    CHECK(log.entries.size() == 1);
}

TEST_CASE("a removed sink receives nothing") {
    CapturedLog log;
    int count = 0;
    const int id = Log::addSink([&count](const LogRecord&) { ++count; });
    ENGINE_INFO("one");
    Log::removeSink(id);
    ENGINE_INFO("two");
    CHECK(count == 1);
    CHECK(log.entries.size() == 2);
}

TEST_CASE("level names") {
    CHECK(std::string(Log::levelName(LogLevel::Error)) == "ERROR");
    CHECK(std::string(Log::levelName(LogLevel::Trace)) == "TRACE");
}

} // TEST_SUITE
