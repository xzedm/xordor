#include "render/Texture.h"

#include "render/GLCheck.h"

namespace Engine {

Texture::Texture(const Image& image, const TextureSettings& settings) : width_(image.width), height_(image.height) {
    // OpenGL expects the first row of texture data to be the *bottom* of the
    // image (texture coordinate v = 0), but files store the top row first.
    // Flipping a copy here means every mesh can use the usual "v = 0 is the
    // bottom" UVs, which is also what OBJ files and most tools use.
    // (glTF uses top-left UVs; its importer will flip v instead, in Phase 4.)
    Image flipped = image;
    flipVertically(flipped);

    GLuint id = 0;
    glGenTextures(1, &id);
    texture_ = GLTexture(id);
    glBindTexture(GL_TEXTURE_2D, id);

    // Rows of RGBA8 are always a multiple of 4 bytes, so the default alignment
    // of 4 is fine; set it anyway so earlier code can't break this upload.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    const GLint internalFormat = settings.colorSpace == ColorSpace::SRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 flipped.pixels.data());

    const GLint wrap = settings.repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);

    // Minification: the texture is smaller on screen than in memory.
    // GL_LINEAR_MIPMAP_LINEAR ("trilinear") blends between two mip levels too.
    GLint minFilter = settings.smooth ? GL_LINEAR : GL_NEAREST;
    if (settings.mipmaps) {
        minFilter = settings.smooth ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    // Magnification never uses mipmaps (there is nothing bigger than level 0).
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, settings.smooth ? GL_LINEAR : GL_NEAREST);
    // TODO(quality): anisotropic filtering keeps floors sharp at grazing angles.
    // It is core only from OpenGL 4.6, but nearly every driver has the extension.

    glBindTexture(GL_TEXTURE_2D, 0);
    XE_GL_CHECK("creating a texture");
}

std::optional<Texture> Texture::fromFile(const std::string& path, const TextureSettings& settings, std::string* error) {
    auto image = loadImage(path, error);
    if (!image) {
        return std::nullopt;
    }
    return Texture(*image, settings);
}

Texture Texture::solidColor(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    Image image(1, 1);
    image.setPixel(0, 0, r, g, b, a);
    TextureSettings settings;
    settings.mipmaps = false;
    return Texture(image, settings);
}

void Texture::bind(int unit) const {
    glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
    glBindTexture(GL_TEXTURE_2D, texture_.id());
}

} // namespace Engine
