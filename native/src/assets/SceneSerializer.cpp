#include "assets/SceneSerializer.h"

#include "core/Log.h"
#include "core/Paths.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <unordered_map>

namespace Engine {
namespace {

const char* builtinPath(AssetId id) {
    if (id == BuiltinAssets::Cube) return "builtin:cube";
    if (id == BuiltinAssets::Plane) return "builtin:plane";
    if (id == BuiltinAssets::Sphere) return "builtin:sphere";
    return nullptr;
}

bool isBuiltin(AssetId id) { return builtinPath(id) != nullptr; }

} // namespace

SceneSerializer::SceneSerializer(const AssetRegistry* registry) : registry_(registry) { registerBuiltins(); }

Json SceneSerializer::assetRef(AssetId id) const {
    if (id.isNull()) {
        return nullptr;
    }
    std::string path = "?";
    if (const char* builtin = builtinPath(id)) {
        path = builtin;
    } else if (registry_) {
        if (const AssetRecord* record = registry_->find(id)) {
            path = record->path;
        }
    }
    return {{"id", id.toString()}, {"path", path}};
}

AssetId SceneSerializer::readAssetRef(const Json& object, const char* key) const {
    if (!object.contains(key) || object[key].is_null()) {
        return {};
    }
    const Json& ref = object[key];
    // Accept a bare id string too, which is handy when typing a file by hand.
    const AssetId id = ref.is_string() ? readAssetId(object, key) : readAssetId(ref, "id");
    if (registry_ && !id.isNull() && !isBuiltin(id) && !registry_->find(id)) {
        const std::string path = ref.is_object() ? readString(ref, "path", "?") : "?";
        ENGINE_WARN("The scene uses asset ", id.toString(), " (", path, "), which isn't in ", registry_->root());
    }
    return id;
}

void SceneSerializer::registerBuiltins() {
    registerComponent<Transform>(
        "transform",
        [](const Transform& t) {
            return Json{{"position", jsonArray(t.position)},
                        {"rotation", jsonArray(Math::eulerDegreesFromQuat(t.rotation), 4)},
                        {"scale", jsonArray(t.scale)}};
        },
        [](Transform& t, const Json& j) {
            t.position = readVec3(j, "position", Vec3(0.0f));
            t.rotation = Math::quatFromEulerDegrees(readVec3(j, "rotation", Vec3(0.0f)));
            t.scale = readVec3(j, "scale", Vec3(1.0f));
        });

    registerComponent<MeshRenderer>(
        "meshRenderer",
        [this](const MeshRenderer& mr) {
            const MeshMaterial& m = mr.material;
            return Json{{"mesh", assetRef(mr.mesh)},
                        {"visible", mr.visible},
                        {"material",
                         {{"baseColor", jsonArray(m.baseColor)},
                          {"albedo", assetRef(m.albedo)},
                          {"specular", tidy(m.specular)},
                          {"shininess", tidy(m.shininess)},
                          {"uvScale", jsonArray(m.uvScale)}}}};
        },
        [this](MeshRenderer& mr, const Json& j) {
            mr.mesh = readAssetRef(j, "mesh");
            mr.visible = readBool(j, "visible", true);
            const Json& m = j.contains("material") ? j["material"] : Json::object();
            mr.material.baseColor = readVec3(m, "baseColor", Vec3(1.0f));
            mr.material.albedo = readAssetRef(m, "albedo");
            mr.material.specular = readFloat(m, "specular", 0.2f);
            mr.material.shininess = readFloat(m, "shininess", 32.0f);
            mr.material.uvScale = readVec2(m, "uvScale", Vec2(1.0f));
        });

    registerComponent<CameraComponent>(
        "camera",
        [](const CameraComponent& c) {
            return Json{{"fovDegrees", tidy(Math::toDegrees(c.fovY), 4)},
                        {"near", tidy(c.nearPlane)},
                        {"far", tidy(c.farPlane)},
                        {"primary", c.primary}};
        },
        [](CameraComponent& c, const Json& j) {
            c.fovY = Math::toRadians(readFloat(j, "fovDegrees", 60.0f));
            c.nearPlane = readFloat(j, "near", 0.05f);
            c.farPlane = readFloat(j, "far", 500.0f);
            c.primary = readBool(j, "primary", true);
        });

    registerComponent<LightComponent>(
        "light",
        [](const LightComponent& l) { return Json{{"color", jsonArray(l.color)}, {"intensity", tidy(l.intensity)}}; },
        [](LightComponent& l, const Json& j) {
            l.color = readVec3(j, "color", LightComponent{}.color);
            l.intensity = readFloat(j, "intensity", 1.0f);
        });

    registerComponent<Collider>(
        "collider",
        [](const Collider& c) {
            return Json{{"shape", c.shape == ColliderShape::Sphere ? "sphere" : "box"},
                        {"halfExtents", jsonArray(c.halfExtents)},
                        {"radius", tidy(c.radius)},
                        {"center", jsonArray(c.center)}};
        },
        [](Collider& c, const Json& j) {
            const std::string shape = readString(j, "shape", "box");
            if (shape != "box" && shape != "sphere") {
                throw JsonError("\"shape\" should be \"box\" or \"sphere\", but is \"" + shape + "\"");
            }
            c.shape = shape == "sphere" ? ColliderShape::Sphere : ColliderShape::Box;
            c.halfExtents = readVec3(j, "halfExtents", Vec3(0.5f));
            c.radius = readFloat(j, "radius", 0.5f);
            c.center = readVec3(j, "center", Vec3(0.0f));
        });
}

// ---------------------------------------------------------------- saving

Json SceneSerializer::toJson(const Scene& scene) const {
    Json entities = Json::array();
    int nextId = 1;
    // Depth first from the roots: parents come before their children, and
    // children keep their order, which keeps files stable between saves.
    for (Entity root : scene.roots()) {
        appendEntity(scene, root, 0, nextId, entities);
    }
    const SceneEnvironment& env = scene.environment;
    return Json{{"format", "xordor-scene"},
                {"version", FormatVersion},
                {"environment",
                 {{"clearColor", jsonArray(env.clearColor)},
                  {"skyAmbient", jsonArray(env.skyAmbient)},
                  {"groundAmbient", jsonArray(env.groundAmbient)}}},
                {"entities", std::move(entities)}};
}

void SceneSerializer::appendEntity(const Scene& scene, Entity e, int parentFileId, int& nextId, Json& entities) const {
    const int id = nextId++;
    // Json is ordered_json, so keys appear in the file in this order.
    Json entry = {{"id", id}, {"name", scene.nameOf(e)}};
    if (parentFileId != 0) {
        entry["parent"] = parentFileId;
    }
    for (const Handler& handler : components_) {
        if (handler.has(scene, e)) {
            entry[handler.key] = handler.save(scene, e);
        }
    }
    entities.push_back(std::move(entry));
    for (Entity child : scene.childrenOf(e)) {
        appendEntity(scene, child, id, nextId, entities);
    }
}

bool SceneSerializer::save(const Scene& scene, const std::string& path, std::string* error) const {
    const std::string text = prettyJson(toJson(scene));
    std::ofstream file(path, std::ios::binary);
    if (!file || !(file << text)) {
        if (error) *error = "Could not write " + path;
        return false;
    }
    return true;
}

// ---------------------------------------------------------------- loading

bool SceneSerializer::fromJson(const Json& document, Scene& out, std::string* error) const {
    // Everything is built in a separate scene and only moved into `out` at
    // the end, so a file with an error halfway through changes nothing.
    Scene scene;
    std::string where = "the document";
    try {
        if (!document.is_object() || readString(document, "format", "") != "xordor-scene") {
            throw JsonError("not a scene file (expected \"format\": \"xordor-scene\")");
        }
        const int version = static_cast<int>(readFloat(document, "version", 0.0f));
        if (version < 1 || version > FormatVersion) {
            throw JsonError("scene format version " + std::to_string(version) + " isn't supported (this build reads 1 to " +
                            std::to_string(FormatVersion) + ")");
        }
        if (document.contains("environment")) {
            where = "\"environment\"";
            const Json& env = document["environment"];
            scene.environment.clearColor = readVec3(env, "clearColor", scene.environment.clearColor);
            scene.environment.skyAmbient = readVec3(env, "skyAmbient", scene.environment.skyAmbient);
            scene.environment.groundAmbient = readVec3(env, "groundAmbient", scene.environment.groundAmbient);
        }
        if (!document.contains("entities") || !document["entities"].is_array()) {
            throw JsonError("\"entities\" should be an array");
        }

        // Pass 1: create every entity and its components, remembering file ids.
        std::unordered_map<int, Entity> byFileId;
        std::vector<std::pair<Entity, int>> parentLinks; // (entity, parent file id)
        std::set<std::string> unknownKeys;
        for (const Json& entry : document["entities"]) {
            const int fileId = static_cast<int>(readFloat(entry, "id", 0.0f));
            const std::string name = readString(entry, "name", "Entity");
            where = "entity " + std::to_string(fileId) + " (\"" + name + "\")";
            if (fileId <= 0 || byFileId.count(fileId)) {
                throw JsonError("every entity needs a unique positive \"id\"");
            }
            const Entity e = scene.createEntity(name);
            byFileId[fileId] = e;
            if (entry.contains("parent")) {
                parentLinks.emplace_back(e, static_cast<int>(readFloat(entry, "parent", 0.0f)));
            }
            for (const auto& item : entry.items()) {
                // Plain variables, not a structured binding: C++17 doesn't let
                // a lambda (the find_if below) capture structured bindings.
                const std::string key = item.key();
                const Json& value = item.value();
                if (key == "id" || key == "name" || key == "parent") {
                    continue;
                }
                const auto handler = std::find_if(components_.begin(), components_.end(),
                                                  [&](const Handler& h) { return h.key == key; });
                if (handler == components_.end()) {
                    unknownKeys.insert(key);
                    continue;
                }
                where = "entity " + std::to_string(fileId) + " (\"" + name + "\"), \"" + key + "\"";
                handler->load(scene, e, value);
            }
        }

        // Pass 2: parents. Done after every entity exists, so a hand-edited
        // file may list a child before its parent.
        where = "the parent links";
        for (const auto& [child, parentId] : parentLinks) {
            const auto parent = byFileId.find(parentId);
            if (parent == byFileId.end()) {
                ENGINE_WARN("Scene: ", scene.nameOf(child), " names parent ", parentId,
                            ", which doesn't exist; keeping it at the top level");
                continue;
            }
            // The file stores local transforms, so keep them as they are.
            scene.setParent(child, parent->second, false);
        }
        for (const std::string& key : unknownKeys) {
            ENGINE_WARN("Scene: skipped \"", key, "\" components: no serializer is registered for that key");
        }
    } catch (const std::exception& e) {
        if (error) *error = "In " + where + ": " + e.what();
        return false;
    }
    // Loaded transforms are where things *are*: nothing to blend from.
    scene.registry().each<Transform>([](Entity, Transform& t) { t.resetInterpolation(); });
    out = std::move(scene);
    return true;
}

bool SceneSerializer::load(const std::string& path, Scene& out, std::string* error) const {
    std::string text;
    if (!readTextFile(path, text)) {
        if (error) *error = "Could not read " + path;
        return false;
    }
    Json document;
    try {
        document = Json::parse(text);
    } catch (const Json::parse_error& e) {
        // nlohmann's message includes the line and column.
        if (error) *error = path + ": " + e.what();
        return false;
    }
    std::string why;
    if (!fromJson(document, out, &why)) {
        if (error) *error = path + ": " + why;
        return false;
    }
    return true;
}

} // namespace Engine
