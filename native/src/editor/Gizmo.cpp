#include "editor/Gizmo.h"

#include <imgui.h>

#include <cmath>

namespace Engine {

// ---------------------------------------------------------------- GizmoMath

namespace GizmoMath {

Vec2 worldToScreen(const Vec3& point, const Mat4& viewProjection, const Vec2& viewportPos, const Vec2& viewportSize,
                   bool* inFront) {
    const Vec4 clip = viewProjection * Vec4(point, 1.0f);
    if (inFront) *inFront = clip.w > 1e-4f;
    const Vec2 ndc = Vec2(clip) / std::max(clip.w, 1e-4f);
    // NDC: -1..1 with +y up. Screen pixels: 0..size with +y down.
    return viewportPos + Vec2((ndc.x * 0.5f + 0.5f) * viewportSize.x, (0.5f - ndc.y * 0.5f) * viewportSize.y);
}

Ray screenToRay(const Vec2& pixel, const Mat4& inverseViewProjection, const Vec2& viewportPos, const Vec2& viewportSize) {
    const Vec2 local = (pixel - viewportPos) / viewportSize;
    return rayFromScreen(Vec2(local.x * 2.0f - 1.0f, 1.0f - local.y * 2.0f), inverseViewProjection);
}

float metersPerPixel(const Vec3& point, const CameraView& camera, float viewportHeight) {
    // projection[1][1] = 1 / tan(fov/2): the screen's half-height, one meter
    // away, is tan(fov/2) meters. At distance d it's d times that.
    const float distance = glm::length(point - camera.position);
    return 2.0f * distance / (camera.projection[1][1] * std::max(viewportHeight, 1.0f));
}

float distanceToSegment(const Vec2& p, const Vec2& a, const Vec2& b) {
    const Vec2 ab = b - a;
    const float lengthSquared = glm::dot(ab, ab);
    const float t = lengthSquared > 0.0f ? Math::clamp(glm::dot(p - a, ab) / lengthSquared, 0.0f, 1.0f) : 0.0f;
    return glm::length(p - (a + ab * t));
}

float snap(float value, float step) {
    return step > 0.0f ? std::round(value / step) * step : value;
}

std::optional<float> axisDragDistance(const Ray& start, const Ray& now, const Vec3& origin, const Vec3& axis) {
    const auto s0 = closestOnLine(start, origin, axis);
    const auto s1 = closestOnLine(now, origin, axis);
    if (!s0 || !s1) return std::nullopt;
    return *s1 - *s0;
}

std::optional<float> ringDragAngle(const Ray& start, const Ray& now, const Vec3& center, const Vec3& axis) {
    const auto t0 = intersectPlane(start, center, axis);
    const auto t1 = intersectPlane(now, center, axis);
    if (!t0 || !t1) return std::nullopt; // the ring is edge-on, or behind us
    const Vec3 a = start.at(*t0) - center;
    const Vec3 b = now.at(*t1) - center;
    if (glm::length(a) < 1e-5f || glm::length(b) < 1e-5f) return std::nullopt;
    return signedAngle(glm::normalize(a), glm::normalize(b), axis);
}

std::optional<float> axisScaleFactor(const Ray& start, const Ray& now, const Vec3& origin, const Vec3& axis) {
    const auto s0 = closestOnLine(start, origin, axis);
    const auto s1 = closestOnLine(now, origin, axis);
    if (!s0 || !s1 || std::fabs(*s0) < 1e-4f) return std::nullopt;
    return *s1 / *s0;
}

} // namespace GizmoMath

// ---------------------------------------------------------------- Gizmo

namespace {

ImU32 handleColor(int handle, int hovered, int active) {
    if (handle == active || (active < 0 && handle == hovered)) return IM_COL32(255, 210, 40, 255);
    switch (handle) {
    case 0: return IM_COL32(230, 70, 70, 255);   // x: red
    case 1: return IM_COL32(110, 210, 80, 255);  // y: green
    case 2: return IM_COL32(70, 130, 240, 255);  // z: blue
    default: return IM_COL32(220, 220, 220, 255);
    }
}

ImVec2 im(const Vec2& v) { return ImVec2(v.x, v.y); }

constexpr int RingSegments = 64;
constexpr float GrabPixels = 7.0f; // how close the mouse must be to grab a handle

} // namespace

bool Gizmo::manipulate(const GizmoViewport& viewport, GizmoMode mode, GizmoSpace space, const GizmoSnap& snap,
                       Mat4& world) {
    started_ = ended_ = false;
    // Where the gizmo sits and which way its handles point. Scale always uses
    // the object's own axes: scaling along a world axis of a rotated object
    // would shear it.
    center_ = Vec3(world[3]);
    const bool local = space == GizmoSpace::Local || mode == GizmoMode::Scale;
    for (int i = 0; i < 3; ++i) {
        Vec3 axis(0.0f);
        axis[i] = 1.0f;
        const Vec3 column(world[i]);
        axes_[i] = local && glm::length(column) > 1e-6f ? glm::normalize(column) : axis;
    }
    length_ = SizePixels * GizmoMath::metersPerPixel(center_, viewport.camera, viewport.size.y);

    const ImGuiIO& io = ImGui::GetIO();
    const Vec2 mouse(io.MousePos.x, io.MousePos.y);
    const Mat4 inverseViewProjection = glm::inverse(viewport.camera.viewProjection());
    const Ray ray = GizmoMath::screenToRay(mouse, inverseViewProjection, viewport.position, viewport.size);

    bool changed = false;
    if (active_ < 0) {
        hovered_ = ImGui::IsWindowHovered() ? pickHandle(viewport, mode, mouse) : -1;
        if (hovered_ >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            active_ = hovered_;
            started_ = true;
            startWorld_ = world;
            startCenter_ = center_;
            for (int i = 0; i < 3; ++i) startAxes_[i] = axes_[i];
            startRay_ = ray;
            startMouse_ = mouse;
        }
    } else if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        // Always compute from the start of the drag, never by adding up
        // small per-frame steps: rounding errors can't pile up, and snapping
        // works on the total.
        const Mat4 result = dragResult(viewport, mode, snap, ray, mouse);
        changed = result != world;
        world = result;
    } else {
        active_ = -1;
        ended_ = true;
    }
    draw(viewport, mode);
    return changed;
}

int Gizmo::pickHandle(const GizmoViewport& viewport, GizmoMode mode, const Vec2& mouse) const {
    const Mat4 viewProjection = viewport.camera.viewProjection();
    bool inFront = false;
    const Vec2 c = GizmoMath::worldToScreen(center_, viewProjection, viewport.position, viewport.size, &inFront);
    if (!inFront) return -1;
    if (mode != GizmoMode::Rotate && std::fabs(mouse.x - c.x) < 8.0f && std::fabs(mouse.y - c.y) < 8.0f) {
        return Center;
    }
    int best = -1;
    float bestDistance = GrabPixels;
    for (int i = 0; i < 3; ++i) {
        float d = 1e9f;
        if (mode == GizmoMode::Rotate) {
            // Distance to the ring, drawn as a polygon around the axis.
            const Vec3 u = glm::normalize(glm::cross(axes_[i], std::fabs(axes_[i].y) < 0.9f ? Math::WorldUp : Math::WorldRight));
            const Vec3 v = glm::cross(axes_[i], u);
            Vec2 previous;
            for (int k = 0; k <= RingSegments; ++k) {
                const float a = Math::TwoPi * static_cast<float>(k) / RingSegments;
                const Vec2 p = GizmoMath::worldToScreen(center_ + (u * std::cos(a) + v * std::sin(a)) * length_ * 0.8f,
                                                        viewProjection, viewport.position, viewport.size);
                if (k > 0) d = std::min(d, GizmoMath::distanceToSegment(mouse, previous, p));
                previous = p;
            }
        } else {
            const Vec2 tip = GizmoMath::worldToScreen(center_ + axes_[i] * length_, viewProjection, viewport.position,
                                                      viewport.size);
            if (glm::length(tip - c) < 12.0f) continue; // pointing at the camera: too short to grab reliably
            d = GizmoMath::distanceToSegment(mouse, c, tip);
        }
        if (d < bestDistance) {
            bestDistance = d;
            best = i;
        }
    }
    return best;
}

Mat4 Gizmo::dragResult(const GizmoViewport& viewport, GizmoMode mode, const GizmoSnap& snap, const Ray& now,
                       const Vec2& mouse) const {
    const auto translate = [](const Vec3& t) { return Math::composeTRS(t, Quat(1, 0, 0, 0), Vec3(1.0f)); };
    if (mode == GizmoMode::Translate) {
        Vec3 delta(0.0f);
        if (active_ == Center) {
            // Slide in the plane through the object that faces the camera.
            const Vec3 normal = glm::normalize(viewport.camera.position - startCenter_);
            const auto t0 = intersectPlane(startRay_, startCenter_, normal);
            const auto t1 = intersectPlane(now, startCenter_, normal);
            if (!t0 || !t1) return startWorld_;
            delta = now.at(*t1) - startRay_.at(*t0);
            if (snap.enabled) delta = Vec3(GizmoMath::snap(delta.x, snap.meters), GizmoMath::snap(delta.y, snap.meters),
                                           GizmoMath::snap(delta.z, snap.meters));
        } else {
            const auto distance = GizmoMath::axisDragDistance(startRay_, now, startCenter_, startAxes_[active_]);
            if (!distance) return startWorld_;
            delta = startAxes_[active_] * (snap.enabled ? GizmoMath::snap(*distance, snap.meters) : *distance);
        }
        return translate(delta) * startWorld_;
    }
    if (mode == GizmoMode::Rotate) {
        const auto angle = GizmoMath::ringDragAngle(startRay_, now, startCenter_, startAxes_[active_]);
        if (!angle) return startWorld_;
        const float a = snap.enabled ? Math::toRadians(GizmoMath::snap(Math::toDegrees(*angle), snap.degrees)) : *angle;
        // Turn around the object's center, not the world origin: move it to
        // the origin, rotate, move it back.
        return translate(startCenter_) * glm::mat4_cast(glm::angleAxis(a, startAxes_[active_])) * translate(-startCenter_) *
               startWorld_;
    }
    // Scale, in the object's own space (world * S scales its local axes).
    Vec3 factors(1.0f);
    if (active_ == Center) {
        // Uniform: dragging right grows, left shrinks; 100 pixels doubles it.
        const float f = std::max(0.01f, 1.0f + (mouse.x - startMouse_.x) / 100.0f);
        factors = Vec3(snap.enabled ? std::max(snap.scaleStep, GizmoMath::snap(f, snap.scaleStep)) : f);
    } else {
        const auto f = GizmoMath::axisScaleFactor(startRay_, now, startCenter_, startAxes_[active_]);
        if (!f) return startWorld_;
        factors[active_] = snap.enabled ? std::max(snap.scaleStep, GizmoMath::snap(*f, snap.scaleStep)) : std::max(0.01f, *f);
    }
    return startWorld_ * Math::composeTRS(Vec3(0.0f), Quat(1, 0, 0, 0), factors);
}

void Gizmo::draw(const GizmoViewport& viewport, GizmoMode mode) const {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const Mat4 viewProjection = viewport.camera.viewProjection();
    bool inFront = false;
    const Vec2 c = GizmoMath::worldToScreen(center_, viewProjection, viewport.position, viewport.size, &inFront);
    if (!inFront) return;
    const auto screen = [&](const Vec3& p) {
        return GizmoMath::worldToScreen(p, viewProjection, viewport.position, viewport.size);
    };

    for (int i = 0; i < 3; ++i) {
        const ImU32 color = handleColor(i, hovered_, active_);
        if (mode == GizmoMode::Rotate) {
            const Vec3 u = glm::normalize(glm::cross(axes_[i], std::fabs(axes_[i].y) < 0.9f ? Math::WorldUp : Math::WorldRight));
            const Vec3 v = glm::cross(axes_[i], u);
            ImVec2 points[RingSegments];
            for (int k = 0; k < RingSegments; ++k) {
                const float a = Math::TwoPi * static_cast<float>(k) / RingSegments;
                points[k] = im(screen(center_ + (u * std::cos(a) + v * std::sin(a)) * length_ * 0.8f));
            }
            draw->AddPolyline(points, RingSegments, color, ImDrawFlags_Closed, 2.5f);
            continue;
        }
        const Vec2 tip = screen(center_ + axes_[i] * length_);
        draw->AddLine(im(c), im(tip), color, 3.0f);
        if (mode == GizmoMode::Translate) {
            // An arrowhead: a triangle pointing along the axis on screen.
            const Vec2 dir = glm::length(tip - c) > 1e-3f ? glm::normalize(tip - c) : Vec2(1.0f, 0.0f);
            const Vec2 side(-dir.y, dir.x);
            draw->AddTriangleFilled(im(tip + dir * 12.0f), im(tip + side * 6.0f), im(tip - side * 6.0f), color);
        } else {
            draw->AddRectFilled(im(tip - Vec2(5.0f)), im(tip + Vec2(5.0f)), color);
        }
    }
    if (mode != GizmoMode::Rotate) {
        const ImU32 color = handleColor(Center, hovered_, active_);
        draw->AddRect(im(c - Vec2(7.0f)), im(c + Vec2(7.0f)), color, 0.0f, 0, 2.0f);
    }
}

} // namespace Engine
