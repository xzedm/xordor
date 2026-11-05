// Tests for the camera: its direction vectors and its matrices. Getting a sign
// wrong here (yaw turning the "wrong" way, or a mirrored view) is a classic
// bug, so the conventions in Camera.h are checked one by one.

#include "render/Camera.h"

#include <doctest/doctest.h>

#include <cmath>
#include <initializer_list>

using namespace Engine;

namespace {

// Where a world point lands in normalized device coordinates (-1..1 on every axis).
Vec3 toNdc(const Camera& camera, const Vec3& world) {
    const Vec4 clip = camera.viewProjection() * Vec4(world, 1.0f);
    return Vec3(clip) / clip.w; // the "perspective divide" the GPU does after the vertex shader
}

} // namespace

TEST_SUITE("camera") {
    TEST_CASE("with no rotation it looks down -Z with +X to the right and +Y up") {
        Camera camera;
        CHECK(Math::nearlyEqual(camera.forward(), Math::WorldForward));
        CHECK(Math::nearlyEqual(camera.right(), Math::WorldRight));
        CHECK(Math::nearlyEqual(camera.up(), Math::WorldUp));
    }

    TEST_CASE("positive yaw turns left, positive pitch looks up") {
        Camera camera;
        camera.yaw = Math::HalfPi;
        CHECK(Math::nearlyEqual(camera.forward(), Vec3(-1.0f, 0.0f, 0.0f)));
        camera.yaw = 0.0f;
        camera.pitch = Math::toRadians(45.0f);
        CHECK(camera.forward().y == doctest::Approx(std::sqrt(0.5f)));
        CHECK(camera.forward().z < 0.0f);
    }

    TEST_CASE("forward, right and up are unit length and at right angles") {
        Camera camera;
        for (float yaw : {-2.5f, -0.3f, 0.0f, 1.0f, 3.0f}) {
            for (float pitch : {-1.2f, 0.0f, 0.7f, 1.5f}) {
                camera.yaw = yaw;
                camera.pitch = pitch;
                const Vec3 f = camera.forward();
                const Vec3 r = camera.right();
                const Vec3 u = camera.up();
                CHECK(glm::length(f) == doctest::Approx(1.0f));
                CHECK(glm::length(r) == doctest::Approx(1.0f));
                CHECK(glm::length(u) == doctest::Approx(1.0f));
                CHECK(glm::dot(f, r) == doctest::Approx(0.0f));
                CHECK(glm::dot(f, u) == doctest::Approx(0.0f));
                CHECK(r.y == doctest::Approx(0.0f)); // no roll: right stays level
            }
        }
    }

    TEST_CASE("lookAt turns the camera toward the target") {
        Camera camera;
        camera.position = Vec3(1.0f, 2.0f, 3.0f);
        const Vec3 target(4.0f, 0.0f, -5.0f);
        camera.lookAt(target);
        CHECK(Math::nearlyEqual(camera.forward(), glm::normalize(target - camera.position), 1e-4f));
    }

    TEST_CASE("lookAt a target at the camera's own position changes nothing") {
        Camera camera;
        camera.yaw = 0.4f;
        camera.pitch = 0.2f;
        camera.lookAt(camera.position);
        CHECK(camera.yaw == 0.4f);
        CHECK(camera.pitch == 0.2f);
    }

    TEST_CASE("pitch stops short of straight up and down") {
        // At exactly 90 degrees, forward and world-up are parallel and the view
        // matrix can't tell which way is "right": the image would spin.
        Camera camera;
        camera.pitch = 3.0f;
        camera.clampPitch();
        CHECK(camera.pitch == doctest::Approx(Camera::MaxPitch));
        camera.lookAt(camera.position + Vec3(0.0f, -10.0f, 0.0f));
        CHECK(camera.pitch == doctest::Approx(-Camera::MaxPitch));
    }

    TEST_CASE("the view matrix moves the world so the camera sits at the origin") {
        Camera camera;
        camera.position = Vec3(5.0f, 1.0f, -2.0f);
        camera.yaw = Math::HalfPi; // looking down -X
        const Mat4 view = camera.view();
        CHECK(Math::nearlyEqual(Math::transformPoint(view, camera.position), Vec3(0.0f), 1e-4f));
        // Two meters in front of the camera ends up two meters down -Z in view space.
        const Vec3 ahead = camera.position + camera.forward() * 2.0f;
        CHECK(Math::nearlyEqual(Math::transformPoint(view, ahead), Vec3(0.0f, 0.0f, -2.0f), 1e-4f));
    }

    TEST_CASE("projection: the near plane maps to depth -1 and the far plane to +1") {
        Camera camera;
        camera.position = Vec3(0.0f);
        camera.nearPlane = 0.1f;
        camera.farPlane = 100.0f;
        CHECK(toNdc(camera, Vec3(0.0f, 0.0f, -0.1f)).z == doctest::Approx(-1.0f));
        CHECK(toNdc(camera, Vec3(0.0f, 0.0f, -100.0f)).z == doctest::Approx(1.0f));
    }

    TEST_CASE("projection: the field of view reaches the top edge of the screen") {
        Camera camera;
        camera.position = Vec3(0.0f);
        camera.fovY = Math::toRadians(90.0f); // 45 degrees above and below the center
        camera.aspect = 2.0f;
        // A point 45 degrees up (as far up as it is ahead) is exactly at the top edge...
        CHECK(toNdc(camera, Vec3(0.0f, 10.0f, -10.0f)).y == doctest::Approx(1.0f));
        // ...and the screen is twice as wide, so the side edge is twice as far out.
        CHECK(toNdc(camera, Vec3(20.0f, 0.0f, -10.0f)).x == doctest::Approx(1.0f));
        // Right in world space is right on screen (no mirroring).
        CHECK(toNdc(camera, Vec3(1.0f, 0.0f, -10.0f)).x > 0.0f);
    }
}
