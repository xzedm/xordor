#pragma once
// Logging: leveled, timestamped messages.
//
//   ENGINE_INFO("Loaded ", count, " meshes in ", ms, " ms");
//
// Arguments are streamed one after another (like std::cout), so anything with
// an operator<< can be logged and no format string can be mistyped.
// Every message carries its level, the time, and the file and line it came from.
//
// Messages go to "sinks": the console by default, optionally a file, and any
// callback you add (the unit tests add one to check what was logged).

#include <functional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace Engine {

enum class LogLevel { Trace = 0, Info = 1, Warn = 2, Error = 3, Off = 4 };

// One finished message, as handed to sinks.
struct LogRecord {
    LogLevel level;
    std::string_view time;     // "14:03:59.123"
    std::string_view location; // "Window.cpp:42"
    std::string_view message;  // the text without decoration
};

class Log {
public:
    using Sink = std::function<void(const LogRecord&)>;

    // Messages below this level are dropped before any formatting work is done.
    static void setMinLevel(LogLevel level);
    static LogLevel minLevel();

    // Also write every message to a file (appends). Returns false if the file
    // could not be opened. An empty path closes the file.
    static bool setLogFile(const std::string& path);

    // Turn the console output on or off (tests switch it off to stay quiet).
    static void setConsoleEnabled(bool enabled);

    // Extra destinations. addSink returns an id that removeSink accepts.
    static int addSink(Sink sink);
    static void removeSink(int id);

    static const char* levelName(LogLevel level);

    // Called by the ENGINE_* macros. Kept as a template so streaming the
    // arguments happens only when the level is enabled.
    template <typename... Args>
    static void write(LogLevel level, const char* file, int line, Args&&... args) {
        if (level < minLevel()) {
            return;
        }
        std::ostringstream text;
        (text << ... << std::forward<Args>(args)); // C++17 fold expression
        emit(level, file, line, text.str());
    }

private:
    static void emit(LogLevel level, const char* file, int line, const std::string& message);
};

} // namespace Engine

#define ENGINE_TRACE(...) ::Engine::Log::write(::Engine::LogLevel::Trace, __FILE__, __LINE__, __VA_ARGS__)
#define ENGINE_INFO(...)  ::Engine::Log::write(::Engine::LogLevel::Info, __FILE__, __LINE__, __VA_ARGS__)
#define ENGINE_WARN(...)  ::Engine::Log::write(::Engine::LogLevel::Warn, __FILE__, __LINE__, __VA_ARGS__)
#define ENGINE_ERROR(...) ::Engine::Log::write(::Engine::LogLevel::Error, __FILE__, __LINE__, __VA_ARGS__)
