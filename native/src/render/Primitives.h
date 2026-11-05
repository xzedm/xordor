#pragma once
// Built-in shapes, generated as MeshData (CPU only, so they are unit tested).
// All are centered on the origin with outward normals and counter-clockwise
// triangles seen from outside.

#include "render/Mesh.h"

namespace Engine::Primitives {

// A cube with sides of `size`. Each face has its own 4 vertices so it can have
// its own flat normal and a full 0..1 texture.
MeshData cube(float size = 1.0f);

// A flat square on the XZ plane facing up (+Y), `size` wide.
MeshData plane(float size = 1.0f);

// A "UV sphere": rings of latitude and segments of longitude, like a globe.
// More of each gives a rounder sphere and more triangles.
MeshData sphere(float radius = 0.5f, int segments = 32, int rings = 16);

} // namespace Engine::Primitives
