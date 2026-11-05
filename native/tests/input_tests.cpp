#include "core/Input.h"

#include <doctest/doctest.h>

using namespace Engine;

TEST_SUITE("input") {

TEST_CASE("a key press shows as pressed for one frame and down until released") {
    Input in;
    in.onKey(Key::Space, true);
    CHECK(in.isKeyDown(Key::Space));
    CHECK(in.wasKeyPressed(Key::Space));
    in.endFrame();
    CHECK(in.isKeyDown(Key::Space));
    CHECK_FALSE(in.wasKeyPressed(Key::Space)); // the edge is gone
    in.onKey(Key::Space, false);
    CHECK_FALSE(in.isKeyDown(Key::Space));
    CHECK(in.wasKeyReleased(Key::Space));
    in.endFrame();
    CHECK_FALSE(in.wasKeyReleased(Key::Space));
}

TEST_CASE("a tap inside one frame still counts as a press") {
    Input in;
    in.onKey(Key::E, true);
    in.onKey(Key::E, false);
    CHECK(in.wasKeyPressed(Key::E));
    CHECK(in.wasKeyReleased(Key::E));
    CHECK_FALSE(in.isKeyDown(Key::E));
}

TEST_CASE("repeated down events don't create new presses") {
    Input in;
    in.onKey(Key::A, true);
    in.endFrame();
    in.onKey(Key::A, true); // e.g. a duplicate event after a focus change
    CHECK_FALSE(in.wasKeyPressed(Key::A));
}

TEST_CASE("unknown keys are ignored safely") {
    Input in;
    in.onKey(Key::Unknown, true);
    in.onKey(static_cast<Key>(9999), true);
    CHECK_FALSE(in.isKeyDown(Key::Unknown));
    CHECK_FALSE(in.isKeyDown(static_cast<Key>(9999)));
}

TEST_CASE("mouse buttons behave like keys") {
    Input in;
    in.onMouseButton(MouseButton::Left, true);
    CHECK(in.wasMousePressed(MouseButton::Left));
    CHECK(in.isMouseDown(MouseButton::Left));
    in.endFrame();
    in.onMouseButton(MouseButton::Left, false);
    CHECK(in.wasMouseReleased(MouseButton::Left));
}

TEST_CASE("mouse movement adds up within a frame and resets after it") {
    Input in;
    in.onMouseMove(Vec2(100.0f, 100.0f)); // the first event only sets the position
    CHECK(in.mouseDelta() == Vec2(0.0f));
    in.onMouseMove(Vec2(110.0f, 95.0f));
    in.onMouseMove(Vec2(115.0f, 90.0f));
    CHECK(in.mouseDelta() == Vec2(15.0f, -10.0f));
    CHECK(in.mousePosition() == Vec2(115.0f, 90.0f));
    in.onScroll(1.0f);
    in.onScroll(0.5f);
    CHECK(in.scrollDelta() == doctest::Approx(1.5f));
    in.endFrame();
    CHECK(in.mouseDelta() == Vec2(0.0f));
    CHECK(in.scrollDelta() == 0.0f);
    CHECK(in.mousePosition() == Vec2(115.0f, 90.0f)); // position is kept
}

TEST_CASE("losing focus releases everything") {
    Input in;
    in.onKey(Key::W, true);
    in.onMouseButton(MouseButton::Right, true);
    in.endFrame();
    in.releaseAll();
    CHECK_FALSE(in.isKeyDown(Key::W));
    CHECK(in.wasKeyReleased(Key::W));
    CHECK_FALSE(in.isMouseDown(MouseButton::Right));
}

} // TEST_SUITE
