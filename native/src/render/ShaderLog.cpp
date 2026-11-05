#include "render/ShaderLog.h"

#include <algorithm>
#include <regex>
#include <sstream>

namespace Engine {
namespace {

// The source line number in one log line, or 0 if there is none.
int lineNumberIn(const std::string& logLine) {
    // Patterns for the three formats in ShaderLog.h. The first number is the
    // source string index (always 0 for us); the second is the line.
    static const std::regex mesaOrApple(R"((?:^|\s)\d+:(\d+)(?:\(\d+\))?:)");
    static const std::regex nvidia(R"(^\s*\d+\((\d+)\)\s*:)");
    std::smatch match;
    if (std::regex_search(logLine, match, nvidia) || std::regex_search(logLine, match, mesaOrApple)) {
        return std::stoi(match[1].str());
    }
    return 0;
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back(); // files saved on Windows
        }
        lines.push_back(line);
    }
    return lines;
}

} // namespace

std::vector<int> shaderLogLines(const std::string& log) {
    std::vector<int> result;
    for (const std::string& line : splitLines(log)) {
        const int n = lineNumberIn(line);
        if (n > 0 && std::find(result.begin(), result.end(), n) == result.end()) {
            result.push_back(n);
        }
    }
    return result;
}

std::string annotateShaderLog(const std::string& source, const std::string& log) {
    const std::vector<std::string> sourceLines = splitLines(source);
    std::ostringstream out;
    for (const std::string& line : splitLines(log)) {
        if (line.empty()) {
            continue;
        }
        out << line << '\n';
        const int n = lineNumberIn(line);
        if (n > 0 && n <= static_cast<int>(sourceLines.size())) {
            out << "    " << n << " | " << sourceLines[static_cast<size_t>(n - 1)] << '\n';
        }
    }
    return out.str();
}

} // namespace Engine
