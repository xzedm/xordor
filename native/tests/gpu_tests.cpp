// GPU tests: draw with the real renderer and check the pixels that come out.
//
// They need an OpenGL 4.1 driver and a display (a hidden window is opened), so
// they live in their own program, gpu_tests, which ctest only runs when
// configured with -DXE_GPU_TESTS=ON. On a Linux server without a screen, run
// them under a virtual display: xvfb-run ctest --test-dir build.
//
// Each test draws into an offscreen framebuffer rather than the window, because
// the contents of a hidden window's own framebuffer are undefined.

#include "assets/AssetManager.h"
#include "assets/AssetRegistry.h"
#include "core/Paths.h"
#include "core/Window.h"
#include "render/Camera.h"
#include "render/ForwardRenderer.h"
#include "render/GLObject.h"
#include "render/Mesh.h"
#include "render/Primitives.h"
#include "render/Shader.h"
#include "render/Texture.h"
#include "scene/Scene.h"
#include "scene/SceneRenderer.h"

#include <doctest/doctest.h>
#include <glad/gl.h>

#include <array>
#include <cstdint>
#include <initializer_list>
#include <memory>

using namespace Engine;

namespace {

constexpr int Size = 128;

using Pixel = std::array<int, 4>;

WindowSettings hiddenWindowSettings() {
    WindowSettings settings;
    settings.title = "Xordor GPU tests";
    settings.width = Size;
    settings.height = Size;
    settings.visible = false;
    settings.vsync = false;
    return settings;
}

// A hidden window (for the OpenGL context) plus an offscreen color and depth
// target to draw into.
class GpuFixture {
    // Declared first, so it is created first and destroyed last: every other
    // GPU object below must be deleted while its context still exists.
    Window window_{hiddenWindowSettings()};
    GLFramebuffer framebuffer_;
    GLRenderbuffer color_;
    GLRenderbuffer depth_;

public:
    std::unique_ptr<ForwardRenderer> renderer;
    Camera camera;
    DirectionalLight light;
    Vec3 clear{0.25f, 0.5f, 0.75f};

    GpuFixture() {
        GLuint ids[3] = {};
        glGenFramebuffers(1, &ids[0]);
        glGenRenderbuffers(2, &ids[1]);
        framebuffer_ = GLFramebuffer(ids[0]);
        color_ = GLRenderbuffer(ids[1]);
        depth_ = GLRenderbuffer(ids[2]);

        glBindRenderbuffer(GL_RENDERBUFFER, color_.id());
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, Size, Size);
        glBindRenderbuffer(GL_RENDERBUFFER, depth_.id());
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, Size, Size);

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_.id());
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color_.id());
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_.id());
        REQUIRE(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

        renderer = std::make_unique<ForwardRenderer>();
        renderer->setViewport(Size, Size);
        camera.position = Vec3(0.0f, 0.0f, 3.0f);
        camera.aspect = 1.0f;
        // Light from behind the camera and no ambient: easy to reason about.
        light.direction = Vec3(0.0f, 0.0f, -1.0f);
        light.color = Vec3(1.0f);
        light.skyAmbient = Vec3(0.0f);
        light.groundAmbient = Vec3(0.0f);
    }

    ~GpuFixture() {
        // Public members are destroyed before the private ones above, but
        // being explicit about the renderer costs nothing.
        renderer.reset();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // (x, y) with y = 0 at the bottom, as OpenGL counts.
    Pixel pixel(int x, int y) const {
        std::array<std::uint8_t, 4> rgba{};
        glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        return {rgba[0], rgba[1], rgba[2], rgba[3]};
    }
    Pixel center() const { return pixel(Size / 2, Size / 2); }
};

// A stand-in for the asset manager: hands out one mesh for any id. The scene
// renderer only knows the RenderAssets interface, so tests need no files.
class OneMeshAssets final : public RenderAssets {
public:
    explicit OneMeshAssets(const Mesh& mesh) : mesh_(mesh) {}
    const Mesh* mesh(AssetId id) override { return id.isNull() ? nullptr : &mesh_; }
    const Texture* texture(AssetId) override { return nullptr; }

private:
    const Mesh& mesh_;
};

int brightness(const Pixel& p) {
    return p[0] + p[1] + p[2];
}

} // namespace

TEST_SUITE("gpu") {
    TEST_CASE_FIXTURE(GpuFixture, "an empty frame is just the clear color") {
        renderer->beginFrame(camera, light, clear);
        renderer->endFrame();
        const Pixel p = center();
        CHECK(p[0] == doctest::Approx(64).epsilon(0.02));
        CHECK(p[1] == doctest::Approx(128).epsilon(0.02));
        CHECK(p[2] == doctest::Approx(191).epsilon(0.02));
        CHECK(renderer->stats().drawCalls == 0);
    }

    TEST_CASE_FIXTURE(GpuFixture, "a lit cube covers the center and leaves the corners alone") {
        const Mesh cube(Primitives::cube());
        renderer->beginFrame(camera, light, clear);
        renderer->submit(cube, Material{}, Mat4(1.0f));
        renderer->endFrame();

        // The front face looks straight at the light: white times full diffuse, plus a highlight.
        const Pixel middle = center();
        CHECK(middle[0] > 230);
        CHECK(middle[1] > 230);
        CHECK(middle[2] > 230);
        const Pixel corner = pixel(2, 2);
        CHECK(corner[0] == doctest::Approx(64).epsilon(0.02));
        CHECK(renderer->stats().drawCalls == 1);
        CHECK(renderer->stats().triangles == 12);
    }

    TEST_CASE_FIXTURE(GpuFixture, "the side of a sphere facing the light is brighter") {
        const Mesh sphere(Primitives::sphere(0.5f, 32, 16));
        camera.position = Vec3(0.0f, 0.0f, 1.5f);
        light.direction = Vec3(-1.0f, 0.0f, 0.0f); // travels toward -X: lights the +X (right) side
        light.skyAmbient = light.groundAmbient = Vec3(0.05f);
        renderer->beginFrame(camera, light, clear);
        renderer->submit(sphere, Material{}, Mat4(1.0f));
        renderer->endFrame();

        const Pixel right = pixel(Size / 2 + 20, Size / 2);
        const Pixel left = pixel(Size / 2 - 20, Size / 2);
        CHECK(brightness(right) > brightness(left) + 200);
        CHECK(brightness(left) > 0); // ambient keeps the dark side from being pure black
    }

    TEST_CASE_FIXTURE(GpuFixture, "the depth test keeps the nearer object in front, whatever the draw order") {
        const Mesh cube(Primitives::cube());
        Material red;
        red.baseColor = Vec3(1.0f, 0.0f, 0.0f);
        red.specular = 0.0f;
        Material green;
        green.baseColor = Vec3(0.0f, 1.0f, 0.0f);
        green.specular = 0.0f;
        const Mat4 nearCube = Mat4(1.0f); // (not "near": that is a macro on Windows)
        const Mat4 farCube = Math::composeTRS(Vec3(0.0f, 0.0f, -2.0f), Quat(1, 0, 0, 0), Vec3(2.0f));

        for (bool nearFirst : {true, false}) {
            CAPTURE(nearFirst);
            renderer->beginFrame(camera, light, clear);
            if (nearFirst) {
                renderer->submit(cube, red, nearCube);
                renderer->submit(cube, green, farCube);
            } else {
                renderer->submit(cube, green, farCube);
                renderer->submit(cube, red, nearCube);
            }
            renderer->endFrame();
            const Pixel p = center();
            CHECK(p[0] > 200);
            CHECK(p[1] < 20);
        }
    }

    TEST_CASE_FIXTURE(GpuFixture, "a texture colors the surface") {
        const Mesh cube(Primitives::cube());
        const Texture blue = Texture::solidColor(0, 0, 255);
        Material material;
        material.albedo = &blue;
        material.specular = 0.0f;
        renderer->beginFrame(camera, light, clear);
        renderer->submit(cube, material, Mat4(1.0f));
        renderer->endFrame();
        const Pixel p = center();
        CHECK(p[0] < 20);
        CHECK(p[1] < 20);
        CHECK(p[2] > 230);
    }

    TEST_CASE_FIXTURE(GpuFixture, "back faces are culled: a cube seen from inside is invisible") {
        const Mesh cube(Primitives::cube());
        camera.position = Vec3(0.0f); // inside the 1 m cube
        renderer->beginFrame(camera, light, clear);
        renderer->submit(cube, Material{}, Mat4(1.0f));
        renderer->endFrame();
        CHECK(center()[0] == doctest::Approx(64).epsilon(0.02));
    }

    TEST_CASE_FIXTURE(GpuFixture, "a shader with an error is reported, not thrown, with the failing line") {
        const std::string vertex = "#version 410 core\n"
                                   "layout(location = 0) in vec3 aPosition;\n"
                                   "void main() { gl_Position = vec4(aPosition, 1.0); }\n";
        const std::string brokenFragment = "#version 410 core\n"
                                           "out vec4 color;\n"
                                           "void main() { color = vec4(notDeclared); }\n";
        std::string error;
        const auto shader = Shader::fromSource("broken", vertex, brokenFragment, &error);
        CHECK_FALSE(shader.has_value());
        CHECK(error.find("broken (fragment shader) failed to compile") != std::string::npos);
        // The annotation repeats the failing line (exact wording varies by driver).
        CHECK(error.find("3 | void main() { color = vec4(notDeclared); }") != std::string::npos);
    }

    TEST_CASE_FIXTURE(GpuFixture, "a scene draws its mesh entities through its camera entity") {
        const Mesh cube(Primitives::cube());
        Scene scene;
        scene.environment.clearColor = clear;
        const Entity cam = scene.createEntity("Camera");
        scene.add<CameraComponent>(cam);
        scene.get<Transform>(cam).position = Vec3(0.0f, 0.0f, 3.0f);
        const Entity sun = scene.createEntity("Sun");
        scene.add<LightComponent>(sun).color = Vec3(1.0f); // default rotation: shines down -Z, at the cube's front

        // A red cube as the child of a parent moved 0.6 m right.
        const Entity parent = scene.createEntity("Parent");
        scene.get<Transform>(parent).position = Vec3(0.6f, 0.0f, 0.0f);
        const Entity box = scene.createEntity("Box");
        MeshRenderer& mr = scene.add<MeshRenderer>(box);
        mr.mesh = BuiltinAssets::Cube;
        mr.material.baseColor = Vec3(1.0f, 0.0f, 0.0f);
        mr.material.specular = 0.0f;
        scene.setParent(box, parent, false);

        OneMeshAssets assets(cube);
        SceneRenderer sceneRenderer(*renderer);
        sceneRenderer.render(scene, assets, 1.0f, 1.0f);
        CHECK(renderer->stats().drawCalls == 1);
        // The cube sits right of center, so the right side is red and the far left is background.
        const Pixel right = pixel(Size / 2 + 25, Size / 2);
        CHECK(right[0] > 200);
        CHECK(right[1] < 30);
        CHECK(pixel(8, Size / 2)[0] == doctest::Approx(64).epsilon(0.02));

        // Hidden entities aren't drawn; colliders add wireframe draws when shown.
        mr.visible = false;
        scene.add<Collider>(box);
        sceneRenderer.showColliders = true;
        sceneRenderer.render(scene, assets, 1.0f, 1.0f);
        CHECK(renderer->stats().drawCalls == 1);
    }

    TEST_CASE_FIXTURE(GpuFixture, "the asset manager loads each asset once and shares it") {
        AssetRegistry registry(assetDirectory());
        registry.scan();
        AssetManager assets(registry);
        const AssetId barrel = registry.idForPath("models/barrel.obj");
        const AssetId crate = registry.idForPath("textures/crate.png");
        REQUIRE_FALSE(barrel.isNull());
        const Mesh* first = assets.mesh(barrel);
        REQUIRE(first);
        CHECK(assets.mesh(barrel) == first); // cached: the same object
        CHECK(assets.mesh(BuiltinAssets::Cube) != nullptr);
        const Texture* texture = assets.texture(crate);
        REQUIRE(texture);
        CHECK(texture->width() == 128);
        CHECK(assets.loadedMeshes() == 2);

        // Unknown or wrong-type ids give nullptr (and are logged), every time.
        CHECK(assets.mesh(AssetId::generate()) == nullptr);
        CHECK(assets.texture(barrel) == nullptr); // a mesh isn't a texture
        CHECK(assets.mesh(AssetId{}) == nullptr);
    }
}
