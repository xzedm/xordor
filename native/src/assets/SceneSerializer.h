#pragma once
// SceneSerializer: saves a Scene to a JSON file and loads it back.
//
// The file is meant to be read (and edited) by people:
//
//   {
//     "format": "xordor-scene", "version": 1,
//     "environment": { "clearColor": [0.47, 0.62, 0.8], ... },
//     "entities": [
//       { "id": 1, "name": "Crate Row", "transform": { "position": [0, 0, 0], ... } },
//       { "id": 2, "name": "Crate 1", "parent": 1,
//         "transform":    { "position": [-3, 0.5, -1], "rotation": [0, 0, 0], "scale": [1, 1, 1] },
//         "meshRenderer": { "mesh": { "id": "00000000-...-000000000001", "path": "builtin:cube" }, ... },
//         "collider":     { "shape": "box", "halfExtents": [0.5, 0.5, 0.5], ... } }
//     ]
//   }
//
// * Entities are written with their SceneId ("id"), which stays the same
//   across saves and loads; "parent" refers to it. Entity handles aren't
//   stable between runs, so they're never written.
// * Rotations are Euler angles in degrees, [pitch, yaw, roll]: readable and
//   typeable, unlike quaternions.
// * Assets are written as {"id", "path"}. Only the id is used to load; the
//   path is there so a person (or a diff in code review) can see which file it
//   is, and so a missing asset can be reported by name.
// * Each component is one key. Engine components are built in; an app adds
//   its own with registerComponent<T>(). A key nobody registered is kept as
//   plain JSON (UnregisteredComponents) and written back on save, so opening
//   and saving a scene in a program that doesn't know the sandbox's "spin"
//   never loses it. Loading reports such keys once.

#include "assets/AssetRegistry.h"
#include "assets/JsonUtil.h"
#include "scene/Scene.h"

#include <functional>
#include <string>
#include <vector>

namespace Engine {

// Components from a scene file that no serializer was registered for, kept
// as JSON so they survive a load and save. Key -> the component's object.
struct UnregisteredComponents {
    Json values = Json::object();
};

class SceneSerializer {
public:
    static constexpr int FormatVersion = 1;

    // The registry is optional; with it, files get readable asset paths and
    // loading can name assets that are missing.
    explicit SceneSerializer(const AssetRegistry* registry = nullptr);

    // Teaches the serializer a component type. save() turns it into a JSON
    // object; load() fills a default-constructed one from JSON (throw
    // JsonError on bad data, as the read* helpers do).
    template <typename T>
    void registerComponent(const std::string& key, std::function<Json(const T&)> save,
                           std::function<void(T&, const Json&)> load) {
        components_.push_back(Handler{
            key,
            [](const Scene& s, Entity e) { return s.has<T>(e); },
            [save](const Scene& s, Entity e) { return save(s.get<T>(e)); },
            [load](Scene& s, Entity e, const Json& j) {
                T component{};
                load(component, j);
                if (s.has<T>(e)) {
                    s.get<T>(e) = std::move(component);
                } else {
                    s.add<T>(e, std::move(component));
                }
            },
            [](Scene& s, Entity e) { s.remove<T>(e); },
        });
    }

    // ---------------------------------------------------------------- one component, by key
    // Used by the editor: undo stores a component as JSON before and after an
    // edit, so one generic command can undo a change to any component type.
    std::vector<std::string> componentKeys() const;
    // The keys this entity has, registered ones first in registration order,
    // then unregistered ones as found in the file.
    std::vector<std::string> componentKeysOf(const Scene& scene, Entity e) const;
    bool isRegistered(const std::string& key) const { return findHandler(key) != nullptr; }
    bool hasComponent(const Scene& scene, Entity e, const std::string& key) const;
    Json saveComponent(const Scene& scene, Entity e, const std::string& key) const; // null if absent
    // Adds or replaces it. Throws JsonError on bad data. An unregistered key
    // is stored as JSON in UnregisteredComponents.
    void loadComponent(Scene& scene, Entity e, const std::string& key, const Json& value) const;
    void removeComponent(Scene& scene, Entity e, const std::string& key) const;

    // ---------------------------------------------------------------- subtrees
    // An entity and all its descendants, as the same JSON objects a scene file
    // holds. Used for delete (and its undo), duplicate, and copy / paste.
    Json saveEntityTree(const Scene& scene, Entity root) const;
    // Recreates entities saved by saveEntityTree. keepIds = true restores the
    // original SceneIds (undoing a delete); false hands out new ones
    // (duplicating). The root goes under `parent` (NullEntity = top level).
    // Returns the new root, or NullEntity (and sets *error) on bad data, in
    // which case nothing is left behind.
    Entity loadEntityTree(Scene& scene, const Json& entities, bool keepIds, Entity parent,
                          std::string* error = nullptr) const;

    Json toJson(const Scene& scene) const;
    // Fills `out` only if the whole document is valid; on error `out` is
    // left as it was and *error says what and where.
    bool fromJson(const Json& document, Scene& out, std::string* error = nullptr) const;

    bool save(const Scene& scene, const std::string& path, std::string* error = nullptr) const;
    bool load(const std::string& path, Scene& out, std::string* error = nullptr) const;

    // Asset references, shared with custom components that point at assets.
    Json assetRef(AssetId id) const;
    AssetId readAssetRef(const Json& object, const char* key) const;

private:
    struct Handler {
        std::string key;
        std::function<bool(const Scene&, Entity)> has;
        std::function<Json(const Scene&, Entity)> save;
        std::function<void(Scene&, Entity, const Json&)> load;
        std::function<void(Scene&, Entity)> remove;
    };

    void registerBuiltins();
    const Handler* findHandler(const std::string& key) const;
    Json entityJson(const Scene& scene, Entity e) const;
    void appendTree(const Scene& scene, Entity e, Json& entities) const;
    // Creates entities from a JSON array (throws JsonError). Returns them in
    // file order. Parents are resolved among the new entities first, then
    // (keepIds only) among existing ones.
    // warn = false: a dry run to validate data; stays quiet about parents
    // that only exist in the real scene.
    std::vector<Entity> createEntities(Scene& scene, const Json& entities, bool keepIds, bool warn = true) const;

    const AssetRegistry* registry_;
    std::vector<Handler> components_;
};

} // namespace Engine
