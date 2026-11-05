// Tests for boxes, spheres and their overlap tests.

#include "core/Geometry.h"

#include <doctest/doctest.h>

#include <cmath>

using namespace Engine;

TEST_SUITE("geometry") {
    TEST_CASE("box from center and half extents") {
        const Aabb box = Aabb::fromCenter(Vec3(1.0f, 2.0f, 3.0f), Vec3(0.5f, 1.0f, 2.0f));
        CHECK(Math::nearlyEqual(box.min, Vec3(0.5f, 1.0f, 1.0f)));
        CHECK(Math::nearlyEqual(box.max, Vec3(1.5f, 3.0f, 5.0f)));
        CHECK(Math::nearlyEqual(box.center(), Vec3(1.0f, 2.0f, 3.0f)));
        CHECK(box.contains(Vec3(1.0f, 2.0f, 3.0f)));
        CHECK_FALSE(box.contains(Vec3(2.0f, 2.0f, 3.0f)));
    }

    TEST_CASE("boxes overlap only when they overlap on every axis") {
        const Aabb a{Vec3(0.0f), Vec3(1.0f)};
        CHECK(overlaps(a, Aabb{Vec3(0.5f), Vec3(2.0f)}));
        CHECK(overlaps(a, Aabb{Vec3(1.0f, 0.0f, 0.0f), Vec3(2.0f, 1.0f, 1.0f)})); // touching faces
        CHECK_FALSE(overlaps(a, Aabb{Vec3(1.1f, 0.0f, 0.0f), Vec3(2.0f, 1.0f, 1.0f)}));
        // Overlaps on x and y but not z: no overlap.
        CHECK_FALSE(overlaps(a, Aabb{Vec3(0.5f, 0.5f, 3.0f), Vec3(1.5f, 1.5f, 4.0f)}));
        CHECK(overlaps(a, Aabb{Vec3(0.2f), Vec3(0.3f)})); // fully inside
    }

    TEST_CASE("spheres overlap when the centers are closer than the radii add up") {
        const Sphere a{Vec3(0.0f), 1.0f};
        CHECK(overlaps(a, Sphere{Vec3(1.5f, 0.0f, 0.0f), 0.6f}));
        CHECK(overlaps(a, Sphere{Vec3(2.0f, 0.0f, 0.0f), 1.0f})); // touching
        CHECK_FALSE(overlaps(a, Sphere{Vec3(2.0f, 0.1f, 0.0f), 1.0f}));
    }

    TEST_CASE("sphere against box uses the box's nearest point") {
        const Aabb box{Vec3(0.0f), Vec3(1.0f)};
        CHECK(Math::nearlyEqual(closestPoint(box, Vec3(2.0f, 0.5f, -1.0f)), Vec3(1.0f, 0.5f, 0.0f)));
        CHECK(Math::nearlyEqual(closestPoint(box, Vec3(0.5f)), Vec3(0.5f))); // inside: the point itself
        CHECK(overlaps(Sphere{Vec3(1.5f, 0.5f, 0.5f), 0.6f}, box));
        // Near a corner, the box's bounding range overlaps but the sphere misses:
        // distance from (1.5,1.5,1.5) to corner (1,1,1) is 0.87 > 0.8.
        CHECK_FALSE(overlaps(Sphere{Vec3(1.5f), 0.8f}, box));
        CHECK(overlaps(box, Sphere{Vec3(0.5f), 0.1f})); // either argument order
    }

    TEST_CASE("transformAabb moves, scales and grows a box to fit its rotation") {
        const Aabb unit = Aabb::fromCenter(Vec3(0.0f), Vec3(0.5f));
        const Aabb moved = transformAabb(unit, Math::composeTRS(Vec3(3.0f, 0.0f, 0.0f), Quat(1, 0, 0, 0), Vec3(2.0f)));
        CHECK(Math::nearlyEqual(moved.min, Vec3(2.0f, -1.0f, -1.0f)));
        CHECK(Math::nearlyEqual(moved.max, Vec3(4.0f, 1.0f, 1.0f)));

        const Mat4 turned = Math::composeTRS(Vec3(0.0f), glm::angleAxis(Math::Pi / 4.0f, Math::WorldUp), Vec3(1.0f));
        const Aabb rotated = transformAabb(unit, turned);
        CHECK(rotated.halfExtents().x == doctest::Approx(std::sqrt(0.5f)));
        CHECK(rotated.halfExtents().z == doctest::Approx(std::sqrt(0.5f)));
        CHECK(rotated.halfExtents().y == doctest::Approx(0.5f));
    }

    TEST_CASE("a ray through the screen center goes straight down the view") {
        const Mat4 proj = Math::perspective(Math::toRadians(60.0f), 1.0f, 0.1f, 100.0f);
        const Mat4 view = Math::lookAt(Vec3(0.0f, 0.0f, 5.0f), Vec3(0.0f));
        const Ray center = rayFromScreen(Vec2(0.0f), glm::inverse(proj * view));
        CHECK(Math::nearlyEqual(center.direction, Vec3(0.0f, 0.0f, -1.0f), 1e-4f));
        CHECK(center.origin.z == doctest::Approx(4.9f).epsilon(1e-3)); // starts on the near plane
        // The top edge of the screen is 30 degrees up (half the field of view).
        const Ray top = rayFromScreen(Vec2(0.0f, 1.0f), glm::inverse(proj * view));
        CHECK(std::asin(top.direction.y) == doctest::Approx(Math::toRadians(30.0f)).epsilon(1e-3));
    }

    TEST_CASE("ray against box: hit distance, miss, and starting inside") {
        const Aabb box{Vec3(-1.0f), Vec3(1.0f)};
        const auto hit = intersect(Ray{Vec3(0.0f, 0.0f, 5.0f), Vec3(0.0f, 0.0f, -1.0f)}, box);
        REQUIRE(hit);
        CHECK(*hit == doctest::Approx(4.0f));
        CHECK_FALSE(intersect(Ray{Vec3(3.0f, 0.0f, 5.0f), Vec3(0.0f, 0.0f, -1.0f)}, box)); // passes beside
        CHECK_FALSE(intersect(Ray{Vec3(0.0f, 0.0f, 5.0f), Vec3(0.0f, 0.0f, 1.0f)}, box));  // pointing away
        CHECK(*intersect(Ray{Vec3(0.0f), Vec3(1.0f, 0.0f, 0.0f)}, box) == 0.0f);           // inside
        const auto diagonal = intersect(Ray{Vec3(-5.0f, -5.0f, 0.0f), glm::normalize(Vec3(1.0f, 1.0f, 0.0f))}, box);
        REQUIRE(diagonal);
        CHECK(*diagonal == doctest::Approx(4.0f * std::sqrt(2.0f)));
    }

    TEST_CASE("ray against sphere and plane") {
        const Sphere s{Vec3(0.0f, 0.0f, -10.0f), 2.0f};
        CHECK(*intersect(Ray{Vec3(0.0f), Vec3(0.0f, 0.0f, -1.0f)}, s) == doctest::Approx(8.0f));
        CHECK_FALSE(intersect(Ray{Vec3(0.0f), Vec3(0.0f, 1.0f, 0.0f)}, s));
        CHECK(*intersectPlane(Ray{Vec3(0.0f, 5.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)}, Vec3(0.0f), Math::WorldUp) ==
              doctest::Approx(5.0f));
        CHECK_FALSE(intersectPlane(Ray{Vec3(0.0f, 5.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f)}, Vec3(0.0f), Math::WorldUp));
    }

    TEST_CASE("closest point on an axis to a ray (gizmo dragging)") {
        // A ray coming down from above at x = 3 passes closest to the X axis at x = 3.
        const auto s = closestOnLine(Ray{Vec3(3.0f, 5.0f, 1.0f), Vec3(0.0f, -1.0f, 0.0f)}, Vec3(0.0f), Vec3(1.0f, 0.0f, 0.0f));
        REQUIRE(s);
        CHECK(*s == doctest::Approx(3.0f));
        // A longer axis direction scales the parameter: 2 units of a length-1.5 axis.
        CHECK(*closestOnLine(Ray{Vec3(3.0f, 5.0f, 1.0f), Vec3(0.0f, -1.0f, 0.0f)}, Vec3(0.0f), Vec3(1.5f, 0.0f, 0.0f)) ==
              doctest::Approx(2.0f));
        // Parallel: no single answer.
        CHECK_FALSE(closestOnLine(Ray{Vec3(0.0f, 1.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f)}, Vec3(0.0f), Vec3(1.0f, 0.0f, 0.0f)));
    }

    TEST_CASE("signed angle around an axis follows the right-hand rule") {
        const Vec3 x(1.0f, 0.0f, 0.0f), z(0.0f, 0.0f, 1.0f);
        // Around +Y, turning +X by +90 degrees gives -Z (as glm::angleAxis does).
        CHECK(signedAngle(x, -z, Math::WorldUp) == doctest::Approx(Math::HalfPi));
        CHECK(signedAngle(x, z, Math::WorldUp) == doctest::Approx(-Math::HalfPi));
        CHECK(Math::nearlyEqual(glm::angleAxis(Math::HalfPi, Math::WorldUp) * x, -z, 1e-5f));
    }
}
