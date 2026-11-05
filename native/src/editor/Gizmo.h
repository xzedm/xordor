#pragma once
// Gizmo: the arrows, rings and boxes you drag to move, rotate and scale the
// selected entity in the viewport.
//
//   Move     three arrows. Drag one: the object slides along that axis only.
//            The square in the middle moves it in the plane facing the camera.
//   Rotate   three rings. Drag one: the object turns around that axis.
//   Scale    three box-tipped handles, plus a center box for uniform scale.
//
// The hard part is turning a 2D mouse position into a 3D change:
//
//   * Along an axis: shoot a ray from the camera through the mouse. It almost
//     never meets the axis exactly, so use the point on the axis *closest* to
//     the ray (Geometry: closestOnLine). The difference between that point now
//     and when the drag started is how far to move.
//   * Around an axis: intersect the ray with the plane the ring lies in. The
//     angle between "where the mouse hit the plane at the start" and "where it
//     hits now", measured around the axis, is how far to turn.
//
// The gizmo works on world matrices. The editor turns the result back into the
// entity's local Transform (relative to its parent).
//
// GizmoMath holds the pure math (unit-tested); Gizmo adds ImGui drawing and
// mouse handling.

#include "core/Geometry.h"
#include "core/Math.h"
#include "render/Camera.h"

#include <optional>

namespace Engine {

namespace GizmoMath {

// Where a world point lands on screen, in pixels (y down, like the mouse),
// for a viewport whose top-left corner is at viewportPos. *inFront is false
// when the point is behind the camera (its screen position is meaningless).
Vec2 worldToScreen(const Vec3& point, const Mat4& viewProjection, const Vec2& viewportPos, const Vec2& viewportSize,
                   bool* inFront = nullptr);
// The ray from the camera through a pixel of the viewport.
Ray screenToRay(const Vec2& pixel, const Mat4& inverseViewProjection, const Vec2& viewportPos, const Vec2& viewportSize);
// How many meters one pixel covers at `point`. Multiplying the gizmo's size in
// pixels by this keeps it the same size on screen at any distance.
float metersPerPixel(const Vec3& point, const CameraView& camera, float viewportHeight);
float distanceToSegment(const Vec2& p, const Vec2& a, const Vec2& b);
// Rounds to the nearest multiple of step (step <= 0: unchanged).
float snap(float value, float step);

// How far to move along `axis` (unit length) between two mouse rays.
std::optional<float> axisDragDistance(const Ray& start, const Ray& now, const Vec3& origin, const Vec3& axis);
// How far (radians) to turn around `axis` through `center` between two rays.
std::optional<float> ringDragAngle(const Ray& start, const Ray& now, const Vec3& center, const Vec3& axis);
// Ratio for scaling along `axis`: how much further from `origin` the closest
// point is now than at the start.
std::optional<float> axisScaleFactor(const Ray& start, const Ray& now, const Vec3& origin, const Vec3& axis);

} // namespace GizmoMath

enum class GizmoMode { Translate, Rotate, Scale };
enum class GizmoSpace { World, Local }; // scale always uses the object's own axes

struct GizmoSnap {
    bool enabled = false;
    float meters = 0.5f;
    float degrees = 15.0f;
    float scaleStep = 0.1f;
};

struct GizmoViewport {
    CameraView camera;
    Vec2 position{0.0f}; // top-left corner on screen, pixels
    Vec2 size{1.0f};
};

class Gizmo {
public:
    // Call once per frame while the viewport window is being built. Draws the
    // gizmo, handles the mouse, and while dragging writes the new world matrix
    // into `world`. Returns true on frames where `world` changed.
    bool manipulate(const GizmoViewport& viewport, GizmoMode mode, GizmoSpace space, const GizmoSnap& snap, Mat4& world);

    bool isHovered() const { return hovered_ >= 0; } // the mouse is over a handle: don't select-click
    bool isActive() const { return active_ >= 0; }   // dragging
    bool dragStarted() const { return started_; }    // this frame (undo: remember "before")
    bool dragEnded() const { return ended_; }        // this frame (undo: push the command)

    static constexpr float SizePixels = 110.0f; // arrow length on screen
    static constexpr int Center = 3;            // handle index of the middle square / box

private:
    int pickHandle(const GizmoViewport& viewport, GizmoMode mode, const Vec2& mouse) const;
    Mat4 dragResult(const GizmoViewport& viewport, GizmoMode mode, const GizmoSnap& snap, const Ray& now,
                    const Vec2& mouse) const;
    void draw(const GizmoViewport& viewport, GizmoMode mode) const;

    // Set up per frame from the target's world matrix.
    Vec3 center_{0.0f};
    Vec3 axes_[3] = {};  // unit directions of the three handles
    float length_ = 1.0f; // meters

    int hovered_ = -1;
    int active_ = -1;
    bool started_ = false;
    bool ended_ = false;
    Mat4 startWorld_{1.0f};
    Vec3 startCenter_{0.0f};
    Vec3 startAxes_[3] = {};
    Ray startRay_;
    Vec2 startMouse_{0.0f};
};

} // namespace Engine
