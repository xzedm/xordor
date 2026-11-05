// Tests for saving and loading scenes as JSON: everything saved comes back,
// files are readable, and bad files are rejected without damaging anything.

#include "assets/SceneSerializer.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>

using namespace Engine;

namespace {

struct Spin {
    float degreesPerSecond = 0.0f;
};

Scene makeScene() {
    Scene scene;
    scene.environment.clearColor = Vec3(0.1f, 0.2f, 0.3f);

    const Entity cam = scene.createEntity("Main Camera");
    scene.add<CameraComponent>(cam).fovY = Math::toRadians(75.0f);
    scene.get<Transform>(cam).position = Vec3(0.0f, 2.0f, 8.0f);

    const Entity sun = scene.createEntity("Sun");
    scene.add<LightComponent>(sun).intensity = 1.5f;
    scene.get<Transform>(sun).rotation = Math::quatFromEulerDegrees(Vec3(-50.0f, 35.0f, 0.0f));

    const Entity group = scene.createEntity("Crates");
    for (int i = 0; i < 2; ++i) {
        const Entity crate = scene.createEntity("Crate " + std::to_string(i));
        scene.setParent(crate, group, false);
        scene.get<Transform>(crate).position = Vec3(static_cast<float>(i) * 2.0f, 0.5f, -1.0f);
        MeshRenderer& mr = scene.add<MeshRenderer>(crate);
        mr.mesh = BuiltinAssets::Cube;
        mr.material.albedo = *AssetId::parse("3f2b8c1e-9d4a-4f6b-8e21-c0ffee000001");
        mr.material.baseColor = Vec3(1.0f, 0.5f, 0.25f);
        mr.material.uvScale = Vec2(2.0f, 3.0f);
        Collider& c = scene.add<Collider>(crate);
        c.shape = i == 0 ? ColliderShape::Box : ColliderShape::Sphere;
        c.radius = 0.75f;
    }
    return scene;
}

} // namespace

TEST_SUITE("serializer") {
    TEST_CASE("save then load gives back the same scene") {
        const Scene original = makeScene();
        const SceneSerializer serializer;
        Scene loaded;
        std::string error;
        REQUIRE_MESSAGE(serializer.fromJson(serializer.toJson(original), loaded, &error), error);

        CHECK(loaded.entityCount() == original.entityCount());
        CHECK(Math::nearlyEqual(loaded.environment.clearColor, Vec3(0.1f, 0.2f, 0.3f)));

        const Entity cam = loaded.findByName("Main Camera");
        REQUIRE_FALSE(cam.isNull());
        CHECK(loaded.get<CameraComponent>(cam).fovY == doctest::Approx(Math::toRadians(75.0f)));
        CHECK(Math::nearlyEqual(loaded.get<Transform>(cam).position, Vec3(0.0f, 2.0f, 8.0f)));
        CHECK(loaded.primaryCamera() == cam);

        const Entity sun = loaded.findByName("Sun");
        CHECK(loaded.get<LightComponent>(sun).intensity == 1.5f);
        CHECK(Math::nearlyEqual(loaded.get<Transform>(sun).rotation,
                                original.get<Transform>(original.findByName("Sun")).rotation, 1e-5f));

        const Entity crate1 = loaded.findByName("Crate 1");
        CHECK(loaded.parentOf(crate1) == loaded.findByName("Crates"));
        CHECK(Math::nearlyEqual(loaded.worldPosition(crate1), Vec3(2.0f, 0.5f, -1.0f)));
        const MeshRenderer& mr = loaded.get<MeshRenderer>(crate1);
        CHECK(mr.mesh == BuiltinAssets::Cube);
        CHECK(mr.material.albedo.toString() == "3f2b8c1e-9d4a-4f6b-8e21-c0ffee000001");
        CHECK(Math::nearlyEqual(mr.material.baseColor, Vec3(1.0f, 0.5f, 0.25f)));
        CHECK(mr.material.uvScale.y == 3.0f);
        CHECK(loaded.get<Collider>(crate1).shape == ColliderShape::Sphere);
        CHECK(loaded.get<Collider>(crate1).radius == 0.75f);
        CHECK(loaded.get<Collider>(loaded.findByName("Crate 0")).shape == ColliderShape::Box);
    }

    TEST_CASE("the file is readable: ordered keys, tidy numbers, Euler degrees, asset paths") {
        Scene scene;
        const Entity e = scene.createEntity("Turned");
        scene.get<Transform>(e).rotation = Math::quatFromEulerDegrees(Vec3(0.0f, 90.0f, 0.0f));
        scene.get<Transform>(e).position = Vec3(0.3f, 0.0f, 0.0f);
        scene.add<MeshRenderer>(e).mesh = BuiltinAssets::Sphere;
        const std::string text = SceneSerializer().toJson(scene).dump(2);
        CHECK(text.find("\"format\": \"xordor-scene\"") != std::string::npos);
        CHECK(text.find("0.3,") != std::string::npos);        // not 0.30000001192092896
        CHECK(text.find("90.0") != std::string::npos);        // the yaw, in degrees
        CHECK(text.find("\"path\": \"builtin:sphere\"") != std::string::npos);
        CHECK(text.find("\"id\": 1") < text.find("\"name\"")); // id comes first
    }

    TEST_CASE("custom components are saved once registered, and reported if not") {
        Scene scene;
        const Entity e = scene.createEntity("Spinner");
        scene.add<Spin>(e).degreesPerSecond = 45.0f;

        SceneSerializer withSpin;
        withSpin.registerComponent<Spin>(
            "spin", [](const Spin& s) { return Json{{"degreesPerSecond", s.degreesPerSecond}}; },
            [](Spin& s, const Json& j) { s.degreesPerSecond = readFloat(j, "degreesPerSecond", 0.0f); });
        const Json doc = withSpin.toJson(scene);
        CHECK(doc["entities"][0].contains("spin"));

        Scene loaded;
        REQUIRE(withSpin.fromJson(doc, loaded));
        CHECK(loaded.get<Spin>(loaded.findByName("Spinner")).degreesPerSecond == 45.0f);

        // A serializer that doesn't know "spin" still loads the rest (and logs the skip).
        Scene partial;
        REQUIRE(SceneSerializer().fromJson(doc, partial));
        CHECK(partial.has<Transform>(partial.findByName("Spinner")));
        CHECK_FALSE(partial.has<Spin>(partial.findByName("Spinner")));
    }

    TEST_CASE("a child listed before its parent still loads (hand-edited files)") {
        const Json doc = Json::parse(R"({
            "format": "xordor-scene", "version": 1,
            "entities": [
                {"id": 2, "name": "Child", "parent": 1, "transform": {"position": [1, 0, 0]}},
                {"id": 1, "name": "Parent", "transform": {"position": [0, 5, 0]}}
            ]})");
        Scene scene;
        REQUIRE(SceneSerializer().fromJson(doc, scene));
        CHECK(Math::nearlyEqual(scene.worldPosition(scene.findByName("Child")), Vec3(1, 5, 0)));
    }

    TEST_CASE("missing fields take their defaults") {
        const Json doc = Json::parse(R"({"format": "xordor-scene", "version": 1,
            "entities": [{"id": 1, "name": "Bare", "light": {}, "collider": {"shape": "sphere"}}]})");
        Scene scene;
        REQUIRE(SceneSerializer().fromJson(doc, scene));
        const Entity e = scene.findByName("Bare");
        CHECK(scene.get<LightComponent>(e).intensity == 1.0f);
        CHECK(scene.get<Collider>(e).radius == 0.5f);
        CHECK(Math::nearlyEqual(scene.get<Transform>(e).scale, Vec3(1.0f)));
    }

    TEST_CASE("bad files are rejected with a message, and the scene is left untouched") {
        Scene scene = makeScene();
        const std::size_t before = scene.entityCount();
        const SceneSerializer serializer;
        std::string error;

        CHECK_FALSE(serializer.fromJson(Json::parse(R"({"format": "something else"})"), scene, &error));
        CHECK(error.find("not a scene file") != std::string::npos);

        CHECK_FALSE(serializer.fromJson(Json::parse(R"({"format": "xordor-scene", "version": 99, "entities": []})"), scene, &error));
        CHECK(error.find("version 99") != std::string::npos);

        // An error deep inside: entity 2's collider has a wrong shape.
        CHECK_FALSE(serializer.fromJson(Json::parse(R"({"format": "xordor-scene", "version": 1, "entities": [
            {"id": 1, "name": "Fine"},
            {"id": 2, "name": "Broken", "collider": {"shape": "cone"}}]})"),
                                        scene, &error));
        CHECK(error.find("entity 2 (\"Broken\"), \"collider\"") != std::string::npos);
        CHECK(error.find("cone") != std::string::npos);

        CHECK_FALSE(serializer.fromJson(Json::parse(R"({"format": "xordor-scene", "version": 1, "entities": [
            {"id": 1, "name": "A"}, {"id": 1, "name": "B"}]})"),
                                        scene, &error));
        CHECK(error.find("unique") != std::string::npos);

        CHECK(scene.entityCount() == before); // nothing was half-loaded
    }

    TEST_CASE("files: save, load, and a syntax error names the line") {
        const std::string path = (std::filesystem::temp_directory_path() / "xordor_test.scene.json").string();
        const SceneSerializer serializer;
        std::string error;
        REQUIRE(serializer.save(makeScene(), path, &error));
        Scene loaded;
        REQUIRE_MESSAGE(serializer.load(path, loaded, &error), error);
        CHECK(loaded.entityCount() == 5);

        {
            std::ofstream broken(path, std::ios::binary);
            broken << "{\n  \"format\": \"xordor-scene\",\n  \"version\": 1,\n  oops\n}";
        }
        CHECK_FALSE(serializer.load(path, loaded, &error));
        CHECK(error.find("line 4") != std::string::npos);
        std::filesystem::remove(path);

        CHECK_FALSE(serializer.load("no/such.scene.json", loaded, &error));
        CHECK(error.find("Could not read") != std::string::npos);
    }
}
