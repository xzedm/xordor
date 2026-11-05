#include "core/Math.h"

#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace Engine::Math {

bool nearlyEqual(float a, float b, float epsilon) {
    // Absolute tolerance for numbers near zero, relative tolerance for big
    // numbers (1000.0001 and 1000.0002 differ by more than 1e-5 but are equal
    // for any practical purpose).
    const float diff = std::fabs(a - b);
    if (diff <= epsilon) {
        return true;
    }
    return diff <= epsilon * std::fmax(std::fabs(a), std::fabs(b));
}

bool nearlyEqual(const Vec3& a, const Vec3& b, float epsilon) {
    return nearlyEqual(a.x, b.x, epsilon) && nearlyEqual(a.y, b.y, epsilon) && nearlyEqual(a.z, b.z, epsilon);
}

bool nearlyEqual(const Mat4& a, const Mat4& b, float epsilon) {
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            if (!nearlyEqual(a[column][row], b[column][row], epsilon)) {
                return false;
            }
        }
    }
    return true;
}

bool nearlyEqual(const Quat& a, const Quat& b, float epsilon) {
    // |dot| close to 1 means the same rotation, whatever the sign.
    return std::fabs(std::fabs(glm::dot(a, b)) - 1.0f) <= epsilon;
}

Quat quatFromEulerDegrees(const Vec3& pitchYawRollDegrees) {
    const Quat yaw = glm::angleAxis(toRadians(pitchYawRollDegrees.y), WorldUp);
    const Quat pitch = glm::angleAxis(toRadians(pitchYawRollDegrees.x), WorldRight);
    const Quat roll = glm::angleAxis(toRadians(pitchYawRollDegrees.z), Vec3(0.0f, 0.0f, 1.0f));
    // Two ways to read the same product:
    //  * right to left, about the fixed world axes: roll, then pitch, then yaw;
    //  * left to right, about the object's own axes: yaw, then pitch about the
    //    already-turned right axis, then roll.
    // The second reading is how a person turns around and then looks up.
    return yaw * pitch * roll;
}

Mat4 composeTRS(const Vec3& translation, const Quat& rotation, const Vec3& scale) {
    const Mat4 t = glm::translate(Mat4(1.0f), translation);
    const Mat4 r = glm::mat4_cast(rotation);
    const Mat4 s = glm::scale(Mat4(1.0f), scale);
    // TODO(performance): build the matrix directly from the quaternion columns
    // times scale; three full 4x4 multiplies is clearer for now.
    return t * r * s;
}

Vec3 transformPoint(const Mat4& m, const Vec3& point) {
    return Vec3(m * Vec4(point, 1.0f));
}

Vec3 transformDirection(const Mat4& m, const Vec3& direction) {
    return Vec3(m * Vec4(direction, 0.0f));
}

Mat3 normalMatrix(const Mat4& model) {
    // TODO(performance): computed per draw on the CPU; cache it per object once
    // transforms live in the scene graph (Phase 3) and only change when moved.
    return glm::transpose(glm::inverse(Mat3(model)));
}

Mat4 perspective(float fovYRadians, float aspect, float zNear, float zFar) {
    // GLM's default clip space is OpenGL's (-1..1 depth, right-handed), which is
    // what we want. Vulkan and DirectX would need the zero-to-one variant.
    return glm::perspective(fovYRadians, aspect, zNear, zFar);
}

Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
    return glm::lookAt(eye, target, up);
}

} // namespace Engine::Math
