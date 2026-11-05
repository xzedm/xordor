#include "render/Image.h"

#include "core/Assert.h"

// The stb functions are declared here and implemented in StbImpl.cpp.
#include <stb_image.h>
#include <stb_image_write.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Engine {

Image::Image(int w, int h) : width(w), height(h), pixels(static_cast<size_t>(w) * static_cast<size_t>(h) * 4, 0) {
    ENGINE_ASSERT(w > 0 && h > 0, "an image needs a positive size");
}

std::uint8_t* Image::at(int x, int y) {
    ENGINE_ASSERT(x >= 0 && y >= 0 && x < width && y < height, "pixel outside the image");
    return pixels.data() + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
}

const std::uint8_t* Image::at(int x, int y) const {
    return const_cast<Image*>(this)->at(x, y);
}

void Image::setPixel(int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    std::uint8_t* p = at(x, y);
    p[0] = r;
    p[1] = g;
    p[2] = b;
    p[3] = a;
}

std::optional<Image> loadImage(const std::string& path, std::string* error) {
    int width = 0;
    int height = 0;
    int channelsInFile = 0;
    // Asking for 4 channels makes stb convert grey, grey+alpha and RGB files to RGBA.
    stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channelsInFile, 4);
    if (!data) {
        if (error) {
            *error = "Could not load image " + path + ": " + stbi_failure_reason();
        }
        return std::nullopt;
    }
    Image image(width, height);
    std::memcpy(image.pixels.data(), data, image.pixels.size());
    // stb allocates with malloc; it must be freed by stb, not by delete.
    stbi_image_free(data);
    return image;
}

bool savePng(const std::string& path, const Image& image) {
    if (image.empty()) {
        return false;
    }
    return stbi_write_png(path.c_str(), image.width, image.height, 4, image.pixels.data(), image.width * 4) != 0;
}

Image makeCheckerboard(int size, int cells, const Vec3& colorA, const Vec3& colorB) {
    ENGINE_ASSERT(size > 0 && cells > 0, "checkerboard needs a positive size and cell count");
    Image image(size, size);
    const auto toByte = [](float v) { return static_cast<std::uint8_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f)); };
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const int cx = x * cells / size;
            const int cy = y * cells / size;
            const Vec3& c = ((cx + cy) % 2 == 0) ? colorA : colorB;
            image.setPixel(x, y, toByte(c.x), toByte(c.y), toByte(c.z));
        }
    }
    return image;
}

void flipVertically(Image& image) {
    const size_t rowBytes = static_cast<size_t>(image.width) * 4;
    for (int top = 0, bottom = image.height - 1; top < bottom; ++top, --bottom) {
        std::swap_ranges(image.pixels.begin() + static_cast<std::ptrdiff_t>(top * rowBytes),
                         image.pixels.begin() + static_cast<std::ptrdiff_t>((top + 1) * rowBytes),
                         image.pixels.begin() + static_cast<std::ptrdiff_t>(bottom * rowBytes));
    }
}

} // namespace Engine
