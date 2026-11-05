#include "render/Camera.h"

#include <cmath>

namespace Engine {

Vec3 Camera::forward() const {
    // Start from -Z, turn by yaw around +Y, tilt by pitch. Written out instead
    // of using a quaternion so the trigonometry is visible:
    //   yaw = 0, pitch = 0         -> ( 0, 0, -1)
    //   yaw = +90 degrees (left)   -> (-1, 0,  0)
    //   pitch = +90 degrees (up)   -> ( 0, 1,  0)
    const float cp = std::cos(pitch);
    return Vec3(-std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp);
}

Vec3 Camera::right() const {
    // Horizontal, so strafing never moves up or down even when looking at the sky.
    return Vec3(std::cos(yaw), 0.0f, -std::sin(yaw));
}

Vec3 Camera::up() const {
    return glm::cross(right(), forward());
}

Mat4 Camera::view() const {
    return Math::lookAt(position, position + forward(), Math::WorldUp);
}

Mat4 Camera::projection() const {
    return Math::perspective(fovY, aspect, nearPlane, farPlane);
}

void Camera::lookAt(const Vec3& target) {
    const Vec3 d = target - position;
    const float horizontal = std::sqrt(d.x * d.x + d.z * d.z);
    if (horizontal < Math::Epsilon && std::fabs(d.y) < Math::Epsilon) {
        return; // the target is where we are: no direction to face
    }
    // Inverse of forward(): forward.x = -sin(yaw)cos(pitch), forward.z = -cos(yaw)cos(pitch).
    yaw = std::atan2(-d.x, -d.z);
    pitch = std::atan2(d.y, horizontal);
    clampPitch();
}

void Camera::clampPitch() {
    pitch = Math::clamp(pitch, -MaxPitch, MaxPitch);
}

} // namespace Engine
