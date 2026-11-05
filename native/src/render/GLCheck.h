#pragma once
// Reporting OpenGL errors.
//
// OpenGL doesn't throw or return errors from most calls. It sets an error flag
// that stays until someone asks with glGetError(), and a failed call usually
// just does nothing. So a mistake (wrong enum, missing bind) shows up as "the
// screen is black" with no clue where.
//
// XE_GL_CHECK("what we were doing") reads every pending error and logs it with
// a readable name and the caller's file and line. Put it after groups of GL
// calls. In release builds it compiles to nothing, because glGetError makes the
// CPU wait for the GPU driver, which is slow.
//
// OpenGL 4.3 added a much better tool (debug output callbacks), but macOS stops
// at 4.1, so we use the portable way.

namespace Engine {

// Logs all pending OpenGL errors. Returns true if there were none.
bool checkGLErrors(const char* what, const char* file, int line);

// "GL_INVALID_ENUM" for 0x0500, and so on.
const char* glErrorName(unsigned int error);

} // namespace Engine

#if defined(NDEBUG)
#define XE_GL_CHECK(what) ((void)0)
#else
#define XE_GL_CHECK(what) ::Engine::checkGLErrors(what, __FILE__, __LINE__)
#endif
