// Tests for asset ids, .meta files and the asset registry. The registry
// tests work in a fresh temporary folder, so they never touch the real assets.

#include "assets/AssetRegistry.h"
#include "assets/JsonUtil.h"
#include "core/AssetId.h"
#include "core/Paths.h"

#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <unordered_set>

namespace fs = std::filesystem;
using namespace Engine;

namespace {

// A temporary folder that deletes itself.
class TempFolder {
public:
    TempFolder() {
        path_ = fs::temp_directory_path() / ("xordor_assets_" + AssetId::generate().toString());
        fs::create_directories(path_);
    }
    ~TempFolder() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }
    std::string path() const { return path_.string(); }
    void write(const std::string& relative, const std::string& text) const {
        const fs::path p = path_ / relative;
        fs::create_directories(p.parent_path());
        std::ofstream(p, std::ios::binary) << text;
    }
    bool exists(const std::string& relative) const { return fs::exists(path_ / relative); }
    std::string read(const std::string& relative) const {
        std::string text;
        readTextFile((path_ / relative).string(), text);
        return text;
    }
    void rename(const std::string& from, const std::string& to) const { fs::rename(path_ / from, path_ / to); }

private:
    fs::path path_;
};

} // namespace

TEST_SUITE("assets") {
    TEST_CASE("asset ids: text form round trip") {
        const AssetId id = AssetId::generate();
        const std::string text = id.toString();
        CHECK(text.size() == 36);
        CHECK(text[14] == '4'); // version 4: random
        CHECK(AssetId::parse(text) == id);
        CHECK(AssetId::parse("3F2B8C1E-9D4A-4F6B-8E21-C0FFEE000001")->toString() == "3f2b8c1e-9d4a-4f6b-8e21-c0ffee000001");
    }

    TEST_CASE("asset ids: malformed text is rejected") {
        CHECK_FALSE(AssetId::parse(""));
        CHECK_FALSE(AssetId::parse("3f2b8c1e9d4a4f6b8e21c0ffee000001"));       // no dashes
        CHECK_FALSE(AssetId::parse("3f2b8c1e-9d4a-4f6b-8e21-c0ffee00000g"));   // not hex
        CHECK_FALSE(AssetId::parse("3f2b8c1e-9d4a-4f6b-8e21-c0ffee0000011"));  // too long
    }

    TEST_CASE("asset ids: generated ids are unique and never null or built-in") {
        std::unordered_set<AssetId> seen;
        for (int i = 0; i < 10000; ++i) {
            const AssetId id = AssetId::generate();
            CHECK_FALSE(id.isNull());
            CHECK(id != BuiltinAssets::Cube);
            seen.insert(id);
        }
        CHECK(seen.size() == 10000);
        CHECK(AssetId{}.isNull());
    }

    TEST_CASE("file types come from the extension") {
        CHECK(AssetRegistry::typeForPath("textures/Crate.PNG") == AssetType::Texture);
        CHECK(AssetRegistry::typeForPath("a/b.jpeg") == AssetType::Texture);
        CHECK(AssetRegistry::typeForPath("models/barrel.obj") == AssetType::Mesh);
        CHECK(AssetRegistry::typeForPath("models/rock.glb") == AssetType::Mesh);
        CHECK(AssetRegistry::typeForPath("scenes/level.scene.json") == AssetType::Scene);
        CHECK_FALSE(AssetRegistry::typeForPath("shaders/lit.frag"));
        CHECK_FALSE(AssetRegistry::typeForPath("notes.json"));
    }

    TEST_CASE("scanning creates .meta files once, and ids survive a rescan") {
        TempFolder folder;
        folder.write("textures/a.png", "not really a png; the registry doesn't open it");
        folder.write("models/b.obj", "v 0 0 0");
        folder.write("readme.txt", "ignored");
        AssetRegistry registry(folder.path());

        const ScanReport first = registry.scan();
        CHECK(first.assets == 2);
        CHECK(first.metaFilesCreated == 2);
        CHECK(first.ignoredFiles == 1);
        CHECK(folder.exists("textures/a.png.meta"));
        CHECK_FALSE(folder.exists("readme.txt.meta"));
        const AssetId a = registry.idForPath("textures/a.png");
        CHECK_FALSE(a.isNull());
        CHECK(registry.find(a)->type == AssetType::Texture);
        CHECK(registry.find(a)->settings["colorSpace"] == "srgb"); // defaults written into the new .meta

        const ScanReport second = registry.scan();
        CHECK(second.metaFilesCreated == 0);
        CHECK(registry.idForPath("textures/a.png") == a);
    }

    TEST_CASE("renaming a file together with its .meta keeps its id") {
        TempFolder folder;
        folder.write("crate.png", "x");
        AssetRegistry registry(folder.path());
        registry.scan();
        const AssetId id = registry.idForPath("crate.png");

        fs::create_directories(fs::path(folder.path()) / "props");
        folder.rename("crate.png", "props/wooden_crate.png");
        folder.rename("crate.png.meta", "props/wooden_crate.png.meta");
        registry.scan();
        CHECK(registry.idForPath("props/wooden_crate.png") == id);
        CHECK(registry.find(id)->path == "props/wooden_crate.png");
        CHECK(registry.idForPath("crate.png").isNull());
    }

    TEST_CASE("a copied .meta gets a new id; a lone .meta is reported") {
        TempFolder folder;
        folder.write("a.png", "x");
        AssetRegistry registry(folder.path());
        registry.scan();
        const AssetId original = registry.idForPath("a.png");

        folder.write("b.png", "x");
        folder.write("b.png.meta", folder.read("a.png.meta")); // copy-paste in a file browser
        folder.write("gone.png.meta", folder.read("a.png.meta"));
        const ScanReport report = registry.scan();
        CHECK(report.duplicateIds == 1);
        CHECK(report.orphanMetaFiles == 1);
        CHECK(registry.idForPath("a.png") == original); // first by path keeps it
        CHECK(registry.idForPath("b.png") != original);
        // The fix was written to disk, so the next scan agrees.
        registry.scan();
        CHECK(registry.idForPath("b.png") != original);
    }

    TEST_CASE("a broken .meta is reported and left alone, not replaced") {
        TempFolder folder;
        folder.write("a.png", "x");
        folder.write("a.png.meta", "{ this is not json");
        AssetRegistry registry(folder.path());
        const ScanReport report = registry.scan();
        CHECK(report.unreadableMetaFiles == 1);
        CHECK(report.assets == 0);
        // Rewriting it would give the asset a new id and break scenes that use the old one.
        CHECK(folder.read("a.png.meta") == "{ this is not json");
    }

    TEST_CASE("settings from the .meta override the defaults; missing ones are filled in") {
        TempFolder folder;
        folder.write("normal.png", "x");
        folder.write("normal.png.meta",
                     R"({"guid": "3f2b8c1e-9d4a-4f6b-8e21-c0ffee000001", "type": "texture", "settings": {"colorSpace": "linear"}})");
        AssetRegistry registry(folder.path());
        registry.scan();
        const AssetRecord* record = registry.findByPath("normal.png");
        REQUIRE(record);
        CHECK(record->id.toString() == "3f2b8c1e-9d4a-4f6b-8e21-c0ffee000001");
        CHECK(record->settings["colorSpace"] == "linear");
        CHECK(record->settings["mipmaps"] == true);
    }

    TEST_CASE("registerFile adds a single new file, such as a saved scene") {
        TempFolder folder;
        AssetRegistry registry(folder.path());
        registry.scan();
        folder.write("level.scene.json", "{}");
        const AssetId id = registry.registerFile("level.scene.json");
        CHECK_FALSE(id.isNull());
        CHECK(registry.find(id)->type == AssetType::Scene);
        CHECK(registry.registerFile("level.scene.json") == id);
        CHECK(registry.registerFile("notes.txt").isNull());
    }

    TEST_CASE("the shipped assets all have committed .meta files") {
        // If this fails, a new asset was added without its .meta: run the
        // sandbox once (it creates them) and commit the new .meta files.
        AssetRegistry registry(assetDirectory());
        const ScanReport report = registry.scan();
        CHECK(report.assets >= 5);
        CHECK(report.metaFilesCreated == 0);
        CHECK(report.duplicateIds == 0);
        CHECK(report.orphanMetaFiles == 0);
    }

    TEST_CASE("JSON readers: defaults when missing, clear errors when wrong") {
        const Json j = Json::parse(R"({"n": 2.5, "flag": true, "v": [1, 2, 3], "bad": "x", "short": [1, 2]})");
        CHECK(readFloat(j, "n", 0.0f) == 2.5f);
        CHECK(readFloat(j, "missing", 7.0f) == 7.0f);
        CHECK(readBool(j, "flag", false));
        CHECK(Math::nearlyEqual(readVec3(j, "v", Vec3(0.0f)), Vec3(1, 2, 3)));
        CHECK(Math::nearlyEqual(readVec3(j, "missing", Vec3(4.0f)), Vec3(4.0f)));
        CHECK_THROWS_WITH_AS(readFloat(j, "bad", 0.0f), "\"bad\" should be a number, but is \"x\"", JsonError);
        CHECK_THROWS_AS(readVec3(j, "short", Vec3(0.0f)), JsonError);
        CHECK_THROWS_AS(readAssetId(j, "bad"), JsonError);
    }

    TEST_CASE("prettyJson keeps short arrays on one line and round-trips") {
        const Json value = Json::parse(R"({"a": [1.5, 2, 3], "b": {"c": []}, "d": [{"e": true}], "f": "text"})");
        const std::string text = prettyJson(value);
        CHECK(text.find("\"a\": [1.5, 2, 3]") != std::string::npos);
        CHECK(text.find("\"c\": []") != std::string::npos);
        CHECK(text.find("\"d\": [\n") != std::string::npos); // arrays of objects are spread out
        CHECK(Json::parse(text) == value);
        CHECK(tidy(0.3f) == 0.3);
        CHECK(tidy(35.0000038f, 4) == 35.0);
        CHECK(std::signbit(tidy(-0.0000001f)) == false); // no "-0.0" in files
    }
}
