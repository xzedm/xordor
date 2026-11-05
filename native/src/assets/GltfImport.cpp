// glTF 2.0 reader, using cgltf.
//
// A glTF file is a scene tree of nodes. Each node has a transform and maybe
// a mesh; a mesh is a list of "primitives" (one draw each); a primitive has
// attributes (POSITION, NORMAL, TEXCOORD_0...) and indices. Each of those is
// an *accessor*: "N elements of type vec3 float, starting at byte X of buffer
// Y, every Z bytes". cgltf_accessor_read_float hides that layout from us.
//
// We walk the node tree, bake each node's world transform into its vertices,
// and append everything to one MeshData.
//
// glTF and our engine agree on most things (meters, right-handed, +Y up,
// counter-clockwise front faces). Two differences:
//   * uv (0,0) is the top-left of the image in glTF, bottom-left for us: v = 1 - v.
//   * glTF models face +Z ("toward the viewer"); our cameras and lights
//     look down -Z. Rotate the entity 180 degrees if a model faces away.

#include "assets/ModelImport.h"

#include "core/Log.h"

#include <cgltf.h>

#include <memory>

namespace Engine {
namespace {

using GltfData = std::unique_ptr<cgltf_data, decltype(&cgltf_free)>;

const char* resultName(cgltf_result r) {
    switch (r) {
    case cgltf_result_success: return "success";
    case cgltf_result_data_too_short: return "the file is cut short";
    case cgltf_result_unknown_format: return "not a glTF file";
    case cgltf_result_invalid_json: return "invalid JSON";
    case cgltf_result_invalid_gltf: return "invalid glTF";
    case cgltf_result_invalid_options: return "invalid options";
    case cgltf_result_file_not_found: return "file not found (the model, or a buffer or texture it names)";
    case cgltf_result_io_error: return "could not read the file";
    case cgltf_result_out_of_memory: return "out of memory";
    case cgltf_result_legacy_gltf: return "glTF 1.0 (only 2.0 is supported)";
    default: return "unknown error";
    }
}

const cgltf_accessor* findAttribute(const cgltf_primitive& prim, cgltf_attribute_type type) {
    for (cgltf_size i = 0; i < prim.attributes_count; ++i) {
        // index 0: TEXCOORD_0 rather than TEXCOORD_1 and so on.
        if (prim.attributes[i].type == type && prim.attributes[i].index == 0) {
            return prim.attributes[i].data;
        }
    }
    return nullptr;
}

class GltfReader {
public:
    GltfReader(const ModelImportSettings& settings, std::string path) : settings_(settings), path_(std::move(path)) {}

    void readNode(const cgltf_node& node) {
        if (node.mesh) {
            cgltf_float m[16];
            cgltf_node_transform_world(&node, m);
            // glTF matrices are column-major, the same as GLM.
            Mat4 world(1.0f); // every entry is overwritten below
            for (int c = 0; c < 4; ++c) {
                for (int r = 0; r < 4; ++r) {
                    world[c][r] = m[c * 4 + r];
                }
            }
            for (cgltf_size p = 0; p < node.mesh->primitives_count; ++p) {
                readPrimitive(node.mesh->primitives[p], world);
            }
        }
        for (cgltf_size i = 0; i < node.children_count; ++i) {
            readNode(*node.children[i]);
        }
    }

    MeshData& mesh() { return mesh_; }
    int skipped() const { return skipped_; }

private:
    void readPrimitive(const cgltf_primitive& prim, const Mat4& world) {
        if (prim.type != cgltf_primitive_type_triangles) {
            ++skipped_; // lines and points: nothing to fill
            return;
        }
        const cgltf_accessor* positions = findAttribute(prim, cgltf_attribute_type_position);
        if (!positions) {
            ++skipped_;
            return;
        }
        const cgltf_accessor* normals = findAttribute(prim, cgltf_attribute_type_normal);
        const cgltf_accessor* uvs = findAttribute(prim, cgltf_attribute_type_texcoord);
        const Mat4 placed = Math::composeTRS(Vec3(0.0f), Quat(1, 0, 0, 0), Vec3(settings_.scale)) * world;
        const Mat3 normalMatrix = Math::normalMatrix(placed);

        const auto base = static_cast<std::uint32_t>(mesh_.vertices.size());
        for (cgltf_size i = 0; i < positions->count; ++i) {
            Vertex v;
            cgltf_float p[3] = {};
            cgltf_accessor_read_float(positions, i, p, 3);
            v.position = Math::transformPoint(placed, Vec3(p[0], p[1], p[2]));
            if (normals && i < normals->count) {
                cgltf_float n[3] = {};
                cgltf_accessor_read_float(normals, i, n, 3);
                v.normal = glm::normalize(normalMatrix * Vec3(n[0], n[1], n[2]));
            } // else (0,0,0): computeMissingNormals fills it in
            if (uvs && i < uvs->count) {
                cgltf_float t[2] = {};
                cgltf_accessor_read_float(uvs, i, t, 2);
                v.uv = Vec2(t[0], 1.0f - t[1]);
            }
            mesh_.vertices.push_back(v);
        }

        // A mirrored node (negative scale) turns the triangles inside out;
        // swapping two corners turns them back.
        const bool mirrored = glm::determinant(Mat3(placed)) < 0.0f;
        const cgltf_size count = prim.indices ? prim.indices->count : positions->count;
        for (cgltf_size i = 0; i + 2 < count; i += 3) {
            std::uint32_t tri[3];
            for (int k = 0; k < 3; ++k) {
                const cgltf_size at = i + static_cast<cgltf_size>(k);
                tri[k] = base + static_cast<std::uint32_t>(prim.indices ? cgltf_accessor_read_index(prim.indices, at) : at);
            }
            if (mirrored) {
                std::swap(tri[1], tri[2]);
            }
            mesh_.indices.insert(mesh_.indices.end(), {tri[0], tri[1], tri[2]});
        }
    }

    ModelImportSettings settings_;
    std::string path_;
    MeshData mesh_;
    int skipped_ = 0;
};

} // namespace

std::optional<MeshData> importGltf(const std::string& path, const ModelImportSettings& settings, std::string* error) {
    const auto fail = [&](const std::string& why) -> std::optional<MeshData> {
        if (error) *error = path + ": " + why;
        return std::nullopt;
    };

    cgltf_options options{};
    cgltf_data* raw = nullptr;
    cgltf_result result = cgltf_parse_file(&options, path.c_str(), &raw);
    // Owned from here on: cgltf_free runs however we leave this function.
    GltfData data(raw, &cgltf_free);
    if (result != cgltf_result_success) {
        return fail(resultName(result));
    }
    // .gltf files keep their vertex data in separate .bin files (or embedded
    // base64 text); this reads them. .glb files carry it inside.
    result = cgltf_load_buffers(&options, data.get(), path.c_str());
    if (result != cgltf_result_success) {
        return fail(std::string("loading buffers: ") + resultName(result));
    }
    result = cgltf_validate(data.get());
    if (result != cgltf_result_success) {
        return fail(std::string("validation: ") + resultName(result));
    }

    GltfReader reader(settings, path);
    const cgltf_scene* scene = data->scene ? data->scene : (data->scenes_count > 0 ? &data->scenes[0] : nullptr);
    if (scene) {
        for (cgltf_size i = 0; i < scene->nodes_count; ++i) {
            reader.readNode(*scene->nodes[i]);
        }
    } else {
        // No scene listed: take every top-level node.
        for (cgltf_size i = 0; i < data->nodes_count; ++i) {
            if (!data->nodes[i].parent) {
                reader.readNode(data->nodes[i]);
            }
        }
    }
    if (reader.skipped() > 0) {
        ENGINE_WARN(path, ": skipped ", reader.skipped(), " primitives that aren't triangles or have no positions");
    }
    if (reader.mesh().indices.empty()) {
        return fail("no triangles found");
    }
    computeMissingNormals(reader.mesh());
    return std::move(reader.mesh());
}

} // namespace Engine
