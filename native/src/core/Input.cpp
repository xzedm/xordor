#include "core/Input.h"

namespace Engine {

bool Input::validKey(Key key) {
    const int code = static_cast<int>(key);
    return code >= 0 && code < KeyCount;
}

bool Input::validButton(MouseButton button) {
    const int code = static_cast<int>(button);
    return code >= 0 && code < MouseButtonCount;
}

bool Input::isKeyDown(Key key) const {
    return validKey(key) && keysDown_.test(static_cast<size_t>(key));
}

bool Input::wasKeyPressed(Key key) const {
    return validKey(key) && keysPressed_.test(static_cast<size_t>(key));
}

bool Input::wasKeyReleased(Key key) const {
    return validKey(key) && keysReleased_.test(static_cast<size_t>(key));
}

bool Input::isMouseDown(MouseButton button) const {
    return validButton(button) && buttonsDown_.test(static_cast<size_t>(button));
}

bool Input::wasMousePressed(MouseButton button) const {
    return validButton(button) && buttonsPressed_.test(static_cast<size_t>(button));
}

bool Input::wasMouseReleased(MouseButton button) const {
    return validButton(button) && buttonsReleased_.test(static_cast<size_t>(button));
}

void Input::onKey(Key key, bool down) {
    // Keys we don't know (media keys, Key::Unknown) are ignored, not an error:
    // the OS sends them and the game simply has no use for them.
    if (!validKey(key)) {
        return;
    }
    const auto i = static_cast<size_t>(key);
    if (down && !keysDown_.test(i)) {
        keysPressed_.set(i);
    }
    if (!down && keysDown_.test(i)) {
        keysReleased_.set(i);
    }
    keysDown_.set(i, down);
}

void Input::onMouseButton(MouseButton button, bool down) {
    if (!validButton(button)) {
        return;
    }
    const auto i = static_cast<size_t>(button);
    if (down && !buttonsDown_.test(i)) {
        buttonsPressed_.set(i);
    }
    if (!down && buttonsDown_.test(i)) {
        buttonsReleased_.set(i);
    }
    buttonsDown_.set(i, down);
}

void Input::onMouseMove(Vec2 position) {
    if (hasMousePosition_) {
        // Several move events can arrive in one frame; add them all up.
        mouseDelta_ += position - mousePosition_;
    }
    mousePosition_ = position;
    hasMousePosition_ = true;
}

void Input::onScroll(float amount) {
    scrollDelta_ += amount;
}

void Input::releaseAll() {
    // Report every held key as released, so code waiting for a release still sees it.
    keysReleased_ |= keysDown_;
    buttonsReleased_ |= buttonsDown_;
    keysDown_.reset();
    buttonsDown_.reset();
}

void Input::endFrame() {
    keysPressed_.reset();
    keysReleased_.reset();
    buttonsPressed_.reset();
    buttonsReleased_.reset();
    mouseDelta_ = Vec2(0.0f);
    scrollDelta_ = 0.0f;
}

} // namespace Engine
