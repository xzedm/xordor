#pragma once
// Input: what the keyboard and mouse are doing right now, and what changed this frame.
//
// Three questions games ask:
//   isKeyDown(Key::W)        "is it held?"            -> keep walking
//   wasKeyPressed(Key::Space) "did it go down this frame?" -> jump once
//   wasKeyReleased(Key::E)   "did it come up this frame?"  -> stop charging
//
// The window layer feeds events in (onKey, onMouseMove, ...). At the very end
// of every frame Application calls endFrame(), which forgets this frame's
// presses, releases and mouse movement but keeps what is still held.
//
// Pressed/released are recorded from the events themselves rather than by
// comparing this frame's state with the last one, so a key tapped and let go
// within a single frame still counts as "pressed" (comparing states would miss it).

#include "core/KeyCodes.h"
#include "core/Math.h"

#include <bitset>

namespace Engine {

class Input {
public:
    bool isKeyDown(Key key) const;
    bool wasKeyPressed(Key key) const;
    bool wasKeyReleased(Key key) const;

    bool isMouseDown(MouseButton button) const;
    bool wasMousePressed(MouseButton button) const;
    bool wasMouseReleased(MouseButton button) const;

    // In window pixels, origin at the top-left (the OS convention).
    Vec2 mousePosition() const { return mousePosition_; }
    // How far the mouse moved this frame, in pixels.
    Vec2 mouseDelta() const { return mouseDelta_; }
    // Scroll wheel movement this frame (positive is away from the user).
    float scrollDelta() const { return scrollDelta_; }

    // ---- called by the window layer when OS events arrive ----
    void onKey(Key key, bool down);
    void onMouseButton(MouseButton button, bool down);
    void onMouseMove(Vec2 position);
    void onScroll(float amount);
    // The window lost focus: release everything, or keys would stay "stuck"
    // down because their release event goes to another program.
    void releaseAll();

    // Call once at the end of every frame.
    void endFrame();

private:
    static bool validKey(Key key);
    static bool validButton(MouseButton button);

    std::bitset<KeyCount> keysDown_;
    std::bitset<KeyCount> keysPressed_;
    std::bitset<KeyCount> keysReleased_;
    std::bitset<MouseButtonCount> buttonsDown_;
    std::bitset<MouseButtonCount> buttonsPressed_;
    std::bitset<MouseButtonCount> buttonsReleased_;

    Vec2 mousePosition_{0.0f};
    Vec2 mouseDelta_{0.0f};
    float scrollDelta_ = 0.0f;
    // The first move event has no previous position to measure from.
    bool hasMousePosition_ = false;
};

} // namespace Engine
