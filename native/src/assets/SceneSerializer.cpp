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

// ---------------------------------------------------------------- one component

const SceneSerializer::Handler* SceneSerializer::findHandler(const std::string& key) const {
    for (const Handler& h : components_) {
        if (h.key == key) return &h;
    }
    return nullptr;
}

std::vector<std::string> SceneSerializer::componentKeys() const {
    std::vector<std::string> keys;
    for (const Handler& h : components_) keys.push_back(h.key);
    return keys;
}

std::vector<std::string> SceneSerializer::componentKeysOf(const Scene& scene, Entity e) const {
    std::vector<std::string> keys;
    if (!scene.isAlive(e)) return keys;
    for (const Handler& h : components_) {
        if (h.has(scene, e)) keys.push_back(h.key);
    }
    if (const auto* raw = scene.tryGet<UnregisteredComponents>(e)) {
        for (const auto& item : raw->values.items()) keys.push_back(item.key());
    }
    return keys;
}

bool SceneSerializer::hasComponent(const Scene& scene, Entity e, const std::string& key) const {
    if (!scene.isAlive(e)) return false;
    if (const Handler* h = findHandler(key)) return h->has(scene, e);
    const auto* raw = scene.tryGet<UnregisteredComponents>(e);
    return raw && raw->values.contains(key);
}

Json SceneSerializer::saveComponent(const Scene& scene, Entity e, const std::string& key) const {
    if (!hasComponent(scene, e, key)) return nullptr;
    if (const Handler* h = findHandler(key)) return h->save(scene, e);
    return scene.get<UnregisteredComponents>(e).values[key];
}

void SceneSerializer::loadComponent(Scene& scene, Entity e, const std::string& key, const Json& value) const {
    if (const Handler* h = findHandler(key)) {
        h->load(scene, e, value);
        return;
    }
    if (!scene.has<UnregisteredComponents>(e)) scene.add<UnregisteredComponents>(e);
    scene.get<UnregisteredComponents>(e).values[key] = value;
}

void SceneSerializer::removeComponent(Scene& scene, Entity e, const std::string& key) const {
    if (const Handler* h = findHandler(key)) {
        h->remove(scene, e);
    } else if (auto* raw = scene.tryGet<UnregisteredComponents>(e)) {
        raw->values.erase(key);
        if (raw->values.empty()) scene.remove<UnregisteredComponents>(e);
    }
}

// ---------------------------------------------------------------- saving

Json SceneSerializer::toJson(const Scene& scene) const {
    Json entities = Json::array();
    // Depth first from the roots: parents come before their children, and
    // children keep their order, which keeps files stable between saves.
    for (Entity root : scene.roots()) {
        appendTree(scene, root, entities);
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

Json SceneSerializer::entityJson(const Scene& scene, Entity e) const {
    // Json is ordered_json, so keys appear in the file in this order.
    Json entry = {{"id", scene.idOf(e)}, {"name", scene.nameOf(e)}};
    const Entity parent = scene.parentOf(e);
    if (!parent.isNull()) {
        entry["parent"] = scene.idOf(parent);
    }
    for (const Handler& handler : components_) {
        if (handler.has(scene, e)) {
            entry[handler.key] = handler.save(scene, e);
        }
    }
    if (const auto* raw = scene.tryGet<UnregisteredComponents>(e)) {
        for (const auto& item : raw->values.items()) {
            entry[item.key()] = item.value(); // written back exactly as read
        }
    }
    return entry;
}

void SceneSerializer::appendTree(const Scene& scene, Entity e, Json& entities) const {
    entities.push_back(entityJson(scene, e));
    for (Entity child : scene.childrenOf(e)) {
        appendTree(scene, child, entities);
    }
}

Json SceneSerializer::saveEntityTree(const Scene& scene, Entity root) const {
    Json entities = Json::array();
    if (scene.isAlive(root)) {
        appendTree(scene, root, entities);
    }
    return entities;
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

std::vector<Entity> SceneSerializer::createEntities(Scene& scene, const Json& entities, bool keepIds, bool warn) const {
    if (!entities.is_array()) {
        throw JsonError("\"entities\" should be an array");
    }
    // Pass 1: create every entity and its components, remembering file ids.
    std::vector<Entity> created;
    // If anything below throws, remove what was already made, so a caller
    // working on a live scene never sees half a subtree.
    struct Cleanup {
        Scene& scene;
        std::vector<Entity>& created;
        bool done = false;
        ~Cleanup() {
            if (!done) {
                for (Entity e : created) scene.destroyEntity(e);
            }
        }
    } cleanup{scene, created};
    std::unordered_map<SceneId, Entity> byFileId;
    std::vector<std::pair<Entity, SceneId>> parentLinks; // (entity, parent's file id)
    std::set<std::string> unknownKeys;
    for (const Json& entry : entities) {
        const SceneId fileId = readUInt64(entry, "id", 0);
        const std::string name = readString(entry, "name", "Entity");
        const std::string where = "entity " + std::to_string(fileId) + " (\"" + name + "\")";
        if (fileId == 0 || byFileId.count(fileId)) {
            throw JsonError(where + ": every entity needs a unique positive \"id\"");
        }
        const Entity e = keepIds ? scene.createEntityWithId(name, fileId) : scene.createEntity(name);
        if (e.isNull()) {
            throw JsonError(where + ": the id is already used in this scene");
        }
        created.push_back(e);
        byFileId[fileId] = e;
        if (entry.contains("parent")) {
            parentLinks.emplace_back(e, readUInt64(entry, "parent", 0));
        }
        for (const auto& item : entry.items()) {
            // Plain variables, not a structured binding: C++17 doesn't let
            // a lambda capture structured bindings.
            const std::string key = item.key();
            if (key == "id" || key == "name" || key == "parent") {
                continue;
            }
            const Handler* handler = findHandler(key);
            if (!handler) {
                unknownKeys.insert(key);
                if (!scene.has<UnregisteredComponents>(e)) scene.add<UnregisteredComponents>(e);
                scene.get<UnregisteredComponents>(e).values[key] = item.value();
                continue;
            }
            try {
                handler->load(scene, e, item.value());
            } catch (const JsonError& error) {
                throw JsonError(where + ", \"" + key + "\": " + error.what());
            }
        }
    }

    // Pass 2: parents, once every entity exists, so a hand-edited file may
    // list a child before its parent.
    for (const auto& [child, parentId] : parentLinks) {
        auto parent = byFileId.find(parentId);
        Entity target = parent != byFileId.end() ? parent->second : (keepIds ? scene.findById(parentId) : NullEntity);
        if (target.isNull()) {
            if ((parent == byFileId.end() && !keepIds) || !warn) {
                continue; // a duplicated subtree's root (the caller places it), or a dry run
            }
            ENGINE_WARN("Scene: ", scene.nameOf(child), " names parent ", parentId,
                        ", which doesn't exist; keeping it at the top level");
            continue;
        }
        // The file stores local transforms, so keep them as they are.
        scene.setParent(child, target, false);
    }
    for (const std::string& key : warn ? unknownKeys : std::set<std::string>{}) {
        ENGINE_INFO("Scene: kept \"", key, "\" components as plain JSON (no serializer is registered for that key)");
    }
    // Loaded transforms are where things *are*: nothing to blend from.
    for (Entity e : created) {
        scene.get<Transform>(e).resetInterpolation();
    }
    cleanup.done = true;
    return created;
}

bool SceneSerializer::fromJson(const Json& document, Scene& out, std::string* error) const {
    // Everything is built in a separate scene and only moved into `out` at
    // the end, so a file with an error halfway through changes nothing.
    Scene scene;
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
            const Json& env = document["environment"];
            try {
                scene.environment.clearColor = readVec3(env, "clearColor", scene.environment.clearColor);
                scene.environment.skyAmbient = readVec3(env, "skyAmbient", scene.environment.skyAmbient);
                scene.environment.groundAmbient = readVec3(env, "groundAmbient", scene.environment.groundAmbient);
            } catch (const JsonError& e) {
                throw JsonError(std::string("\"environment\": ") + e.what());
            }
        }
        if (!document.contains("entities")) {
            throw JsonError("\"entities\" should be an array");
        }
        createEntities(scene, document["entities"], true);
    } catch (const std::exception& e) {
        if (error) *error = e.what();
        return false;
    }
    out = std::move(scene);
    return true;
}

Entity SceneSerializer::loadEntityTree(Scene& scene, const Json& entities, bool keepIds, Entity parent,
                                       std::string* error) const {
    // Build in a scratch scene first, so bad data leaves the real scene alone,
    // then build again for real. (Simple rather than fast: subtrees are small.)
    try {
        Scene scratch;
        createEntities(scratch, entities, true, false);
    } catch (const std::exception& e) {
        if (error) *error = e.what();
        return NullEntity;
    }
    std::vector<Entity> created;
    try {
        created = createEntities(scene, entities, keepIds);
    } catch (const std::exception& e) {
        // Only possible with keepIds (an id already taken in this scene);
        // createEntities has removed what it made.
        if (error) *error = e.what();
        return NullEntity;
    }
    if (created.empty()) {
        return NullEntity;
    }
    const Entity root = created.front();
    if (!parent.isNull() || !keepIds) {
        scene.setParent(root, parent, false);
    }
    return root;
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
