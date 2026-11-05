#pragma once
// AssetManager: turns asset ids into loaded GPU objects, and keeps them.
//
//   const Mesh* m = assets.mesh(id);     first call: find the file in the
//                                        registry, import it, upload it, cache it
//                                        later calls: the cached pointer
//
// Each asset is loaded once no matter how many entities use it: a hundred
// crates share one mesh and one texture on the GPU. That's the "texture cache"
// (and mesh cache) of this phase.
//
// A failed load is remembered too, so a missing file is reported once rather
// than retried and logged 60 times a second.
//
// Pointers stay valid until unloadAll() or the manager is destroyed (the
// objects live in unique_ptrs, which don't move when the maps grow). Like
// every GPU object, the manager must be destroyed before the window.

#include "assets/AssetRegistry.h"
#include "render/Mesh.h"
#include "render/Texture.h"
#include "scene/RenderAssets.h"

#include <memory>
#include <unordered_map>

namespace Engine {

class AssetManager final : public RenderAssets {
public:
    // The registry must outlive the manager (not owned).
    explicit AssetManager(const AssetRegistry& registry) : registry_(registry) {}

    const Mesh* mesh(AssetId id) override;
    const Texture* texture(AssetId id) override;

    // Frees everything loaded; the next request loads from disk again.
    // TODO(later phase): reload only what changed on disk, like shaders do.
    void unloadAll();

    std::size_t loadedMeshes() const { return meshes_.size(); }
    std::size_t loadedTextures() const { return textures_.size(); }

private:
    std::unique_ptr<Mesh> loadMesh(AssetId id);
    std::unique_ptr<Texture> loadTexture(AssetId id);
    // The record for id if it exists and has the expected type; logs otherwise.
    const AssetRecord* recordFor(AssetId id, AssetType expected) const;

    const AssetRegistry& registry_;
    // A null unique_ptr value means "tried, failed".
    std::unordered_map<AssetId, std::unique_ptr<Mesh>> meshes_;
    std::unordered_map<AssetId, std::unique_ptr<Texture>> textures_;
};

} // namespace Engine
