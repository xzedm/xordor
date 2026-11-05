// Tests for the model importers: the OBJ reader on small hand-written files,
// and both importers on the sample models in assets/models/.

#include "assets/JsonUtil.h"
#include "assets/ModelImport.h"
#include "core/Paths.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>

using namespace Engine;

namespace {

// The signed volume of a closed mesh: positive when every triangle is
// counter-clockwise seen from outside (the normals point out), negative when
// the mesh is inside out. Sums the volume of the tetrahedron each triangle
// makes with the origin.
float signedVolume(const MeshData& mesh) {
    double volume = 0.0;
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const Vec3& a = mesh.vertices[mesh.indices[i]].position;
        const Vec3& b = mesh.vertices[mesh.indices[i + 1]].position;
        const Vec3& c = mesh.vertices[mesh.indices[i + 2]].position;
        volume += glm::dot(a, glm::cross(b, c)) / 6.0;
    }
    return static_cast<float>(volume);
}

bool normalsMatchWinding(const MeshData& mesh) {
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const Vertex& a = mesh.vertices[mesh.indices[i]];
        const Vertex& b = mesh.vertices[mesh.indices[i + 1]];
        const Vertex& c = mesh.vertices[mesh.indices[i + 2]];
        const Vec3 face = glm::cross(b.position - a.position, c.position - a.position);
        if (glm::length(face) > 1e-8f && glm::dot(face, a.normal + b.normal + c.normal) <= 0.0f) {
            return false;
        }
    }
    return true;
}

bool allNormalsUnit(const MeshData& mesh) {
    for (const Vertex& v : mesh.vertices) {
        if (std::fabs(glm::length(v.normal) - 1.0f) > 1e-3f) return false;
    }
    return true;
}

const char* Triangle = "v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nvn 0 0 1\nf 1/1/1 2/2/1 3/3/1\n";

} // namespace

TEST_SUITE("models") {
    TEST_CASE("OBJ: one triangle with uvs and normals") {
        std::string error;
        const auto mesh = parseObj(Triangle, {}, &error);
        REQUIRE_MESSAGE(mesh, error);
        CHECK(mesh->vertices.size() == 3);
        CHECK(mesh->indices == std::vector<std::uint32_t>{0, 1, 2});
        CHECK(Math::nearlyEqual(mesh->vertices[1].position, Vec3(1, 0, 0)));
        CHECK(mesh->vertices[2].uv.y == 1.0f);
        CHECK(Math::nearlyEqual(mesh->vertices[0].normal, Vec3(0, 0, 1)));
    }

    TEST_CASE("OBJ: a quad becomes two triangles that share corners") {
        const auto mesh = parseObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf 1 2 3 4\n");
        REQUIRE(mesh);
        CHECK(mesh->vertices.size() == 4); // shared, not duplicated
        CHECK(mesh->indices == std::vector<std::uint32_t>{0, 1, 2, 0, 2, 3});
    }

    TEST_CASE("OBJ: the same position with different normals makes separate vertices") {
        // A cube corner is one position but three normals (one per face).
        const auto mesh = parseObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\nvn 0 0 -1\nvn 0 -1 0\n"
                                   "f 1//1 3//1 2//1\nf 1//2 2//2 4//2\n");
        REQUIRE(mesh);
        // Positions 1 and 2 appear with both normals (4 vertices), 3 and 4 with one each.
        CHECK(mesh->vertices.size() == 6);
    }

    TEST_CASE("OBJ: negative indices count back from the end") {
        const auto mesh = parseObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf -3 -2 -1\n");
        REQUIRE(mesh);
        CHECK(Math::nearlyEqual(mesh->vertices[2].position, Vec3(0, 1, 0)));
    }

    TEST_CASE("OBJ: missing normals are computed from the faces") {
        const auto mesh = parseObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
        REQUIRE(mesh);
        for (const Vertex& v : mesh->vertices) {
            CHECK(Math::nearlyEqual(v.normal, Vec3(0, 0, 1))); // counter-clockwise in XY faces +Z
        }
    }

    TEST_CASE("OBJ: scale, comments and unused lines") {
        ModelImportSettings settings;
        settings.scale = 0.01f; // centimeters to meters
        const auto mesh = parseObj("# a comment\nmtllib x.mtl\no Thing\nv 100 0 0 # trailing comment\nv 0 100 0\nv 0 0 100\n"
                                   "usemtl red\ns off\nf 1 2 3\n",
                                   settings);
        REQUIRE(mesh);
        CHECK(Math::nearlyEqual(mesh->vertices[0].position, Vec3(1, 0, 0)));
    }

    TEST_CASE("OBJ: errors name the line and the problem") {
        std::string error;
        CHECK_FALSE(parseObj("v 0 0 0\nv 1 0 0\nf 1 2 3\n", {}, &error, "bad.obj"));
        CHECK(error.find("bad.obj, line 3") != std::string::npos);
        CHECK(error.find("index 3") != std::string::npos);

        CHECK_FALSE(parseObj("v 0 zero 0\n", {}, &error));
        CHECK(error.find("line 1: expected 3 numbers") != std::string::npos);

        CHECK_FALSE(parseObj("v 0 0 0\nv 1 0 0\nf 1 2\n", {}, &error));
        CHECK(error.find("at least 3 corners") != std::string::npos);

        CHECK_FALSE(parseObj("# only a comment\n", {}, &error));
        CHECK(error.find("no faces") != std::string::npos);

        CHECK_FALSE(importObj("no/such/model.obj", {}, &error));
        CHECK(error.find("no/such/model.obj") != std::string::npos);
    }

    TEST_CASE("the sample barrel (OBJ) is a closed, outward-facing mesh") {
        std::string error;
        const auto barrel = importModel(assetPath("models/barrel.obj"), {}, &error);
        REQUIRE_MESSAGE(barrel, error);
        CHECK(barrel->triangleCount() > 100);
        CHECK(allNormalsUnit(*barrel));
        CHECK(normalsMatchWinding(*barrel));
        // Roughly a cylinder of radius ~0.37 m and height 1 m: about 0.43 m^3.
        CHECK(signedVolume(*barrel) == doctest::Approx(0.43f).epsilon(0.1));
    }

    TEST_CASE("the sample rock (glTF) imports with computed normals and its node scale") {
        std::string error;
        const auto rock = importModel(assetPath("models/rock.gltf"), {}, &error);
        REQUIRE_MESSAGE(rock, error);
        CHECK(rock->triangleCount() == 320);
        CHECK(allNormalsUnit(*rock));          // the file has no normals: computed
        CHECK(normalsMatchWinding(*rock));
        CHECK(signedVolume(*rock) > 0.1f);     // outward-facing
        // The node scales Y by 0.7: compare with the bounds the file itself lists.
        std::string text;
        REQUIRE(readTextFile(assetPath("models/rock.gltf"), text));
        const Json file = Json::parse(text);
        const float fileMaxY = file["accessors"][1]["max"][1].get<float>();
        float maxY = -1.0f;
        for (const Vertex& v : rock->vertices) {
            maxY = std::max(maxY, v.position.y);
            CHECK(v.uv.y >= 0.0f);
            CHECK(v.uv.y <= 1.0f);
        }
        CHECK(maxY == doctest::Approx(fileMaxY * 0.7f));

        ModelImportSettings half;
        half.scale = 0.5f;
        const auto small = importModel(assetPath("models/rock.gltf"), half);
        REQUIRE(small);
        CHECK(signedVolume(*small) == doctest::Approx(signedVolume(*rock) / 8.0f).epsilon(1e-3));
    }

    TEST_CASE("glTF and unknown formats report errors") {
        std::string error;
        CHECK_FALSE(importModel("no/such/rock.gltf", {}, &error));
        CHECK(error.find("no/such/rock.gltf") != std::string::npos);
        CHECK_FALSE(importModel("model.fbx", {}, &error));
        CHECK(error.find("unknown model format") != std::string::npos);
    }
}
