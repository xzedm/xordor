#include "core/Log.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <mutex>
#include <vector>

#if defined(_WIN32)
#include <io.h>
#define XE_ISATTY _isatty
#define XE_FILENO _fileno
#else
#include <unistd.h>
#define XE_ISATTY isatty
#define XE_FILENO fileno
#endif

namespace Engine {
namespace {

struct SinkEntry {
    int id;
    Log::Sink sink;
};

// All logger state in one place. It lives inside a function (see state())
// instead of as a global, because globals in different .cpp files are
// constructed in an unspecified order: another global's constructor might log
// before a global logger existed. A function-local static is created the first
// time it is used, which is always early enough.
struct LogState {
    std::mutex mutex; // one message at a time, so lines from two threads never interleave
    // Atomic because every log call reads it (before taking the lock, to skip
    // disabled messages cheaply) while another thread might be changing it.
    std::atomic<LogLevel> minLevel{LogLevel::Trace};
    bool consoleEnabled = true;
    bool consoleColors = false;
    std::ofstream file;
    std::vector<SinkEntry> sinks;
    int nextSinkId = 1;

    LogState() {
        // Colors only when printing to a real terminal; escape codes in a log
        // file or an IDE output pane are just noise.
        consoleColors = XE_ISATTY(XE_FILENO(stdout)) != 0;
#if defined(_WIN32)
        consoleColors = false; // TODO: enable virtual terminal processing on Windows 10+ for colors.
#endif
    }
};

LogState& state() {
    static LogState instance;
    return instance;
}

// "14:03:59.123", local time with milliseconds.
std::string currentTime() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const std::time_t seconds = system_clock::to_time_t(now);
    const auto ms = duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000;
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &seconds); // the portable std::localtime is not thread-safe
#else
    localtime_r(&seconds, &local);
#endif
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d.%03d", local.tm_hour, local.tm_min, local.tm_sec,
                  static_cast<int>(ms));
    return buffer;
}

// "C:\\src\\core\\Window.cpp" or "/home/me/src/core/Window.cpp" -> "Window.cpp"
std::string_view fileName(const char* path) {
    std::string_view p(path);
    const auto slash = p.find_last_of("/\\");
    return slash == std::string_view::npos ? p : p.substr(slash + 1);
}

const char* levelColor(LogLevel level) {
    switch (level) {
    case LogLevel::Trace: return "\x1b[90m"; // grey
    case LogLevel::Info: return "\x1b[32m";  // green
    case LogLevel::Warn: return "\x1b[33m";  // yellow
    case LogLevel::Error: return "\x1b[31m"; // red
    default: return "";
    }
}

} // namespace

void Log::setMinLevel(LogLevel level) {
    state().minLevel.store(level);
}

LogLevel Log::minLevel() {
    return state().minLevel.load();
}

bool Log::setLogFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(state().mutex);
    auto& s = state();
    if (s.file.is_open()) {
        s.file.close();
    }
    if (path.empty()) {
        return true;
    }
    s.file.open(path, std::ios::out | std::ios::app);
    return s.file.is_open();
}

void Log::setConsoleEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(state().mutex);
    state().consoleEnabled = enabled;
}

int Log::addSink(Sink sink) {
    std::lock_guard<std::mutex> lock(state().mutex);
    auto& s = state();
    const int id = s.nextSinkId++;
    s.sinks.push_back({id, std::move(sink)});
    return id;
}

void Log::removeSink(int id) {
    std::lock_guard<std::mutex> lock(state().mutex);
    auto& sinks = state().sinks;
    for (auto it = sinks.begin(); it != sinks.end(); ++it) {
        if (it->id == id) {
            sinks.erase(it);
            return;
        }
    }
}

const char* Log::levelName(LogLevel level) {
    switch (level) {
    case LogLevel::Trace: return "TRACE";
    case LogLevel::Info: return "INFO";
    case LogLevel::Warn: return "WARN";
    case LogLevel::Error: return "ERROR";
    case LogLevel::Off: return "OFF";
    }
    return "?";
}

void Log::emit(LogLevel level, const char* file, int line, const std::string& message) {
    const std::string time = currentTime();
    const std::string location = std::string(fileName(file)) + ":" + std::to_string(line);

    std::lock_guard<std::mutex> lock(state().mutex);
    auto& s = state();
    // Padding the level name keeps messages lined up in a column.
    char levelText[8];
    std::snprintf(levelText, sizeof(levelText), "%-5s", levelName(level));

    if (s.consoleEnabled) {
        // Warnings and errors go to stderr so they still show when stdout is redirected.
        std::ostream& out = level >= LogLevel::Warn ? std::cerr : std::cout;
        if (s.consoleColors) {
            out << "\x1b[90m" << time << "\x1b[0m " << levelColor(level) << levelText << "\x1b[0m "
                << "\x1b[90m" << location << "\x1b[0m  " << message << '\n';
        } else {
            out << time << ' ' << levelText << ' ' << location << "  " << message << '\n';
        }
        // Errors are flushed right away: if the program crashes next, the message is not lost in a buffer.
        if (level >= LogLevel::Error) {
            out.flush();
        }
    }
    if (s.file.is_open()) {
        s.file << time << ' ' << levelText << ' ' << location << "  " << message << '\n';
        s.file.flush();
    }
    const LogRecord record{level, time, location, message};
    for (const auto& entry : s.sinks) {
        entry.sink(record);
    }
}

} // namespace Engine
