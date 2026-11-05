#pragma once
// Saves what is currently drawn to an image file.
//
// The format is binary PPM ("P6"): a short text header and then raw RGB bytes.
// It needs no library to write, and most image viewers (and GIMP, Photoshop,
// ImageMagick) open it. Phase 2 can switch to PNG once stb is in the build.

#include <string>

namespace Engine {

// Reads the current OpenGL framebuffer (width x height pixels) and writes it to
// `path`. Call it after drawing and before swapBuffers(), while the frame is
// still in the back buffer. Returns false (and logs why) on failure.
bool saveScreenshot(const std::string& path, int width, int height);

} // namespace Engine
