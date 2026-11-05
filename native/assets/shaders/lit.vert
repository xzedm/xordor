// Lit surface, vertex stage: runs once for every vertex of every mesh.
// Its job: place the vertex on screen, and pass along what the fragment
// stage needs (world position, normal, texture coordinate).
//
// OpenGL 4.1 core = GLSL 4.10. (Hot reload: edit and save this file while the
// sandbox runs; the renderer recompiles it within half a second.)
#version 410 core

// Vertex inputs. The numbers match the attribute setup in Mesh.cpp.
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;

uniform mat4 uModel;          // object -> world
uniform mat4 uViewProjection; // world -> clip (camera)
uniform mat3 uNormalMatrix;   // object -> world for normals (see Math::normalMatrix)
uniform vec2 uUvScale;        // > 1 repeats the texture (tiling)

// Outputs are interpolated across the triangle for each pixel.
out vec3 vWorldPosition;
out vec3 vNormal;
out vec2 vUv;

void main() {
    vec4 world = uModel * vec4(aPosition, 1.0); // w = 1: a point, so translation applies
    vWorldPosition = world.xyz;
    vNormal = uNormalMatrix * aNormal;
    vUv = aUv * uUvScale;
    gl_Position = uViewProjection * world;
}
