#include "core/Math.h"

#include <doctest/doctest.h>

#include <cmath>

using namespace Engine;

TEST_SUITE("math") {

TEST_CASE("nearlyEqual tolerates rounding but not real differences") {
    CHECK(Math::nearlyEqual(0.1f + 0.2f, 0.3f));
    CHECK_FALSE(Math::nearlyEqual(0.3f, 0.31f));
    // Relative tolerance for large numbers.
    CHECK(Math::nearlyEqual(100000.0f, 100000.01f));
    CHECK(Math::nearlyEqual(Vec3(1.0f, 2.0f, 3.0f), Vec3(1.0f, 2.0f, 3.000001f)));
}

TEST_CASE("degree and radian conversion round-trips") {
    CHECK(Math::nearlyEqual(Math::toRadians(180.0f), Math::Pi));
    CHECK(Math::nearlyEqual(Math::toDegrees(Math::HalfPi), 90.0f));
    CHECK(Math::nearlyEqual(Math::toDegrees(Math::toRadians(37.5f)), 37.5f));
}

TEST_CASE("lerp and clamp") {
    CHECK(Math::lerp(2.0f, 4.0f, 0.5f) == doctest::Approx(3.0f));
    CHECK(Math::nearlyEqual(Math::lerp(Vec3(0.0f), Vec3(10.0f, 0.0f, -10.0f), 0.25f), Vec3(2.5f, 0.0f, -2.5f)));
    CHECK(Math::clamp(5, 0, 3) == 3);
    CHECK(Math::clamp(-1.0f, 0.0f, 1.0f) == 0.0f);
}

TEST_CASE("yaw turns the forward direction around the up axis") {
    // Facing -Z, turning left by 90 degrees (positive yaw, counter-clockwise
    // seen from above in a right-handed system) faces -X.
    const Quat yaw90 = Math::quatFromEulerDegrees(Vec3(0.0f, 90.0f, 0.0f));
    CHECK(Math::nearlyEqual(yaw90 * Math::WorldForward, Vec3(-1.0f, 0.0f, 0.0f)));
    // Pitch up by 90 degrees makes forward point up.
    const Quat pitch90 = Math::quatFromEulerDegrees(Vec3(90.0f, 0.0f, 0.0f));
    CHECK(Math::nearlyEqual(pitch90 * Math::WorldForward, Math::WorldUp));
}

TEST_CASE("Euler order is yaw first, then pitch about the turned right axis") {
    // Turn left 90 degrees (now facing -X), then look up 45 degrees.
    const Quat q = Math::quatFromEulerDegrees(Vec3(45.0f, 90.0f, 0.0f));
    const float c = std::sqrt(0.5f);
    CHECK(Math::nearlyEqual(q * Math::WorldForward, Vec3(-c, c, 0.0f)));
}

TEST_CASE("composeTRS scales, then rotates, then translates") {
    const Mat4 m = Math::composeTRS(Vec3(10.0f, 0.0f, 0.0f), Math::quatFromEulerDegrees(Vec3(0.0f, 90.0f, 0.0f)),
                                    Vec3(2.0f));
    // (1,0,0) -> scale -> (2,0,0) -> yaw 90 -> (0,0,-2) -> move -> (10,0,-2)
    CHECK(Math::nearlyEqual(Math::transformPoint(m, Vec3(1.0f, 0.0f, 0.0f)), Vec3(10.0f, 0.0f, -2.0f)));
    // Directions ignore translation.
    CHECK(Math::nearlyEqual(Math::transformDirection(m, Vec3(1.0f, 0.0f, 0.0f)), Vec3(0.0f, 0.0f, -2.0f)));
}

TEST_CASE("identity TRS is the identity matrix") {
    CHECK(Math::nearlyEqual(Math::composeTRS(Vec3(0.0f), Quat(1.0f, 0.0f, 0.0f, 0.0f), Vec3(1.0f)), Mat4(1.0f)));
}

TEST_CASE("q and -q are the same rotation") {
    const Quat q = Math::quatFromEulerDegrees(Vec3(10.0f, 20.0f, 30.0f));
    CHECK(Math::nearlyEqual(q, -q));
    CHECK_FALSE(Math::nearlyEqual(q, Math::quatFromEulerDegrees(Vec3(10.0f, 25.0f, 30.0f))));
}

TEST_CASE("perspective maps the near plane to depth -1 and the far plane to +1") {
    const Mat4 p = Math::perspective(Math::toRadians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);
    const auto ndcDepth = [&](float distance) {
        const Vec4 clip = p * Vec4(0.0f, 0.0f, -distance, 1.0f); // in front of the camera is -Z
        return clip.z / clip.w;
    };
    CHECK(ndcDepth(0.1f) == doctest::Approx(-1.0f));
    CHECK(ndcDepth(100.0f) == doctest::Approx(1.0f));
}

TEST_CASE("lookAt puts the target straight ahead of the camera") {
    const Mat4 view = Math::lookAt(Vec3(0.0f, 2.0f, 5.0f), Vec3(0.0f, 2.0f, 0.0f));
    // In view space the camera sits at the origin looking down -Z.
    CHECK(Math::nearlyEqual(Math::transformPoint(view, Vec3(0.0f, 2.0f, 0.0f)), Vec3(0.0f, 0.0f, -5.0f)));
}

TEST_CASE("normalMatrix keeps normals perpendicular under non-uniform scale") {
    // A 45-degree slope in the XY plane: it runs along `tangent`, `normal` sticks out of it.
    const Vec3 tangent = glm::normalize(Vec3(1.0f, -1.0f, 0.0f));
    const Vec3 normal = glm::normalize(Vec3(1.0f, 1.0f, 0.0f));
    const Mat4 stretchX = Math::composeTRS(Vec3(0.0f), Quat(1, 0, 0, 0), Vec3(2.0f, 1.0f, 1.0f));

    // The surface itself (its tangent) is moved by the model matrix as usual.
    const Vec3 worldTangent = Math::transformDirection(stretchX, tangent);
    const Vec3 rightNormal = Math::normalMatrix(stretchX) * normal;
    const Vec3 wrongNormal = Mat3(stretchX) * normal;

    CHECK(glm::dot(rightNormal, worldTangent) == doctest::Approx(0.0f));
    // With the model matrix itself the "normal" is no longer at a right angle to the surface.
    CHECK(std::fabs(glm::dot(glm::normalize(wrongNormal), glm::normalize(worldTangent))) > 0.5f);
    // For rotation only, the normal matrix is just the rotation.
    const Mat4 turn = Math::composeTRS(Vec3(1, 2, 3), Math::quatFromEulerDegrees(Vec3(10, 70, -20)), Vec3(1.0f));
    CHECK(Math::nearlyEqual(Mat4(Math::normalMatrix(turn)), Mat4(Mat3(turn)), 1e-5f));
}

} // TEST_SUITE
