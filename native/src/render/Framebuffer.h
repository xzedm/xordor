#pragma once
// Framebuffer: an offscreen image to draw into instead of the window.
//
// The editor draws the scene into one of these, then shows its color texture
// inside a panel like any other image. That's how a viewport can be a
// resizable, dockable window rather than the whole screen.
//
//   color   a texture (so it can be shown, or read by a later shader)
//   depth   a "renderbuffer": storage the GPU can depth-test against but
//           nobody samples, which lets the driver keep it in a faster layout
//
// While bound, every draw call lands here. bindDefault() goes back to the window.

#include "render/GLObject.h"

namespace Engine {

class Framebuffer {
public:
    Framebuffer(int width, int height);

    Framebuffer(Framebuffer&&) = default;
    Framebuffer& operator=(Framebuffer&&) = default;

    // Reallocates the images when the size changed (a panel was resized).
    // Sizes below 1 are clamped to 1: a zero-size texture is invalid.
    void resize(int width, int height);

    void bind() const;
    static void bindDefault();

    GLuint colorTexture() const { return color_.id(); }
    int width() const { return width_; }
    int height() const { return height_; }

private:
    void allocate();

    GLFramebuffer framebuffer_;
    GLTexture color_;
    GLRenderbuffer depth_;
    int width_ = 1;
    int height_ = 1;
};

} // namespace Engine
