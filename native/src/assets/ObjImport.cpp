// Wavefront OBJ reader.
//
// An OBJ file is lines of text. The ones we use:
//
//   v  1.0 2.0 3.0       a position
//   vt 0.5 1.0           a texture coordinate
//   vn 0.0 1.0 0.0       a normal
//   f  1/1/1 2/2/1 3/3/1 a face: position/uv/normal indices, counting from 1
//
// Face corners can also be "1" (position only), "1/2" (no normal) or "1//3"
// (no uv), and negative numbers count back from the latest entry (-1 = last).
// Faces with more than three corners are split into a fan of triangles.
//
// OBJ keeps separate lists of positions, uvs and normals, but the GPU needs
// one vertex per unique combination. So each distinct "position/uv/normal"
// triple becomes one Vertex, reused whenever the same triple appears again.

#include "assets/ModelImport.h"

#include "core/Log.h"
#include "core/Paths.h"

#include <array>
#include <cctype>
#include <initializer_list>
#include <map>
#include <sstream>
#include <vector>

namespace Engine {
namespace {

struct Corner {
    int position = -1; // 0-based; -1 = not given
    int uv = -1;
    int normal = -1;
    bool operator<(const Corner& o) const {
        return std::array<int, 3>{position, uv, normal} < std::array<int, 3>{o.position, o.uv, o.normal};
    }
};

class ObjParser {
public:
    ObjParser(const ModelImportSettings& settings, std::string name) : settings_(settings), name_(std::move(name)) {}

    std::optional<MeshData> parse(const std::string& text, std::string* error) {
        std::istringstream lines(text);
        std::string line;
        while (std::getline(lines, line)) {
            ++lineNumber_;
            if (!parseLine(line)) {
                if (error) *error = name_ + ", line " + std::to_string(lineNumber_) + ": " + problem_;
                return std::nullopt;
            }
        }
        if (mesh_.indices.empty()) {
            if (error) *error = name_ + ": no faces found";
            return std::nullopt;
        }
        if (ignoredLines_ > 0) {
            // Not an error: materials, groups and smoothing groups are common,
            // we just don't use them yet. Saying so avoids mystery later.
            ENGINE_INFO(name_, ": ignored ", ignoredLines_, " lines (materials, groups, etc. aren't imported)");
        }
        computeMissingNormals(mesh_);
        return std::move(mesh_);
    }

private:
    bool parseLine(std::string line) {
        if (const auto hash = line.find('#'); hash != std::string::npos) {
            line.erase(hash); // comment
        }
        std::istringstream in(line);
        std::string keyword;
        if (!(in >> keyword)) {
            return true; // blank line
        }
        if (keyword == "v") return readFloats(in, 3, [&](const float* f) { positions_.emplace_back(f[0], f[1], f[2]); });
        if (keyword == "vt") return readFloats(in, 2, [&](const float* f) { uvs_.emplace_back(f[0], f[1]); });
        if (keyword == "vn") return readFloats(in, 3, [&](const float* f) { normals_.emplace_back(f[0], f[1], f[2]); });
        if (keyword == "f") return readFace(in);
        ++ignoredLines_;
        return true;
    }

    template <typename Store>
    bool readFloats(std::istringstream& in, int count, Store store) {
        float values[3] = {};
        for (int i = 0; i < count; ++i) {
            if (!(in >> values[i])) {
                problem_ = "expected " + std::to_string(count) + " numbers";
                return false;
            }
        }
        // Extra numbers (like a "w" weight) are allowed and ignored.
        store(values);
        return true;
    }

    bool readFace(std::istringstream& in) {
        std::vector<std::uint32_t> corners;
        std::string token;
        while (in >> token) {
            Corner c;
            if (!parseCorner(token, c)) {
                return false;
            }
            corners.push_back(vertexFor(c));
        }
        if (corners.size() < 3) {
            problem_ = "a face needs at least 3 corners";
            return false;
        }
        // Fan: (0,1,2), (0,2,3), (0,3,4)... keeps the file's winding, which by
        // convention is counter-clockwise, the same as ours.
        for (size_t i = 1; i + 1 < corners.size(); ++i) {
            mesh_.indices.insert(mesh_.indices.end(), {corners[0], corners[i], corners[i + 1]});
        }
        return true;
    }

    bool parseCorner(const std::string& token, Corner& c) {
        int* fields[3] = {&c.position, &c.uv, &c.normal};
        const size_t sizes[3] = {positions_.size(), uvs_.size(), normals_.size()};
        size_t start = 0;
        for (int field = 0; field < 3 && start <= token.size(); ++field) {
            const size_t slash = token.find('/', start);
            const std::string part = token.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
            if (!part.empty()) {
                int index = 0;
                try {
                    index = std::stoi(part);
                } catch (const std::exception&) {
                    problem_ = "\"" + token + "\" is not a face corner like 1/2/3";
                    return false;
                }
                // 1-based from the start, or negative from the end.
                const long resolved = index > 0 ? index - 1 : static_cast<long>(sizes[field]) + index;
                if (index == 0 || resolved < 0 || resolved >= static_cast<long>(sizes[field])) {
                    problem_ = "index " + part + " in \"" + token + "\" points past the " +
                               std::to_string(sizes[field]) + " entries read so far";
                    return false;
                }
                *fields[field] = static_cast<int>(resolved);
            }
            if (slash == std::string::npos) {
                break;
            }
            start = slash + 1;
        }
        if (c.position < 0) {
            problem_ = "face corner \"" + token + "\" has no position";
            return false;
        }
        return true;
    }

    std::uint32_t vertexFor(const Corner& c) {
        const auto found = unique_.find(c);
        if (found != unique_.end()) {
            return found->second;
        }
        Vertex v;
        v.position = positions_[static_cast<size_t>(c.position)] * settings_.scale;
        v.normal = c.normal >= 0 ? glm::normalize(normals_[static_cast<size_t>(c.normal)]) : Vec3(0.0f);
        v.uv = c.uv >= 0 ? uvs_[static_cast<size_t>(c.uv)] : Vec2(0.0f);
        const auto index = static_cast<std::uint32_t>(mesh_.vertices.size());
        mesh_.vertices.push_back(v);
        unique_.emplace(c, index);
        return index;
    }

    ModelImportSettings settings_;
    std::string name_;
    std::vector<Vec3> positions_;
    std::vector<Vec2> uvs_;
    std::vector<Vec3> normals_;
    std::map<Corner, std::uint32_t> unique_; // TODO(performance): unordered_map for big files
    MeshData mesh_;
    int lineNumber_ = 0;
    int ignoredLines_ = 0;
    std::string problem_;
};

} // namespace

std::optional<MeshData> parseObj(const std::string& text, const ModelImportSettings& settings, std::string* error,
                                 const std::string& nameForErrors) {
    return ObjParser(settings, nameForErrors).parse(text, error);
}

std::optional<MeshData> importObj(const std::string& path, const ModelImportSettings& settings, std::string* error) {
    std::string text;
    if (!readTextFile(path, text)) {
        if (error) *error = "Could not read " + path;
        return std::nullopt;
    }
    return parseObj(text, settings, error, path);
}

void computeMissingNormals(MeshData& mesh) {
    std::vector<Vec3> sums(mesh.vertices.size(), Vec3(0.0f));
    std::vector<bool> missing(mesh.vertices.size());
    bool any = false;
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        missing[i] = glm::dot(mesh.vertices[i].normal, mesh.vertices[i].normal) == 0.0f;
        any = any || missing[i];
    }
    if (!any) {
        return;
    }
    for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const std::uint32_t a = mesh.indices[t], b = mesh.indices[t + 1], c = mesh.indices[t + 2];
        // The cross product's length is twice the triangle's area, so adding
        // it unnormalized weights each face by its size for free.
        const Vec3 face = glm::cross(mesh.vertices[b].position - mesh.vertices[a].position,
                                     mesh.vertices[c].position - mesh.vertices[a].position);
        for (std::uint32_t v : {a, b, c}) {
            sums[v] += face;
        }
    }
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        if (missing[i]) {
            const float length = glm::length(sums[i]);
            // A vertex used only by zero-area triangles has no direction; point it up.
            mesh.vertices[i].normal = length > 0.0f ? sums[i] / length : Math::WorldUp;
        }
    }
}

std::optional<MeshData> importModel(const std::string& path, const ModelImportSettings& settings, std::string* error) {
    std::string lower = path;
    for (char& ch : lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    const auto endsWith = [&](const char* ext) {
        const std::string e = ext;
        return lower.size() >= e.size() && lower.compare(lower.size() - e.size(), e.size(), e) == 0;
    };
    if (endsWith(".obj")) return importObj(path, settings, error);
    if (endsWith(".gltf") || endsWith(".glb")) return importGltf(path, settings, error);
    if (error) *error = path + ": unknown model format (supported: .obj, .gltf, .glb)";
    return std::nullopt;
}

} // namespace Engine
