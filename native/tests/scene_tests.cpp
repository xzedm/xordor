// Tests for the scene: the parent-child hierarchy, world matrices, transform
// interpolation, cameras and lights from entities, and collision queries.

#include "scene/Collision.h"
#include "scene/Scene.h"
#include "scene/SceneRenderer.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>

using namespace Engine;

namespace {

bool contains(const std::vector<Entity>& list, Entity e) {
    return std::find(list.begin(), list.end(), e) != list.end();
}

} // namespace

TEST_SUITE("scene") {
    TEST_CASE("createEntity gives a name, a transform and a hierarchy") {
        Scene scene;
        const Entity e = scene.createEntity("Crate");
        CHECK(scene.nameOf(e) == "Crate");
        CHECK(scene.has<Transform>(e));
        CHECK(scene.has<Hierarchy>(e));
        CHECK(scene.findByName("Crate") == e);
        CHECK(scene.findByName("Nope").isNull());
        CHECK(scene.entityCount() == 1);
    }

    TEST_CASE("a child moves with its parent") {
        Scene scene;
        const Entity parent = scene.createEntity("Parent");
        const Entity child = scene.createEntity("Child");
        REQUIRE(scene.setParent(child, parent, false));
        scene.get<Transform>(child).position = Vec3(2.0f, 0.0f, 0.0f);

        scene.get<Transform>(parent).position = Vec3(0.0f, 1.0f, 0.0f);
        CHECK(Math::nearlyEqual(scene.worldPosition(child), Vec3(2.0f, 1.0f, 0.0f)));

        // Turn the parent 90 degrees left: the child swings from +X to -Z.
        scene.get<Transform>(parent).rotation = glm::angleAxis(Math::HalfPi, Math::WorldUp);
        CHECK(Math::nearlyEqual(scene.worldPosition(child), Vec3(0.0f, 1.0f, -2.0f), 1e-5f));

        // Scale the parent: the child's offset scales too.
        scene.get<Transform>(parent).scale = Vec3(3.0f);
        CHECK(Math::nearlyEqual(scene.worldPosition(child), Vec3(0.0f, 1.0f, -6.0f), 1e-4f));
    }

    TEST_CASE("three levels: the grandchild follows both") {
        Scene scene;
        const Entity sun = scene.createEntity("Sun");
        const Entity planet = scene.createEntity("Planet");
        const Entity moon = scene.createEntity("Moon");
        scene.setParent(planet, sun, false);
        scene.setParent(moon, planet, false);
        scene.get<Transform>(planet).position = Vec3(10.0f, 0.0f, 0.0f);
        scene.get<Transform>(moon).position = Vec3(1.0f, 0.0f, 0.0f);
        scene.get<Transform>(sun).position = Vec3(0.0f, 0.0f, 5.0f);
        CHECK(Math::nearlyEqual(scene.worldPosition(moon), Vec3(11.0f, 0.0f, 5.0f)));
        CHECK(scene.isDescendantOf(moon, sun));
        CHECK_FALSE(scene.isDescendantOf(sun, moon));
    }

    TEST_CASE("setParent keeps the world transform by default") {
        Scene scene;
        const Entity parent = scene.createEntity("Parent");
        Transform& pt = scene.get<Transform>(parent);
        pt.position = Vec3(5.0f, 0.0f, 0.0f);
        pt.rotation = Math::quatFromEulerDegrees(Vec3(0.0f, 30.0f, 0.0f));
        pt.scale = Vec3(2.0f);

        const Entity child = scene.createEntity("Child");
        Transform& ct = scene.get<Transform>(child);
        ct.position = Vec3(1.0f, 2.0f, 3.0f);
        ct.rotation = Math::quatFromEulerDegrees(Vec3(10.0f, 0.0f, 0.0f));
        const Mat4 before = scene.worldMatrix(child);

        REQUIRE(scene.setParent(child, parent));
        CHECK(Math::nearlyEqual(scene.worldMatrix(child), before, 1e-4f));
        // ...and back to the top level, still in place.
        REQUIRE(scene.setParent(child, NullEntity));
        CHECK(Math::nearlyEqual(scene.worldMatrix(child), before, 1e-4f));
        CHECK(Math::nearlyEqual(scene.get<Transform>(child).position, Vec3(1.0f, 2.0f, 3.0f), 1e-4f));
    }

    TEST_CASE("setParent without keeping the world transform keeps the local numbers") {
        Scene scene;
        const Entity parent = scene.createEntity("Parent");
        scene.get<Transform>(parent).position = Vec3(5.0f, 0.0f, 0.0f);
        const Entity child = scene.createEntity("Child");
        scene.get<Transform>(child).position = Vec3(1.0f, 0.0f, 0.0f);
        scene.setParent(child, parent, false);
        CHECK(Math::nearlyEqual(scene.worldPosition(child), Vec3(6.0f, 0.0f, 0.0f)));
    }

    TEST_CASE("setParent refuses loops and keeps both sides in agreement") {
        Scene scene;
        const Entity a = scene.createEntity("A");
        const Entity b = scene.createEntity("B");
        const Entity c = scene.createEntity("C");
        scene.setParent(b, a);
        scene.setParent(c, b);
        CHECK_FALSE(scene.setParent(a, c)); // A under its own grandchild
        CHECK_FALSE(scene.setParent(a, a)); // under itself
        CHECK(scene.parentOf(a).isNull());

        // Moving C from B to A updates both children lists.
        scene.setParent(c, a);
        CHECK(scene.parentOf(c) == a);
        CHECK(contains(scene.childrenOf(a), c));
        CHECK_FALSE(contains(scene.childrenOf(b), c));
        CHECK(scene.roots() == std::vector<Entity>{a});
    }

    TEST_CASE("destroying an entity destroys its children and detaches it from its parent") {
        Scene scene;
        const Entity root = scene.createEntity("Root");
        const Entity middle = scene.createEntity("Middle");
        const Entity leaf = scene.createEntity("Leaf");
        const Entity sibling = scene.createEntity("Sibling");
        scene.setParent(middle, root);
        scene.setParent(leaf, middle);
        scene.setParent(sibling, root);

        scene.destroyEntity(middle);
        CHECK_FALSE(scene.isAlive(middle));
        CHECK_FALSE(scene.isAlive(leaf));
        CHECK(scene.isAlive(sibling));
        CHECK(scene.childrenOf(root) == std::vector<Entity>{sibling});
        CHECK(scene.entityCount() == 2);
    }

    TEST_CASE("interpolation blends from the previous fixed step to the current one") {
        Scene scene;
        const Entity e = scene.createEntity("Mover");
        scene.beginFixedStep(); // remembers position 0
        scene.get<Transform>(e).position = Vec3(10.0f, 0.0f, 0.0f);

        CHECK(Vec3(scene.interpolatedWorldMatrix(e, 0.0f)[3]).x == doctest::Approx(0.0f));
        CHECK(Vec3(scene.interpolatedWorldMatrix(e, 0.25f)[3]).x == doctest::Approx(2.5f));
        CHECK(Vec3(scene.interpolatedWorldMatrix(e, 1.0f)[3]).x == doctest::Approx(10.0f));
        // Gameplay always sees the current state.
        CHECK(scene.worldPosition(e).x == doctest::Approx(10.0f));

        scene.get<Transform>(e).resetInterpolation(); // a teleport: no sliding
        CHECK(Vec3(scene.interpolatedWorldMatrix(e, 0.0f)[3]).x == doctest::Approx(10.0f));
    }

    TEST_CASE("an entity placed before any fixed step is drawn where it is, not sliding from the origin") {
        Scene scene;
        const Entity e = scene.createEntity("Spawned");
        scene.get<Transform>(e).position = Vec3(7.0f, 0.0f, 0.0f);
        CHECK(Vec3(scene.interpolatedWorldMatrix(e, 0.0f)[3]).x == doctest::Approx(7.0f));
    }

    TEST_CASE("a camera entity looks down its -Z axis, ignoring scale") {
        Scene scene;
        const Entity cam = scene.createEntity("Camera");
        scene.add<CameraComponent>(cam);
        Transform& t = scene.get<Transform>(cam);
        t.position = Vec3(0.0f, 2.0f, 5.0f);
        t.rotation = glm::angleAxis(Math::HalfPi, Math::WorldUp); // turned left: looks down -X
        t.scale = Vec3(4.0f, 0.5f, 2.0f);                          // must not distort the view

        CHECK(scene.primaryCamera() == cam);
        const CameraView view = cameraViewFor(scene, cam, 1.0f);
        CHECK(Math::nearlyEqual(view.position, t.position));
        const Vec3 ahead = t.position + Vec3(-3.0f, 0.0f, 0.0f);
        CHECK(Math::nearlyEqual(Math::transformPoint(view.view, ahead), Vec3(0.0f, 0.0f, -3.0f), 1e-4f));
    }

    TEST_CASE("the primary flag picks the camera") {
        Scene scene;
        const Entity a = scene.createEntity("A");
        scene.add<CameraComponent>(a).primary = false;
        CHECK(scene.primaryCamera().isNull());
        const Entity b = scene.createEntity("B");
        scene.add<CameraComponent>(b);
        CHECK(scene.primaryCamera() == b);
    }

    TEST_CASE("a light entity shines along its -Z axis") {
        Scene scene;
        const Entity sun = scene.createEntity("Sun");
        scene.add<LightComponent>(sun).intensity = 2.0f;
        // Pitched 90 degrees down: -Z turns to point at the ground.
        scene.get<Transform>(sun).rotation = glm::angleAxis(-Math::HalfPi, Math::WorldRight);
        const DirectionalLight light = directionalLightFor(scene, sun);
        CHECK(Math::nearlyEqual(light.direction, Vec3(0.0f, -1.0f, 0.0f), 1e-5f));
        CHECK(light.intensity == 2.0f);
        // No light entity: ambient only.
        CHECK(directionalLightFor(scene, NullEntity).intensity == 0.0f);
    }

    TEST_CASE("free camera rotation matches its forward vector") {
        Camera camera;
        camera.yaw = 0.7f;
        camera.pitch = -0.4f;
        CHECK(Math::nearlyEqual(camera.rotation() * Math::WorldForward, camera.forward(), 1e-5f));
    }

    TEST_CASE("collision: overlapping colliders are found, each pair once") {
        Scene scene;
        const Entity ball = scene.createEntity("Ball");
        scene.add<Collider>(ball).shape = ColliderShape::Sphere;
        const Entity nearCrate = scene.createEntity("Near crate");
        scene.add<Collider>(nearCrate);
        scene.get<Transform>(nearCrate).position = Vec3(0.9f, 0.0f, 0.0f); // box from 0.4: inside the ball's 0.5
        const Entity farCrate = scene.createEntity("Far crate");
        scene.add<Collider>(farCrate);
        scene.get<Transform>(farCrate).position = Vec3(5.0f, 0.0f, 0.0f);
        scene.createEntity("No collider");

        const auto pairs = findOverlaps(scene);
        REQUIRE(pairs.size() == 1);
        CHECK(((pairs[0].first == ball && pairs[0].second == nearCrate) || (pairs[0].first == nearCrate && pairs[0].second == ball)));
        CHECK(overlapping(scene, ball) == std::vector<Entity>{nearCrate});
        CHECK(overlapping(scene, farCrate).empty());

        scene.get<Transform>(ball).position = Vec3(4.0f, 0.0f, 0.0f); // moved next to the far crate
        CHECK(overlapping(scene, ball) == std::vector<Entity>{farCrate});
    }

    TEST_CASE("collision: colliders follow scale, rotation and parents") {
        Scene scene;
        const Entity parent = scene.createEntity("Parent");
        scene.get<Transform>(parent).position = Vec3(10.0f, 0.0f, 0.0f);
        const Entity child = scene.createEntity("Child");
        scene.add<Collider>(child).shape = ColliderShape::Sphere;
        scene.setParent(child, parent, false);
        scene.get<Transform>(child).scale = Vec3(1.0f, 3.0f, 1.0f); // largest axis -> radius 1.5

        const WorldCollider w = toWorld(scene.get<Collider>(child), scene.worldMatrix(child));
        CHECK(Math::nearlyEqual(w.sphere.center, Vec3(10.0f, 0.0f, 0.0f)));
        CHECK(w.sphere.radius == doctest::Approx(1.5f));

        // A 1 m box turned 45 degrees: its world box is sqrt(2) wide.
        Collider box;
        const Mat4 turned = Math::composeTRS(Vec3(0.0f), glm::angleAxis(Math::Pi / 4.0f, Math::WorldUp), Vec3(1.0f));
        const WorldCollider wb = toWorld(box, turned);
        CHECK(wb.box.halfExtents().x == doctest::Approx(std::sqrt(0.5f)));
        CHECK(wb.box.halfExtents().y == doctest::Approx(0.5f));
    }
}
