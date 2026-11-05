#pragma once
// AssetRegistry: knows every asset file in the project and its id.
//
// scan() walks the assets folder. For every file it can use (textures,
// models, scenes) it reads the ".meta" file beside it:
//
//   assets/textures/crate.png
//   assets/textures/crate.png.meta     {"guid": "...", "type": "texture", "settings": {...}}
//
// A file without a .meta gets one, with a fresh id. That's the only time an
// id is made, so commit .meta files along with their assets: they are what
// keeps scenes pointing at the right files.
//
//   rename crate.png -> wooden_crate.png (and its .meta) -> same id, scenes still work
//   copy   crate.png and its .meta                         -> two files, one id: the copy gets
//                                                            a new id and a warning
//   delete crate.png but not the .meta                     -> an "orphan" .meta: warned, left alone
//
// The registry only reads and writes small text files; it loads nothing onto
// the GPU (that's AssetManager), so it works, and is tested, without a window.

#include "assets/JsonUtil.h"
#include "core/AssetId.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Engine {

enum class AssetType { Texture, Mesh, Scene };

struct AssetRecord {
    AssetId id;
    AssetType type = AssetType::Texture;
    std::string path; // relative to the assets folder, with forward slashes: "models/barrel.obj"
    Json settings;    // import settings from the .meta (texture color space, model scale...)
};

struct ScanReport {
    int assets = 0;            // usable files found
    int metaFilesCreated = 0;  // new ids handed out
    int duplicateIds = 0;      // copied .meta files that got a new id
    int orphanMetaFiles = 0;   // .meta files whose asset is gone
    int unreadableMetaFiles = 0; // broken .meta files (asset skipped, file left as is)
    int ignoredFiles = 0;      // files of other kinds (shaders, notes...)
};

class AssetRegistry {
public:
    explicit AssetRegistry(std::string rootDirectory);

    // (Re)reads the whole folder. Safe to call again after files change.
    ScanReport scan();

    const AssetRecord* find(AssetId id) const;
    const AssetRecord* findByPath(const std::string& relativePath) const;
    // Shortcut: the id of a path, or a null id if it isn't a known asset.
    AssetId idForPath(const std::string& relativePath) const;
    std::string absolutePath(const AssetRecord& record) const;
    // Every asset, sorted by path.
    std::vector<const AssetRecord*> all() const;
    const std::string& root() const { return root_; }

    // Adds (or refreshes) one file, for example a scene that was just saved,
    // creating its .meta if needed. Returns its id, or a null id if the file
    // isn't a usable asset type.
    AssetId registerFile(const std::string& relativePath);

    // "textures/crate.png" -> Texture, "a.scene.json" -> Scene, "notes.txt" -> nothing.
    static std::optional<AssetType> typeForPath(const std::string& path);
    static const char* typeName(AssetType type);
    // The settings a new .meta starts with, per type.
    static Json defaultSettings(AssetType type);

private:
    enum class MetaResult { Read, Created, Unreadable };
    MetaResult loadOrCreateMeta(const std::string& relativePath, AssetType type, AssetRecord& out);
    void writeMeta(const AssetRecord& record) const;
    void add(AssetRecord record);

    std::string root_;
    std::unordered_map<AssetId, AssetRecord> byId_;
    std::unordered_map<std::string, AssetId> byPath_;
};

} // namespace Engine
