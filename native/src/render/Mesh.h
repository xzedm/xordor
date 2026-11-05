#pragma once
// Mesh: triangles stored on the GPU, ready to draw.
//
// MeshData is the CPU-side description (a list of vertices and a list of
// indices); Mesh uploads it once and owns the GPU buffers.
//
// Indices: a cube has 8 corners but needs 24 vertices (each face has its own
// normal), and 12 triangles of 3 corners each. Instead of repeating vertices
// for every triangle, the index list says which vertices make each triangle:
// indices 0,1,2 then 0,2,3 make a square from 4 vertices instead of 6.
//
// Winding: triangles are listed counter-clockwise as seen from outside. The
// GPU uses that to tell front faces from back faces and skips the back ones
// ("back-face culling"), roughly halving the work for closed shapes.

#include "core/Math.h"
#include "render/GLObject.h"

#include <cstdint>
#include <vector>

namespace Engine {

// One corner of a triangle. The layout matters: Mesh.cpp tells OpenGL where
// each field is inside this struct (see the attribute setup there).
struct Vertex {
    Vec3 position;
    Vec3 normal; // the direction the surface faces, length 1; used for lighting
    Vec2 uv;     // where to read the texture: (0, 0) bottom-left, (1, 1) top-right
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices; // three per triangle, counter-clockwise from outside

    size_t triangleCount() const { return indices.size() / 3; }
};

class Mesh {
public:
    explicit Mesh(const MeshData& data);

    Mesh(Mesh&&) = default;
    Mesh& operator=(Mesh&&) = default;

    // Issues one draw call for all triangles. The caller has set up the shader.
    void draw() const;

    GLsizei indexCount() const { return indexCount_; }
    size_t triangleCount() const { return static_cast<size_t>(indexCount_) / 3; }

private:
    // A vertex array object (VAO) remembers the buffer bindings and the vertex
    // layout, so drawing is "bind the VAO, draw" instead of re-describing the
    // layout every frame.
    GLVertexArray vao_;
    GLBuffer vertexBuffer_;
    GLBuffer indexBuffer_;
    GLsizei indexCount_ = 0;
};

} // namespace Engine
