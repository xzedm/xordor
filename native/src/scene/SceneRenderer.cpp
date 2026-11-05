#include "scene/SceneRenderer.h"

#include "core/Log.h"
#include "render/Primitives.h"
#include "scene/Collision.h"

namespace Engine {

CameraView cameraViewFor(const Scene& scene, Entity camera, float aspect, float alpha) {
    const Mat4 world = scene.interpolatedWorldMatrix(camera, alpha);
    // The world matrix's columns are the entity's axes in world space.
    // Normalizing them drops any scale, leaving just where it is and which way
    // it points. Inverting world directly would also invert the scale.
    const Vec3 position(world[3]);
    const Vec3 forward = -glm::normalize(Vec3(world[2])); // the entity looks down its -Z
    const Vec3 up = glm::normalize(Vec3(world[1]));

    CameraView view;
    view.position = position;
    view.view = Math::lookAt(position, position + forward, up);
    const CameraComponent* c = scene.tryGet<CameraComponent>(camera);
    const CameraComponent settings = c ? *c : CameraComponent{};
    view.projection = Math::perspective(settings.fovY, aspect, settings.nearPlane, settings.farPlane);
    return view;
}

DirectionalLight directionalLightFor(const Scene& scene, Entity light, float alpha) {
    DirectionalLight result;
    result.skyAmbient = scene.environment.skyAmbient;
    result.groundAmbient = scene.environment.groundAmbient;
    const LightComponent* l = scene.tryGet<LightComponent>(light);
    if (!l) {
        result.intensity = 0.0f; // no light: ambient only
        return result;
    }
    result.color = l->color;
    result.intensity = l->intensity;
    result.direction = -glm::normalize(Vec3(scene.interpolatedWorldMatrix(light, alpha)[2]));
    return result;
}

SceneRenderer::SceneRenderer(ForwardRenderer& renderer)
    : renderer_(renderer), debugCube_(Primitives::cube()), debugSphere_(Primitives::sphere(1.0f, 24, 12)) {}

void SceneRenderer::render(const Scene& scene, RenderAssets& assets, float aspect, float alpha) {
    const Entity camera = scene.primaryCamera();
    if (camera.isNull()) {
        if (!warnedNoCamera_) {
            ENGINE_WARN("The scene has no primary camera (CameraComponent), so nothing is drawn");
            warnedNoCamera_ = true;
        }
        // Still clear the screen, so the last frame doesn't stay up.
        renderer_.beginFrame(CameraView{}, DirectionalLight{}, scene.environment.clearColor);
        renderer_.endFrame();
        return;
    }

    Entity light;
    scene.registry().each<LightComponent>([&](Entity e, const LightComponent&) {
        if (light.isNull()) {
            light = e;
        }
    });
    if (light.isNull() && !warnedNoLight_) {
        ENGINE_WARN("The scene has no light (LightComponent); drawing with ambient light only");
        warnedNoLight_ = true;
    }

    renderer_.beginFrame(cameraViewFor(scene, camera, aspect, alpha), directionalLightFor(scene, light, alpha),
                         scene.environment.clearColor);
    scene.registry().each<MeshRenderer>([&](Entity e, const MeshRenderer& mr) {
        if (!mr.visible) {
            return;
        }
        const Mesh* mesh = assets.mesh(mr.mesh);
        if (!mesh) {
            // The asset manager already said why; this says which entity.
            if (warnedMissingMesh_.insert(e).second) {
                ENGINE_WARN(scene.nameOf(e), " isn't drawn: its mesh ", mr.mesh.toString(), " isn't available");
            }
            return;
        }
        Material material;
        material.baseColor = mr.material.baseColor;
        // A missing texture falls back to white (the asset manager logs it).
        material.albedo = mr.material.albedo.isNull() ? nullptr : assets.texture(mr.material.albedo);
        material.specular = mr.material.specular;
        material.shininess = mr.material.shininess;
        material.uvScale = mr.material.uvScale;
        renderer_.submit(*mesh, material, scene.interpolatedWorldMatrix(e, alpha));
    });
    if (showColliders) {
        submitColliders(scene);
    }
    renderer_.endFrame();
}

void SceneRenderer::submitColliders(const Scene& scene) {
    Material wire;
    wire.baseColor = colliderColor;
    wire.wireframe = true;
    wire.specular = 0.0f;
    // Drawn 2% larger: a collider usually matches its mesh exactly, and two
    // surfaces at the same depth flicker between each other ("z-fighting"),
    // which turns the lines into dashes. Slightly larger wins the depth test
    // cleanly while staying hidden behind other objects.
    const float inflate = 1.02f;
    // Colliders are drawn where the collision code sees them: the current
    // fixed-step state, not the blended one. Fast objects can look slightly
    // ahead of their mesh; that is the honest picture.
    scene.registry().each<Collider>([&](Entity e, const Collider& c) {
        const WorldCollider w = toWorld(c, scene.worldMatrix(e));
        if (w.shape == ColliderShape::Sphere) {
            renderer_.submit(debugSphere_, wire,
                             Math::composeTRS(w.sphere.center, Quat(1, 0, 0, 0), Vec3(w.sphere.radius * inflate)));
        } else {
            // The unit cube is 1 m wide, so scale by the full size (2 x half extents).
            renderer_.submit(debugCube_, wire,
                             Math::composeTRS(w.box.center(), Quat(1, 0, 0, 0), w.box.halfExtents() * 2.0f * inflate));
        }
    });
}

} // namespace Engine
