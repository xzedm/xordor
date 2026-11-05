#pragma once
// Makes shader compiler errors readable.
//
// Drivers report errors with a line number but no source, and every vendor
// formats it differently:
//
//   Mesa (Linux):            0:12(5): error: `colr' undeclared
//   NVIDIA, AMD, Intel:      0(12) : error C1008: undefined variable "colr"
//   Apple:                   ERROR: 0:12: Use of undeclared identifier 'colr'
//
// annotateShaderLog() finds those line numbers and adds the offending source
// line under each message, so the log shows the mistake itself.
// It is plain string work, so it is unit tested without a GPU.

#include <string>
#include <vector>

namespace Engine {

// Line numbers (1-based) mentioned in a compiler log, in order, without repeats.
std::vector<int> shaderLogLines(const std::string& log);

// The log with "  12 | <source line>" printed after every message that names a line.
std::string annotateShaderLog(const std::string& source, const std::string& log);

} // namespace Engine
