#include "render/FlyCameraController.h"

#include <cmath>

namespace Engine {

void FlyCameraController::update(Camera& camera, const Input& input, float frameSeconds) {
    flying_ = input.isMouseDown(MouseButton::Right);
    if (!flying_) {
        return;
    }

    // Look: moving the mouse right turns right, which is negative yaw (see Camera.h).
    // Moving it down (positive y in window coordinates) looks down.
    const Vec2 delta = input.mouseDelta();
    camera.yaw -= delta.x * lookSensitivity;
    camera.pitch -= delta.y * lookSensitivity;
    camera.clampPitch();
    // Keep yaw in -pi..pi so it doesn't grow without bound over a long session.
    camera.yaw = std::remainder(camera.yaw, Math::TwoPi);

    // Scrolling multiplies the speed, so each notch feels the same whether
    // creeping at 1 m/s or racing at 100 m/s.
    if (input.scrollDelta() != 0.0f) {
        speed = Math::clamp(speed * std::pow(1.2f, input.scrollDelta()), minSpeed, maxSpeed);
    }

    Vec3 move(0.0f);
    if (input.isKeyDown(Key::W)) move += camera.forward();
    if (input.isKeyDown(Key::S)) move -= camera.forward();
    if (input.isKeyDown(Key::D)) move += camera.right();
    if (input.isKeyDown(Key::A)) move -= camera.right();
    if (input.isKeyDown(Key::E)) move += Math::WorldUp;
    if (input.isKeyDown(Key::Q)) move -= Math::WorldUp;
    // Normalize so diagonal movement isn't faster than straight movement.
    if (glm::dot(move, move) > 0.0f) {
        const bool fast = input.isKeyDown(Key::LeftShift) || input.isKeyDown(Key::RightShift);
        camera.position += glm::normalize(move) * speed * (fast ? 4.0f : 1.0f) * frameSeconds;
    }
}

} // namespace Engine
