#pragma once
// Turning model files into MeshData (positions, normals, uvs, triangles).
//
//   .obj         read by our own code (ObjImport.cpp). OBJ is plain text, one
//                line per vertex or face, so the whole reader fits on a few
//                screens and shows exactly what a mesh file contains.
//   .gltf .glb   read with cgltf (GltfImport.cpp). glTF stores binary
//                buffers viewed through "accessors" with types and strides;
//                a library handles that bookkeeping and we copy the data out.
//
// Every importer merges a file's meshes into one MeshData, in meters, with
// counter-clockwise front faces, uv (0,0) at the bottom-left of the image,
// and unit normals (computed if the file has none).
//
// Not imported yet: materials, multiple separate meshes, animation.
// TODO(later phase): import base color textures and keep sub-meshes apart.

#include "render/Mesh.h"

#include <optional>
#include <string>

namespace Engine {

struct ModelImportSettings {
    float scale = 1.0f; // e.g. 0.01 for a file made in centimeters
};

// Picks the importer from the file extension. On failure returns nothing and
// sets *error to a message that names the file (and the line, for OBJ).
std::optional<MeshData> importModel(const std::string& path, const ModelImportSettings& settings = {},
                                    std::string* error = nullptr);

std::optional<MeshData> importObj(const std::string& path, const ModelImportSettings& settings = {},
                                  std::string* error = nullptr);
// The OBJ reader works on text, so tests can feed it a string directly.
std::optional<MeshData> parseObj(const std::string& text, const ModelImportSettings& settings = {},
                                 std::string* error = nullptr, const std::string& nameForErrors = "OBJ text");

std::optional<MeshData> importGltf(const std::string& path, const ModelImportSettings& settings = {},
                                   std::string* error = nullptr);

// For vertices whose normal is (0,0,0): the average of the normals of the
// triangles that use them, weighted by area, so large faces count more.
// Shared vertices come out smooth; vertices used by one face come out flat.
void computeMissingNormals(MeshData& mesh);

} // namespace Engine
