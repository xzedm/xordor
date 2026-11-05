#pragma once
// ENGINE_ASSERT(condition, "message", ...)
//
// Checks something that must always be true if the code is correct (a
// programmer mistake, not bad user input). In debug builds a failure logs the
// condition and message, then stops the program right there, so the debugger
// or the crash points at the cause instead of some later symptom.
// In release builds (NDEBUG) the check is removed entirely, so asserts cost
// nothing. That is also why the condition must never have side effects.
//
// For errors that can really happen (a missing file, a failed driver call),
// log and handle them instead; don't assert.

#include "core/Log.h"

#include <cstdlib>

#if defined(NDEBUG)
#define ENGINE_ASSERT(condition, ...) ((void)0)
#else
#define ENGINE_ASSERT(condition, ...)                                                                                \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            ::Engine::Log::write(::Engine::LogLevel::Error, __FILE__, __LINE__, "Assertion failed: ", #condition,      \
                                 " -- ", __VA_ARGS__);                                                                 \
            std::abort();                                                                                              \
        }                                                                                                              \
    } while (false)
#endif
