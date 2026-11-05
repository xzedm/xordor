#pragma once
// Image: pixels in main memory (CPU side), before they become a GPU Texture.
//
// Always 8-bit RGBA, 4 bytes per pixel, rows from top to bottom (the way image
// files and most people think). Keeping one format makes everything that
// touches pixels simpler; the cost is a little memory for images without alpha.

#include "core/Math.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Engine {

struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels; // width * height * 4 bytes

    Image() = default;
    // Every pixel starts transparent black (all bytes 0), like a blank canvas.
    Image(int w, int h);

    bool empty() const { return width <= 0 || height <= 0; }
    // Pixel (x, y), y = 0 is the top row. Returns the RGBA bytes.
    std::uint8_t* at(int x, int y);
    const std::uint8_t* at(int x, int y) const;
    void setPixel(int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);
};

// Loads PNG, JPEG, TGA, BMP and more (via stb_image). Returns nothing and sets
// *error on failure (missing file, unsupported format...).
std::optional<Image> loadImage(const std::string& path, std::string* error = nullptr);

// Writes a PNG. Returns false on failure.
bool savePng(const std::string& path, const Image& image);

// A size x size checkerboard of `cells` x `cells` squares, alternating two
// colors given in 0..1 (sRGB, like colors in a paint program).
Image makeCheckerboard(int size, int cells, const Vec3& colorA, const Vec3& colorB);

// Swaps the top and bottom rows. OpenGL's texture coordinate (0, 0) is the
// bottom-left corner, image files start at the top-left; see Texture.cpp.
void flipVertically(Image& image);

} // namespace Engine
