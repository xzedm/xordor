// Tests for reading shader compiler error logs. Every GPU vendor words its
// errors differently; these are real formats from Mesa (Linux), NVIDIA, and
// Apple, so a typo in a shader is always shown next to the line it's on.

#include "render/ShaderLog.h"

#include <doctest/doctest.h>

using namespace Engine;

TEST_SUITE("shaderlog") {
    TEST_CASE("finds the line number in each vendor's format") {
        // Mesa: <source>:<line>(<column>): error: ...
        CHECK(shaderLogLines("0:12(5): error: `foo' undeclared") == std::vector<int>{12});
        // NVIDIA: <source>(<line>) : error C...: ...
        CHECK(shaderLogLines("0(7) : error C1008: undefined variable \"foo\"") == std::vector<int>{7});
        // Apple and AMD: ERROR: <source>:<line>: ...
        CHECK(shaderLogLines("ERROR: 0:31: 'foo' : undeclared identifier") == std::vector<int>{31});
    }

    TEST_CASE("lists each line once, in the order the errors appear") {
        const std::string log = "0:9(1): error: first\n"
                                "0:4(2): error: second\n"
                                "0:9(8): warning: same line again\n";
        CHECK(shaderLogLines(log) == std::vector<int>{9, 4});
    }

    TEST_CASE("log lines without a location are ignored") {
        CHECK(shaderLogLines("Link failed.\nOut of memory").empty());
        CHECK(shaderLogLines("").empty());
    }

    TEST_CASE("annotate adds the failing source line under each error") {
        const std::string source = "#version 410 core\n"
                                   "out vec4 color;\n"
                                   "void main() { color = vec4(missing); }\n";
        const std::string log = "0:3(29): error: `missing' undeclared\n";
        const std::string annotated = annotateShaderLog(source, log);
        CHECK(annotated.find("0:3(29): error: `missing' undeclared") != std::string::npos);
        CHECK(annotated.find("    3 | void main() { color = vec4(missing); }") != std::string::npos);
    }

    TEST_CASE("annotate handles Windows line endings and out-of-range lines") {
        const std::string source = "line one\r\nline two\r\n";
        const std::string annotated = annotateShaderLog(source, "0:2(1): error: a\n0:99(1): error: b\n");
        CHECK(annotated.find("    2 | line two\n") != std::string::npos); // no stray '\r'
        CHECK(annotated.find("99 |") == std::string::npos);                // no such line: nothing added
        CHECK(annotated.find("0:99(1): error: b") != std::string::npos);   // but the message is kept
    }
}
