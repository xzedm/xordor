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
}
