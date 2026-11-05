#include "render/Mesh.h"

#include "core/Assert.h"
#include "render/GLCheck.h"

#include <cstddef> // offsetof

namespace Engine {
namespace {

// Attribute numbers shared with the shaders ("layout(location = 0) in vec3 aPosition").
constexpr GLuint PositionAttribute = 0;
constexpr GLuint NormalAttribute = 1;
constexpr GLuint UvAttribute = 2;

GLuint createBuffer() {
    GLuint id = 0;
    glGenBuffers(1, &id);
    return id;
}

GLuint createVertexArray() {
    GLuint id = 0;
    glGenVertexArrays(1, &id);
    return id;
}

// Describes one field of Vertex to OpenGL: which attribute it feeds, how many
// floats it has, and where it starts inside the struct.
void describeAttribute(GLuint attribute, GLint floatCount, size_t offsetInVertex) {
    glEnableVertexAttribArray(attribute);
    glVertexAttribPointer(attribute, floatCount, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          // A byte offset passed as a pointer: an old OpenGL quirk.
                          reinterpret_cast<const void*>(offsetInVertex));
}

} // namespace

// GLM is built with GLM_FORCE_XYZW_ONLY and no padding, so Vertex is 8 tightly
// packed floats. If that ever changed, the offsets below would still be right
// (they come from offsetof), but the size check documents the assumption.
static_assert(sizeof(Vertex) == 8 * sizeof(float), "Vertex is expected to be 8 tightly packed floats");

Mesh::Mesh(const MeshData& data)
    : vao_(createVertexArray()), vertexBuffer_(createBuffer()), indexBuffer_(createBuffer()),
      indexCount_(static_cast<GLsizei>(data.indices.size())) {
    ENGINE_ASSERT(!data.vertices.empty() && data.indices.size() % 3 == 0, "a mesh needs vertices and whole triangles");

    // Everything below is recorded into the VAO while it is bound.
    glBindVertexArray(vao_.id());

    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_.id());
    // STATIC_DRAW: uploaded once, drawn many times. The driver may put it in
    // fast GPU memory. (Data changed every frame would use GL_DYNAMIC_DRAW.)
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.vertices.size() * sizeof(Vertex)),
                 data.vertices.data(), GL_STATIC_DRAW);

    describeAttribute(PositionAttribute, 3, offsetof(Vertex, position));
    describeAttribute(NormalAttribute, 3, offsetof(Vertex, normal));
    describeAttribute(UvAttribute, 2, offsetof(Vertex, uv));

    // The index buffer binding is part of the VAO's state (unlike
    // GL_ARRAY_BUFFER), so it must stay bound until the VAO is unbound.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer_.id());
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.indices.size() * sizeof(std::uint32_t)),
                 data.indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    XE_GL_CHECK("creating a mesh");
}

void Mesh::draw() const {
    glBindVertexArray(vao_.id());
    glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

} // namespace Engine
