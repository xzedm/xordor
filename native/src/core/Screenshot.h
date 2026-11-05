#pragma once
// Saves what is currently drawn to a PNG file.

#include <string>

namespace Engine {

// Reads the current OpenGL framebuffer (width x height pixels) and writes it to
// `path` as a PNG. Call it after drawing and before swapBuffers(), while the
// frame is still in the back buffer. Returns false (and logs why) on failure.
bool saveScreenshot(const std::string& path, int width, int height);

} // namespace Engine
