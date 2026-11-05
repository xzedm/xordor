#pragma once
// Camera: where we look from, and how the 3D world is flattened onto the screen.
//
// Two matrices do all the work:
//
//   view        world space -> camera space. Moves the world so the camera sits
//               at the origin looking down -Z. (We never move the camera; we
//               move everything else the opposite way.)
//   projection  camera space -> clip space. Perspective: things further away
//               get smaller. Only what ends up inside the -1..1 cube is drawn.
//
// The vertex shader computes  clip = projection * view * model * position.
//
// Orientation is stored as yaw (turning left/right around world up) and
// pitch (looking up/down), in radians. With no roll, two angles are easier to
// reason about and to clamp than a quaternion, and they can't drift.

#include "core/Math.h"

namespace Engine {

// What the renderer needs to know about a camera for one frame. Anything that
// can produce these three things can be a camera: the free-flying Camera
// below, or a camera entity in a scene (scene/SceneRenderer.cpp).
struct CameraView {
    Mat4 view{1.0f};
    Mat4 projection{1.0f};
    Vec3 position{0.0f};
    Mat4 viewProjection() const { return projection * view; }
};

class Camera {
public:
    Vec3 position{0.0f, 1.5f, 5.0f};
    float yaw = 0.0f;   // 0 looks down -Z; positive turns left (counter-clockwise seen from above)
    float pitch = 0.0f; // positive looks up; kept inside +-89 degrees
    float fovY = Math::toRadians(60.0f); // vertical field of view
    float nearPlane = 0.05f;
    float farPlane = 500.0f;
    float aspect = 16.0f / 9.0f; // width / height of the viewport

    Vec3 forward() const;
    Vec3 right() const;
    Vec3 up() const;

    Mat4 view() const;
    Mat4 projection() const;
    Mat4 viewProjection() const { return projection() * view(); }
    CameraView toView() const { return {view(), projection(), position}; }

    // The same orientation as a quaternion: yaw about world up, then pitch
    // about the turned right axis. rotation() * WorldForward == forward().
    // Lets a scene entity copy this camera's orientation.
    Quat rotation() const;

    // Turns the camera to face a point.
    void lookAt(const Vec3& target);
    // Keeps pitch away from straight up/down, where yaw stops meaning anything
    // ("gimbal lock") and lookAt's up vector would be parallel to the view.
    void clampPitch();

    static constexpr float MaxPitch = 89.0f * (Math::Pi / 180.0f);
};

} // namespace Engine
