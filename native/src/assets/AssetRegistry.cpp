#include "assets/AssetRegistry.h"

#include "core/Log.h"
#include "core/Paths.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <initializer_list>

namespace fs = std::filesystem;

namespace Engine {
namespace {

const char* MetaExtension = ".meta";

bool endsWith(const std::string& text, const std::string& suffix) {
    return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string lowercase(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

std::optional<AssetType> typeFromName(const std::string& name) {
    if (name == "texture") return AssetType::Texture;
    if (name == "mesh") return AssetType::Mesh;
    if (name == "scene") return AssetType::Scene;
    return std::nullopt;
}

bool isHidden(const fs::path& relative) {
    // Skip anything inside a folder (or file) whose name starts with a dot,
    // such as .git or editor backup folders.
    for (const auto& part : relative) {
        const std::string name = part.string();
        if (!name.empty() && name[0] == '.' && name != "." && name != "..") {
            return true;
        }
    }
    return false;
}

} // namespace

AssetRegistry::AssetRegistry(std::string rootDirectory) : root_(std::move(rootDirectory)) {}

std::optional<AssetType> AssetRegistry::typeForPath(const std::string& path) {
    const std::string p = lowercase(path);
    if (endsWith(p, ".scene.json")) return AssetType::Scene;
    for (const char* ext : {".png", ".jpg", ".jpeg", ".tga", ".bmp"}) {
        if (endsWith(p, ext)) return AssetType::Texture;
    }
    for (const char* ext : {".obj", ".gltf", ".glb"}) {
        if (endsWith(p, ext)) return AssetType::Mesh;
    }
    return std::nullopt;
}

const char* AssetRegistry::typeName(AssetType type) {
    switch (type) {
    case AssetType::Texture: return "texture";
    case AssetType::Mesh: return "mesh";
    case AssetType::Scene: return "scene";
    }
    return "unknown";
}

Json AssetRegistry::defaultSettings(AssetType type) {
    switch (type) {
    case AssetType::Texture:
        // Same defaults as TextureSettings. "linear" is for data textures
        // such as normal maps, whose numbers aren't colors.
        return {{"colorSpace", "srgb"}, {"mipmaps", true}, {"repeat", true}, {"smooth", true}};
    case AssetType::Mesh:
        // Many tools export centimeters; scale 0.01 turns them into meters.
        return {{"scale", 1.0}};
    case AssetType::Scene:
        return Json::object();
    }
    return Json::object();
}

ScanReport AssetRegistry::scan() {
    byId_.clear();
    byPath_.clear();
    ScanReport report;
    std::error_code ec;
    if (!fs::is_directory(root_, ec)) {
        ENGINE_ERROR("Asset folder not found: ", root_);
        return report;
    }

    // Collect and sort first, so ids are handed out in the same order on
    // every machine (directory order differs between file systems).
    std::vector<std::string> files;
    std::vector<std::string> metaFiles;
    for (auto it = fs::recursive_directory_iterator(root_, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) {
            ENGINE_WARN("While scanning ", root_, ": ", ec.message());
            break;
        }
        if (!it->is_regular_file()) {
            continue;
        }
        const fs::path relative = fs::relative(it->path(), root_);
        if (isHidden(relative)) {
            continue;
        }
        const std::string rel = relative.generic_string(); // forward slashes on every OS
        (endsWith(rel, MetaExtension) ? metaFiles : files).push_back(rel);
    }
    std::sort(files.begin(), files.end());

    for (const std::string& rel : files) {
        const auto type = typeForPath(rel);
        if (!type) {
            ++report.ignoredFiles;
            continue;
        }
        AssetRecord record;
        const MetaResult result = loadOrCreateMeta(rel, *type, record);
        if (result == MetaResult::Unreadable) {
            ++report.unreadableMetaFiles;
            continue;
        }
        if (result == MetaResult::Created) {
            ++report.metaFilesCreated;
        }
        if (byId_.count(record.id)) {
            // Two files claim one id: almost always a file copied together
            // with its .meta. The first (by path) keeps the id.
            ENGINE_WARN(rel, " has the same id as ", byId_.at(record.id).path,
                        " (copied .meta file?). Giving it a new id.");
            record.id = AssetId::generate();
            writeMeta(record);
            ++report.duplicateIds;
        }
        add(std::move(record));
        ++report.assets;
    }

    for (const std::string& meta : metaFiles) {
        const std::string assetPath = meta.substr(0, meta.size() - std::string(MetaExtension).size());
        if (!fs::exists(fs::path(root_) / assetPath)) {
            ENGINE_WARN("Orphan ", meta, ": its asset is gone. Scenes using its id will report it missing.");
            ++report.orphanMetaFiles;
        }
    }
    ENGINE_INFO("Assets: ", report.assets, " in ", root_, " (", report.metaFilesCreated, " new .meta files)");
    return report;
}

AssetRegistry::MetaResult AssetRegistry::loadOrCreateMeta(const std::string& rel, AssetType type, AssetRecord& out) {
    out.path = rel;
    out.type = type;
    const std::string metaPath = (fs::path(root_) / (rel + MetaExtension)).string();

    std::string text;
    if (!readTextFile(metaPath, text)) {
        out.id = AssetId::generate();
        out.settings = defaultSettings(type);
        writeMeta(out);
        return MetaResult::Created;
    }

    // A .meta exists but can't be used: say so and leave it alone. Writing a
    // fresh one would give the asset a new id and silently break every scene
    // that uses the old one.
    try {
        const Json meta = Json::parse(text);
        const auto id = AssetId::parse(readString(meta, "guid", ""));
        if (!id || id->isNull()) {
            ENGINE_ERROR(metaPath, ": missing or malformed \"guid\". Fix or delete the file; ", rel, " is skipped.");
            return MetaResult::Unreadable;
        }
        out.id = *id;
        const std::string typeName = readString(meta, "type", AssetRegistry::typeName(type));
        if (typeFromName(typeName) != type) {
            ENGINE_WARN(metaPath, " says type \"", typeName, "\", but the file looks like a ",
                        AssetRegistry::typeName(type), "; using ", AssetRegistry::typeName(type));
        }
        // Start from the defaults so settings added in newer versions get a value.
        out.settings = defaultSettings(type);
        if (meta.contains("settings") && meta["settings"].is_object()) {
            out.settings.update(meta["settings"]);
        }
    } catch (const std::exception& error) {
        ENGINE_ERROR(metaPath, " isn't valid: ", error.what(), ". Fix or delete it; ", rel, " is skipped.");
        return MetaResult::Unreadable;
    }
    return MetaResult::Read;
}

void AssetRegistry::writeMeta(const AssetRecord& record) const {
    const Json meta = {{"guid", record.id.toString()}, {"type", typeName(record.type)}, {"settings", record.settings}};
    const fs::path path = fs::path(root_) / (record.path + MetaExtension);
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        ENGINE_ERROR("Could not write ", path.string(), "; ", record.path, " will get a new id next time");
        return;
    }
    file << prettyJson(meta); // indented: these files get read in code review
}

void AssetRegistry::add(AssetRecord record) {
    byPath_[record.path] = record.id;
    const AssetId id = record.id;
    byId_[id] = std::move(record);
}

AssetId AssetRegistry::registerFile(const std::string& relativePath) {
    const auto type = typeForPath(relativePath);
    if (!type) {
        return {};
    }
    if (const AssetRecord* existing = findByPath(relativePath)) {
        return existing->id;
    }
    AssetRecord record;
    if (loadOrCreateMeta(relativePath, *type, record) == MetaResult::Unreadable) {
        return {};
    }
    const AssetId id = record.id;
    add(std::move(record));
    return id;
}

const AssetRecord* AssetRegistry::find(AssetId id) const {
    const auto it = byId_.find(id);
    return it == byId_.end() ? nullptr : &it->second;
}

const AssetRecord* AssetRegistry::findByPath(const std::string& relativePath) const {
    const auto it = byPath_.find(relativePath);
    return it == byPath_.end() ? nullptr : find(it->second);
}

AssetId AssetRegistry::idForPath(const std::string& relativePath) const {
    const AssetRecord* record = findByPath(relativePath);
    return record ? record->id : AssetId{};
}

std::string AssetRegistry::absolutePath(const AssetRecord& record) const {
    return (fs::path(root_) / record.path).string();
}

std::vector<const AssetRecord*> AssetRegistry::all() const {
    std::vector<const AssetRecord*> result;
    result.reserve(byId_.size());
    for (const auto& entry : byId_) {
        result.push_back(&entry.second);
    }
    std::sort(result.begin(), result.end(), [](const AssetRecord* a, const AssetRecord* b) { return a->path < b->path; });
    return result;
}

} // namespace Engine
