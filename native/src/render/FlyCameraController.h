#pragma once
// FlyCameraController: moves a Camera like the Unreal Editor viewport.
//
//   hold right mouse   look around (the cursor is hidden while held)
//   W A S D            forward, left, back, right (only while right mouse is held)
//   Q / E              down / up
//   Shift              four times faster
//   scroll             change speed (while flying)
//
// It reads Input and writes Camera; it knows nothing about windows. The app
// asks wantsCursorCaptured() each frame and hides the cursor accordingly.

#include "core/Input.h"
#include "render/Camera.h"

namespace Engine {

class FlyCameraController {
public:
    float speed = 6.0f;                  // meters per second
    float lookSensitivity = 0.0025f;     // radians per pixel of mouse movement
    float minSpeed = 0.5f;
    float maxSpeed = 200.0f;

    // Call once per frame (in onUpdate: camera motion should follow real time).
    void update(Camera& camera, const Input& input, float frameSeconds);

    bool wantsCursorCaptured() const { return flying_; }

private:
    bool flying_ = false;
};

} // namespace Engine
