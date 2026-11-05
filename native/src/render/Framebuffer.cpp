#include "render/Framebuffer.h"

#include "core/Log.h"
#include "render/GLCheck.h"

#include <algorithm>

namespace Engine {

Framebuffer::Framebuffer(int width, int height) : width_(std::max(width, 1)), height_(std::max(height, 1)) {
    // glGen* write names into an array; we make one of each.
    GLuint ids[3] = {};
    glGenFramebuffers(1, &ids[0]);
    glGenTextures(1, &ids[1]);
    glGenRenderbuffers(1, &ids[2]);
    framebuffer_ = GLFramebuffer(ids[0]);
    color_ = GLTexture(ids[1]);
    depth_ = GLRenderbuffer(ids[2]);
    allocate();
}

void Framebuffer::resize(int width, int height) {
    width = std::max(width, 1);
    height = std::max(height, 1);
    if (width == width_ && height == height_) {
        return;
    }
    width_ = width;
    height_ = height;
    allocate();
}

void Framebuffer::allocate() {
    // Color: plain 8-bit RGBA. The lit shader already writes display-ready
    // (gamma-encoded) colors, so no sRGB format here; the panel shows the
    // bytes as they are. Linear filtering in case the panel is scaled.
    glBindTexture(GL_TEXTURE_2D, color_.id());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindRenderbuffer(GL_RENDERBUFFER, depth_.id());
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width_, height_);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_.id());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_.id(), 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_.id());
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        // Shouldn't happen with these formats; say so loudly if a driver disagrees.
        ENGINE_ERROR("Framebuffer ", width_, "x", height_, " is incomplete; the viewport will stay empty");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    XE_GL_CHECK("allocating a framebuffer");
}

void Framebuffer::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_.id());
}

void Framebuffer::bindDefault() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace Engine
