// Tests for the editor's non-UI parts: the undo stack, every command (each
// must undo exactly), the gizmo math and the inspector's field hints.

#include "editor/Commands.h"
#include "editor/FieldHints.h"
#include "editor/Gizmo.h"

#include <doctest/doctest.h>

#include <cmath>

using namespace Engine;

namespace {

// A document with: Root (id 1) > [A (2), B (3) > C (4)], and Other (5).
struct Fixture {
    SceneSerializer serializer;
    EditorDocument doc;
    UndoStack undo;

    Fixture() {
        doc.serializer = &serializer;
        Scene& s = doc.scene;
        const Entity root = s.createEntity("Root");
        const Entity a = s.createEntity("A");
        const Entity b = s.createEntity("B");
        const Entity c = s.createEntity("C");
        s.createEntity("Other");
        s.setParent(a, root, false);
        s.setParent(b, root, false);
        s.setParent(c, b, false);
        s.get<Transform>(root).position = Vec3(10.0f, 0.0f, 0.0f);
        s.get<Transform>(b).position = Vec3(0.0f, 2.0f, 0.0f);
        s.add<MeshRenderer>(c).mesh = BuiltinAssets::Cube;
    }
    Entity e(SceneId id) const { return doc.scene.findById(id); }
    std::string sceneJson() const { return serializer.toJson(doc.scene).dump(); }
};

} // namespace

TEST_SUITE("editor") {
    TEST_CASE("undo stack: execute, undo, redo, and a new action drops the redo branch") {
        Fixture f;
        f.undo.execute(std::make_unique<RenameCommand>(2, "A", "First"), f.doc);
        f.undo.execute(std::make_unique<RenameCommand>(2, "First", "Second"), f.doc);
        CHECK(f.doc.scene.nameOf(f.e(2)) == "Second");
        CHECK(f.undo.undoName() == "Rename Second");

        CHECK(f.undo.undo(f.doc));
        CHECK(f.doc.scene.nameOf(f.e(2)) == "First");
        CHECK(f.undo.redoName() == "Rename Second");
        CHECK(f.undo.redo(f.doc));
        CHECK(f.doc.scene.nameOf(f.e(2)) == "Second");

        f.undo.undo(f.doc);
        f.undo.execute(std::make_unique<RenameCommand>(2, "First", "Third"), f.doc);
        CHECK_FALSE(f.undo.canRedo()); // "Second" can't come back any more
        CHECK(f.undo.size() == 2);
        CHECK(f.undo.undo(f.doc));
        CHECK(f.undo.undo(f.doc));
        CHECK_FALSE(f.undo.undo(f.doc)); // nothing left
        CHECK(f.doc.scene.nameOf(f.e(2)) == "A");
    }

    TEST_CASE("undo stack: dirty means 'not at the saved position'") {
        Fixture f;
        CHECK_FALSE(f.undo.isDirty());
        f.undo.execute(std::make_unique<RenameCommand>(2, "A", "X"), f.doc);
        CHECK(f.undo.isDirty());
        f.undo.markSaved();
        CHECK_FALSE(f.undo.isDirty());
        f.undo.undo(f.doc);
        CHECK(f.undo.isDirty());
        f.undo.redo(f.doc);
        CHECK_FALSE(f.undo.isDirty()); // back where it was saved
        f.undo.undo(f.doc);
        f.undo.execute(std::make_unique<RenameCommand>(2, "A", "Y"), f.doc);
        CHECK(f.undo.isDirty()); // the saved state is gone from history
    }

    TEST_CASE("change component: edit, add and remove all undo") {
        Fixture f;
        const Json before = f.serializer.saveComponent(f.doc.scene, f.e(4), "meshRenderer");
        Json after = before;
        after["material"]["baseColor"] = {1.0, 0.0, 0.0};
        f.undo.execute(std::make_unique<ChangeComponentCommand>(4, "meshRenderer", before, after), f.doc);
        CHECK(f.doc.scene.get<MeshRenderer>(f.e(4)).material.baseColor.y == 0.0f);
        f.undo.undo(f.doc);
        CHECK(f.doc.scene.get<MeshRenderer>(f.e(4)).material.baseColor.y == 1.0f);

        // Add (before = null) and remove (after = null).
        f.undo.execute(std::make_unique<ChangeComponentCommand>(5, "collider", nullptr, Json::object()), f.doc);
        CHECK(f.doc.scene.has<Collider>(f.e(5)));
        f.undo.undo(f.doc);
        CHECK_FALSE(f.doc.scene.has<Collider>(f.e(5)));
        f.undo.execute(std::make_unique<ChangeComponentCommand>(4, "meshRenderer", before, nullptr), f.doc);
        CHECK_FALSE(f.doc.scene.has<MeshRenderer>(f.e(4)));
        f.undo.undo(f.doc);
        CHECK(f.doc.scene.get<MeshRenderer>(f.e(4)).mesh == BuiltinAssets::Cube);
    }

    TEST_CASE("delete then undo restores the subtree: same ids, parent, position and data") {
        Fixture f;
        const std::string original = f.sceneJson();
        f.doc.selected = 4;
        f.undo.execute(std::make_unique<DeleteEntityCommand>(f.doc, 2), f.doc); // A, the first child of Root
        f.undo.execute(std::make_unique<DeleteEntityCommand>(f.doc, 3), f.doc); // B, which holds C
        CHECK(f.e(3).isNull());
        CHECK(f.e(4).isNull());     // the child went with it
        CHECK(f.doc.selected == 0); // the selection was inside the deleted subtree
        CHECK(f.doc.scene.entityCount() == 2);

        f.undo.undo(f.doc);
        f.undo.undo(f.doc);
        CHECK(f.sceneJson() == original); // exactly as before, order included
        CHECK(f.doc.scene.parentOf(f.e(4)) == f.e(3));
        CHECK(f.doc.scene.childrenOf(f.e(1)) == std::vector<Entity>{f.e(2), f.e(3)});
    }

    TEST_CASE("commands made before a delete still work after it's undone (they use SceneIds)") {
        Fixture f;
        f.undo.execute(std::make_unique<RenameCommand>(4, "C", "Renamed"), f.doc);
        f.undo.execute(std::make_unique<DeleteEntityCommand>(f.doc, 4), f.doc);
        f.undo.undo(f.doc); // C is back, as a new Entity handle with the same id
        f.undo.undo(f.doc); // the rename's undo must find it
        CHECK(f.doc.scene.nameOf(f.e(4)) == "C");
        f.undo.redo(f.doc);
        CHECK(f.doc.scene.nameOf(f.e(4)) == "Renamed");
    }

    TEST_CASE("create: a new entity, selected, removed by undo, same id on redo") {
        Fixture f;
        auto command = makeCreateEntityCommand(f.doc, "Lamp", 1, Json{{"light", Json::object()}});
        const SceneId id = command->rootId();
        f.undo.execute(std::move(command), f.doc);
        REQUIRE_FALSE(f.e(id).isNull());
        CHECK(f.doc.scene.has<LightComponent>(f.e(id)));
        CHECK(f.doc.scene.parentOf(f.e(id)) == f.e(1));
        CHECK(f.doc.selected == id);
        f.undo.undo(f.doc);
        CHECK(f.e(id).isNull());
        f.undo.redo(f.doc);
        CHECK_FALSE(f.e(id).isNull());
    }

    TEST_CASE("duplicate: a copy with new ids next to the original, children included") {
        Fixture f;
        const std::size_t before = f.doc.scene.entityCount();
        auto command = makeDuplicateCommand(f.doc, 3); // B > C
        REQUIRE(command);
        CHECK(f.doc.scene.entityCount() == before); // making the command changes nothing yet
        const SceneId copyId = command->rootId();
        f.undo.execute(std::move(command), f.doc);
        const Entity copy = f.e(copyId);
        CHECK(f.doc.scene.nameOf(copy) == "B (copy)");
        CHECK(copyId != 3);
        CHECK(f.doc.scene.childrenOf(copy).size() == 1);
        CHECK(f.doc.scene.childrenOf(f.e(1)) == std::vector<Entity>{f.e(2), f.e(3), copy}); // right after B
        CHECK(f.doc.scene.entityCount() == before + 2);
        f.undo.undo(f.doc);
        CHECK(f.doc.scene.entityCount() == before);
    }

    TEST_CASE("reparent keeps the world position; undo restores the exact local numbers") {
        Fixture f;
        const Vec3 world = f.doc.scene.worldPosition(f.e(4)); // C, under B, under Root
        f.undo.execute(std::make_unique<ReparentCommand>(f.doc, 4, 5, 0), f.doc);
        CHECK(f.doc.scene.parentOf(f.e(4)) == f.e(5));
        CHECK(Math::nearlyEqual(f.doc.scene.worldPosition(f.e(4)), world, 1e-4f));
        f.undo.undo(f.doc);
        CHECK(f.doc.scene.parentOf(f.e(4)) == f.e(3));
        CHECK(f.doc.scene.get<Transform>(f.e(4)).position == Vec3(0.0f));

        // Reorder among siblings: move A after B.
        f.undo.execute(std::make_unique<ReparentCommand>(f.doc, 2, 1, 1), f.doc);
        CHECK(f.doc.scene.childrenOf(f.e(1)) == std::vector<Entity>{f.e(3), f.e(2)});
        f.undo.undo(f.doc);
        CHECK(f.doc.scene.childrenOf(f.e(1)) == std::vector<Entity>{f.e(2), f.e(3)});
    }

    TEST_CASE("gizmo math: screen and world agree") {
        CameraView cam;
        cam.position = Vec3(0.0f, 0.0f, 5.0f);
        cam.view = Math::lookAt(cam.position, Vec3(0.0f));
        cam.projection = Math::perspective(Math::toRadians(60.0f), 2.0f, 0.1f, 100.0f);
        const Vec2 pos(100.0f, 50.0f), size(800.0f, 400.0f);
        bool inFront = false;
        const Vec2 center = GizmoMath::worldToScreen(Vec3(0.0f), cam.viewProjection(), pos, size, &inFront);
        CHECK(inFront);
        CHECK(Math::nearlyEqual(Vec3(center, 0.0f), Vec3(500.0f, 250.0f, 0.0f), 1e-3f)); // the viewport's middle
        // Up in the world is up on screen, which is smaller y in pixels.
        CHECK(GizmoMath::worldToScreen(Vec3(0.0f, 1.0f, 0.0f), cam.viewProjection(), pos, size).y < 250.0f);
        GizmoMath::worldToScreen(Vec3(0.0f, 0.0f, 10.0f), cam.viewProjection(), pos, size, &inFront);
        CHECK_FALSE(inFront); // behind the camera

        // A ray through a pixel passes through the world point that lands there.
        const Vec3 point(1.0f, -0.5f, -2.0f);
        const Vec2 pixel = GizmoMath::worldToScreen(point, cam.viewProjection(), pos, size);
        const Ray ray = GizmoMath::screenToRay(pixel, glm::inverse(cam.viewProjection()), pos, size);
        const Vec3 toPoint = glm::normalize(point - ray.origin);
        CHECK(glm::dot(toPoint, ray.direction) == doctest::Approx(1.0f).epsilon(1e-5));

        // 5 m away with a 60-degree view 400 px tall: 2 * 5 * tan(30) / 400 meters per pixel.
        CHECK(GizmoMath::metersPerPixel(Vec3(0.0f), cam, 400.0f) == doctest::Approx(2.0f * 5.0f * std::tan(Math::Pi / 6.0f) / 400.0f));
    }

    TEST_CASE("gizmo math: dragging along an axis, around a ring, and scaling") {
        // Two rays straight down from above, 3 m apart along X.
        const Ray start{Vec3(1.0f, 10.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)};
        const Ray now{Vec3(4.0f, 10.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)};
        CHECK(*GizmoMath::axisDragDistance(start, now, Vec3(0.0f), Vec3(1.0f, 0.0f, 0.0f)) == doctest::Approx(3.0f));
        CHECK(*GizmoMath::axisDragDistance(start, now, Vec3(0.0f), Vec3(0.0f, 0.0f, 1.0f)) == doctest::Approx(0.0f));
        // From x = 1 to x = 4 is 4 times as far from the origin.
        CHECK(*GizmoMath::axisScaleFactor(start, now, Vec3(0.0f), Vec3(1.0f, 0.0f, 0.0f)) == doctest::Approx(4.0f));
        // Around +Y, the mouse going from +X to -Z on the ground is +90 degrees.
        const Ray toMinusZ{Vec3(0.0f, 10.0f, -1.0f), Vec3(0.0f, -1.0f, 0.0f)};
        CHECK(*GizmoMath::ringDragAngle(start, toMinusZ, Vec3(0.0f), Math::WorldUp) == doctest::Approx(Math::HalfPi));
        // A ring seen exactly edge-on can't be dragged.
        const Ray sideways{Vec3(-5.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f)};
        CHECK_FALSE(GizmoMath::ringDragAngle(sideways, sideways, Vec3(0.0f), Math::WorldUp));
    }

    TEST_CASE("gizmo math: snapping and distance to a segment") {
        CHECK(GizmoMath::snap(1.26f, 0.5f) == doctest::Approx(1.5f));
        CHECK(GizmoMath::snap(-0.2f, 0.5f) == doctest::Approx(0.0f));
        CHECK(GizmoMath::snap(1.26f, 0.0f) == 1.26f);
        CHECK(GizmoMath::distanceToSegment(Vec2(5.0f, 3.0f), Vec2(0.0f), Vec2(10.0f, 0.0f)) == doctest::Approx(3.0f));
        CHECK(GizmoMath::distanceToSegment(Vec2(13.0f, 4.0f), Vec2(0.0f), Vec2(10.0f, 0.0f)) == doctest::Approx(5.0f));
    }

    TEST_CASE("field hints and labels") {
        CHECK(prettyLabel("meshRenderer") == "Mesh Renderer");
        CHECK(prettyLabel("halfExtents") == "Half Extents");
        CHECK(prettyLabel("fovDegrees") == "Fov Degrees");
        CHECK(fieldHint("meshRenderer", "material.baseColor").widget == FieldWidget::Color);
        CHECK(fieldHint("collider", "shape").choices == std::vector<std::string>{"box", "sphere"});
        CHECK(fieldHint("spin", "axis").widget == FieldWidget::Auto); // unknown: from the JSON type
    }
}
