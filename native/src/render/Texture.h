#pragma once
// Texture: an image stored on the GPU, sampled by shaders.
//
// Two decisions matter for every texture:
//
// 1. Color space. Images made for people (photos, painted textures) store
//    colors in sRGB, which spends more numbers on dark tones because our eyes
//    are more sensitive there. Lighting math must happen on *linear* values.
//    Uploading with an sRGB format makes the GPU convert to linear for free
//    each time the shader samples. Data textures (normal maps, masks) are
//    already linear and must not be converted.
//
// 2. Filtering. Mipmaps are pre-shrunk copies of the image (1/2, 1/4, ...).
//    When a texture is far away, the GPU reads the copy that matches its
//    on-screen size; without them, distant textures shimmer and sparkle.

#include "render/GLObject.h"
#include "render/Image.h"

#include <optional>
#include <string>

namespace Engine {

enum class ColorSpace { SRGB, Linear };

struct TextureSettings {
    ColorSpace colorSpace = ColorSpace::SRGB;
    bool mipmaps = true;
    bool repeat = true;  // tile when UVs go past 0..1 (false: stretch the edge pixels)
    bool smooth = true;  // blend neighboring pixels (false: crisp, "pixel art" look)
};

class Texture {
public:
    Texture(const Image& image, const TextureSettings& settings = {});
    // Convenience: loads a file. Returns nothing (and sets *error) on failure.
    static std::optional<Texture> fromFile(const std::string& path, const TextureSettings& settings = {},
                                           std::string* error = nullptr);
    // A 1x1 texture of one color. Used as "no texture": a white texture
    // multiplied by a color is just the color, so shaders need no special case.
    static Texture solidColor(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);

    Texture(Texture&&) = default;
    Texture& operator=(Texture&&) = default;

    // Shaders read textures from numbered "units". Binding puts this texture
    // in a unit; the shader's sampler uniform says which unit to read.
    void bind(int unit) const;

    int width() const { return width_; }
    int height() const { return height_; }
    GLuint id() const { return texture_.id(); }

private:
    GLTexture texture_;
    int width_ = 0;
    int height_ = 0;
};

} // namespace Engine
