#include "core/Screenshot.h"

#include "core/Log.h"

#include <glad/gl.h>
// Declarations only; the implementation is compiled once in render/StbImpl.cpp.
#include <stb_image_write.h>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace Engine {

bool saveScreenshot(const std::string& path, int width, int height) {
    if (width <= 0 || height <= 0) {
        ENGINE_WARN("Screenshot skipped: the framebuffer is ", width, "x", height);
        return false;
    }
    const size_t rowBytes = static_cast<size_t>(width) * 3;
    std::vector<std::uint8_t> pixels(rowBytes * static_cast<size_t>(height));

    // By default OpenGL pads each row to a multiple of 4 bytes. RGB rows are
    // 3 * width bytes, which often isn't, so ask for tightly packed rows.
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // OpenGL's row 0 is the bottom of the image; image files start at the top.
    for (int top = 0, bottom = height - 1; top < bottom; ++top, --bottom) {
        std::swap_ranges(pixels.begin() + static_cast<std::ptrdiff_t>(top * rowBytes),
                         pixels.begin() + static_cast<std::ptrdiff_t>((top + 1) * rowBytes),
                         pixels.begin() + static_cast<std::ptrdiff_t>(bottom * rowBytes));
    }

    if (!stbi_write_png(path.c_str(), width, height, 3, pixels.data(), static_cast<int>(rowBytes))) {
        ENGINE_ERROR("Could not write screenshot to ", path);
        return false;
    }
    ENGINE_INFO("Saved screenshot ", path, " (", width, "x", height, ")");
    return true;
}

} // namespace Engine
