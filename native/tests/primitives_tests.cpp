// Tests for the meshes made in code. They check the CPU-side data only, so
// they need no window or GPU. The most important check is winding: if a
// triangle's corners are in the wrong order, back-face culling hides it and
// the shape gets holes, a bug that's easy to make and hard to spot.

#include "render/Primitives.h"

#include <doctest/doctest.h>

#include <cmath>

using namespace Engine;

namespace {

struct WindingReport {
    int counterClockwise = 0; // seen from the side the normals point to
    int clockwise = 0;
    int degenerate = 0;       // zero area (for example at a sphere's poles)
};

WindingReport checkWinding(const MeshData& mesh) {
    WindingReport report;
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const Vertex& a = mesh.vertices[mesh.indices[i]];
        const Vertex& b = mesh.vertices[mesh.indices[i + 1]];
        const Vertex& c = mesh.vertices[mesh.indices[i + 2]];
        // The cross product of two edges points out of the side from which
        // the corners a -> b -> c look counter-clockwise (right-hand rule).
        const Vec3 face = glm::cross(b.position - a.position, c.position - a.position);
        if (glm::length(face) < 1e-7f) {
            ++report.degenerate;
            continue;
        }
        const Vec3 vertexNormals = a.normal + b.normal + c.normal;
        if (glm::dot(face, vertexNormals) > 0.0f) {
            ++report.counterClockwise;
        } else {
            ++report.clockwise;
        }
    }
    return report;
}

bool allIndicesInRange(const MeshData& mesh) {
    for (std::uint32_t index : mesh.indices) {
        if (index >= mesh.vertices.size()) {
            return false;
        }
    }
    return true;
}

bool allNormalsUnitLength(const MeshData& mesh) {
    for (const Vertex& v : mesh.vertices) {
        if (!Math::nearlyEqual(glm::length(v.normal), 1.0f, 1e-4f)) {
            return false;
        }
    }
    return true;
}

bool allUvsInUnitSquare(const MeshData& mesh) {
    for (const Vertex& v : mesh.vertices) {
        if (v.uv.x < 0.0f || v.uv.x > 1.0f || v.uv.y < 0.0f || v.uv.y > 1.0f) {
            return false;
        }
    }
    return true;
}

} // namespace

TEST_SUITE("primitives") {
    TEST_CASE("cube: 6 faces of 4 vertices and 2 triangles each") {
        const MeshData cube = Primitives::cube();
        // 24 vertices, not 8: a corner touches 3 faces with 3 different
        // normals, so each face needs its own copy of the corner.
        CHECK(cube.vertices.size() == 24);
        CHECK(cube.indices.size() == 36);
        CHECK(cube.triangleCount() == 12);
        CHECK(allIndicesInRange(cube));
    }

    TEST_CASE("cube corners sit at half the size from the center") {
        const MeshData cube = Primitives::cube(2.0f);
        for (const Vertex& v : cube.vertices) {
            CHECK(std::fabs(v.position.x) == doctest::Approx(1.0f));
            CHECK(std::fabs(v.position.y) == doctest::Approx(1.0f));
            CHECK(std::fabs(v.position.z) == doctest::Approx(1.0f));
        }
    }

    TEST_CASE("cube normals point out of the cube") {
        const MeshData cube = Primitives::cube();
        CHECK(allNormalsUnitLength(cube));
        for (const Vertex& v : cube.vertices) {
            // The center is the origin, so "outward" means the same side as the position.
            CHECK(glm::dot(v.normal, v.position) > 0.0f);
        }
    }

    TEST_CASE("plane lies flat at y = 0 and faces up") {
        const MeshData plane = Primitives::plane(4.0f);
        REQUIRE(plane.vertices.size() == 4);
        CHECK(plane.triangleCount() == 2);
        for (const Vertex& v : plane.vertices) {
            CHECK(v.position.y == 0.0f);
            CHECK(Math::nearlyEqual(v.normal, Math::WorldUp));
            CHECK(std::fabs(v.position.x) == doctest::Approx(2.0f));
        }
    }

    TEST_CASE("sphere: vertex grid and triangle counts") {
        const int segments = 12;
        const int rings = 6;
        const MeshData sphere = Primitives::sphere(0.5f, segments, rings);
        CHECK(sphere.vertices.size() == static_cast<size_t>((segments + 1) * (rings + 1)));
        CHECK(sphere.triangleCount() == static_cast<size_t>(segments * rings * 2));
        CHECK(allIndicesInRange(sphere));
    }

    TEST_CASE("sphere: every vertex is on the surface, with the normal pointing straight out") {
        const float radius = 2.5f;
        const MeshData sphere = Primitives::sphere(radius, 16, 8);
        CHECK(allNormalsUnitLength(sphere));
        for (const Vertex& v : sphere.vertices) {
            CHECK(glm::length(v.position) == doctest::Approx(radius).epsilon(1e-4));
            CHECK(Math::nearlyEqual(v.normal, v.position / radius, 1e-4f));
        }
    }

    TEST_CASE("triangles are counter-clockwise seen from outside") {
        SUBCASE("cube") {
            const WindingReport r = checkWinding(Primitives::cube());
            CHECK(r.counterClockwise == 12);
            CHECK(r.clockwise == 0);
        }
        SUBCASE("plane") {
            const WindingReport r = checkWinding(Primitives::plane());
            CHECK(r.counterClockwise == 2);
            CHECK(r.clockwise == 0);
        }
        SUBCASE("sphere") {
            const int segments = 24;
            const WindingReport r = checkWinding(Primitives::sphere(0.5f, segments, 12));
            CHECK(r.clockwise == 0);
            // Each grid cell touching a pole has one zero-area triangle.
            CHECK(r.degenerate == 2 * segments);
        }
    }

    TEST_CASE("texture coordinates stay inside the image") {
        CHECK(allUvsInUnitSquare(Primitives::cube()));
        CHECK(allUvsInUnitSquare(Primitives::plane()));
        CHECK(allUvsInUnitSquare(Primitives::sphere()));
    }
}
