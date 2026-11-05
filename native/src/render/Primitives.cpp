#include "render/Primitives.h"

#include "core/Assert.h"

#include <cmath>

namespace Engine::Primitives {
namespace {

// Adds one square face. `normal` points out of the face; `right` and `up` lie
// in it. The corners are listed so that, seen from the outside (looking
// against the normal), they go counter-clockwise: bottom-left, bottom-right,
// top-right, top-left.
void addQuad(MeshData& mesh, const Vec3& center, const Vec3& normal, const Vec3& right, const Vec3& up) {
    const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({center - right - up, normal, {0.0f, 0.0f}});
    mesh.vertices.push_back({center + right - up, normal, {1.0f, 0.0f}});
    mesh.vertices.push_back({center + right + up, normal, {1.0f, 1.0f}});
    mesh.vertices.push_back({center - right + up, normal, {0.0f, 1.0f}});
    // Two triangles: (bl, br, tr) and (bl, tr, tl), both counter-clockwise.
    mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

} // namespace

MeshData cube(float size) {
    const float h = size * 0.5f;
    MeshData mesh;
    mesh.vertices.reserve(24);
    mesh.indices.reserve(36);
    //      normal              right              up        (right x up = normal)
    addQuad(mesh, {0, 0, h}, {0, 0, 1}, {h, 0, 0}, {0, h, 0});    // front  (+Z)
    addQuad(mesh, {0, 0, -h}, {0, 0, -1}, {-h, 0, 0}, {0, h, 0}); // back   (-Z)
    addQuad(mesh, {h, 0, 0}, {1, 0, 0}, {0, 0, -h}, {0, h, 0});   // right  (+X)
    addQuad(mesh, {-h, 0, 0}, {-1, 0, 0}, {0, 0, h}, {0, h, 0});  // left   (-X)
    addQuad(mesh, {0, h, 0}, {0, 1, 0}, {h, 0, 0}, {0, 0, -h});   // top    (+Y)
    addQuad(mesh, {0, -h, 0}, {0, -1, 0}, {h, 0, 0}, {0, 0, h});  // bottom (-Y)
    return mesh;
}

MeshData plane(float size) {
    const float h = size * 0.5f;
    MeshData mesh;
    // Facing up: right is +X, "up" in the face is -Z (away from the viewer).
    addQuad(mesh, {0, 0, 0}, {0, 1, 0}, {h, 0, 0}, {0, 0, -h});
    return mesh;
}

MeshData sphere(float radius, int segments, int rings) {
    ENGINE_ASSERT(segments >= 3 && rings >= 2, "a sphere needs at least 3 segments and 2 rings");
    MeshData mesh;
    // A grid of (segments + 1) x (rings + 1) vertices wrapped around the sphere.
    // The first and last column are in the same place but have different u
    // (0 and 1), so the texture can wrap without a smeared seam.
    for (int ring = 0; ring <= rings; ++ring) {
        const float v = static_cast<float>(ring) / static_cast<float>(rings); // 0 = bottom pole, 1 = top pole
        const float latitude = (v - 0.5f) * Math::Pi;                         // -90..90 degrees
        for (int segment = 0; segment <= segments; ++segment) {
            const float u = static_cast<float>(segment) / static_cast<float>(segments);
            const float longitude = u * Math::TwoPi;
            // On a unit sphere the normal *is* the position, which is why spheres light so smoothly.
            const Vec3 n(std::cos(latitude) * std::sin(longitude), std::sin(latitude),
                         std::cos(latitude) * std::cos(longitude));
            mesh.vertices.push_back({n * radius, n, {u, v}});
        }
    }
    const auto columns = static_cast<std::uint32_t>(segments + 1);
    for (std::uint32_t ring = 0; ring < static_cast<std::uint32_t>(rings); ++ring) {
        for (std::uint32_t segment = 0; segment < static_cast<std::uint32_t>(segments); ++segment) {
            const std::uint32_t a = ring * columns + segment; // bottom-left of this grid cell
            const std::uint32_t b = a + 1;                    // bottom-right
            const std::uint32_t c = a + columns;              // top-left
            const std::uint32_t d = c + 1;                    // top-right
            // Longitude grows toward +X from +Z, which is "rightward" seen from
            // outside, so (a, b, d) and (a, d, c) are counter-clockwise.
            // The cells touching the poles have one zero-area triangle each;
            // harmless, and the GPU discards it.
            mesh.indices.insert(mesh.indices.end(), {a, b, d, a, d, c});
        }
    }
    return mesh;
}

} // namespace Engine::Primitives
