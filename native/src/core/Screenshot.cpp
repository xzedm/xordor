#include "core/Screenshot.h"

#include "core/Log.h"

#include <glad/gl.h>

#include <cstdint>
#include <fstream>
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

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        ENGINE_ERROR("Could not write screenshot to ", path);
        return false;
    }
    file << "P6\n" << width << ' ' << height << "\n255\n";
    // OpenGL's row 0 is the bottom of the image; image files start at the top.
    for (int row = height - 1; row >= 0; --row) {
        file.write(reinterpret_cast<const char*>(pixels.data() + static_cast<size_t>(row) * rowBytes),
                   static_cast<std::streamsize>(rowBytes));
    }
    ENGINE_INFO("Saved screenshot ", path, " (", width, "x", height, ")");
    return static_cast<bool>(file);
}

} // namespace Engine
