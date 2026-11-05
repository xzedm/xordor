#include "assets/AssetManager.h"

#include "assets/ModelImport.h"
#include "core/Log.h"
#include "render/Primitives.h"

namespace Engine {

const Mesh* AssetManager::mesh(AssetId id) {
    if (id.isNull()) {
        return nullptr;
    }
    auto it = meshes_.find(id);
    if (it == meshes_.end()) {
        it = meshes_.emplace(id, loadMesh(id)).first; // loaded (or failed) exactly once
    }
    return it->second.get();
}

const Texture* AssetManager::texture(AssetId id) {
    if (id.isNull()) {
        return nullptr;
    }
    auto it = textures_.find(id);
    if (it == textures_.end()) {
        it = textures_.emplace(id, loadTexture(id)).first;
    }
    return it->second.get();
}

void AssetManager::unloadAll() {
    meshes_.clear();
    textures_.clear();
}

const AssetRecord* AssetManager::recordFor(AssetId id, AssetType expected) const {
    const AssetRecord* record = registry_.find(id);
    if (!record) {
        ENGINE_ERROR("Asset ", id.toString(), " is unknown: no file in ", registry_.root(),
                     " has this id in its .meta (deleted, or its .meta was lost?)");
        return nullptr;
    }
    if (record->type != expected) {
        ENGINE_ERROR("Asset ", record->path, " is a ", AssetRegistry::typeName(record->type), ", not a ",
                     AssetRegistry::typeName(expected));
        return nullptr;
    }
    return record;
}

std::unique_ptr<Mesh> AssetManager::loadMesh(AssetId id) {
    // The meshes made in code have fixed ids and no file.
    if (id == BuiltinAssets::Cube) return std::make_unique<Mesh>(Primitives::cube());
    if (id == BuiltinAssets::Plane) return std::make_unique<Mesh>(Primitives::plane());
    if (id == BuiltinAssets::Sphere) return std::make_unique<Mesh>(Primitives::sphere(0.5f, 48, 24));

    const AssetRecord* record = recordFor(id, AssetType::Mesh);
    if (!record) {
        return nullptr;
    }
    ModelImportSettings settings;
    settings.scale = readFloat(record->settings, "scale", 1.0f);
    std::string error;
    const auto data = importModel(registry_.absolutePath(*record), settings, &error);
    if (!data) {
        ENGINE_ERROR("Could not import ", record->path, ": ", error);
        return nullptr;
    }
    ENGINE_INFO("Loaded mesh ", record->path, " (", data->vertices.size(), " vertices, ", data->triangleCount(),
                " triangles)");
    return std::make_unique<Mesh>(*data);
}

std::unique_ptr<Texture> AssetManager::loadTexture(AssetId id) {
    const AssetRecord* record = recordFor(id, AssetType::Texture);
    if (!record) {
        return nullptr;
    }
    TextureSettings settings;
    try {
        settings.colorSpace = readString(record->settings, "colorSpace", "srgb") == "linear" ? ColorSpace::Linear
                                                                                              : ColorSpace::SRGB;
        settings.mipmaps = readBool(record->settings, "mipmaps", true);
        settings.repeat = readBool(record->settings, "repeat", true);
        settings.smooth = readBool(record->settings, "smooth", true);
    } catch (const JsonError& e) {
        ENGINE_WARN(record->path, ".meta: ", e.what(), "; using default texture settings");
        settings = TextureSettings{};
    }
    std::string error;
    auto loaded = Texture::fromFile(registry_.absolutePath(*record), settings, &error);
    if (!loaded) {
        ENGINE_ERROR("Could not load ", record->path, ": ", error);
        return nullptr;
    }
    ENGINE_INFO("Loaded texture ", record->path, " (", loaded->width(), "x", loaded->height(), ")");
    return std::make_unique<Texture>(std::move(*loaded));
}

} // namespace Engine
