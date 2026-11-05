#pragma once
// Math: short names for GLM types, our coordinate conventions, and the few
// helpers the engine needs.
//
// Why wrap GLM at all? The rest of the engine says Engine::Vec3 instead of
// glm::vec3, so the conventions below live in one place, and the helpers can be
// named for what the engine needs ("composeTRS") rather than for how GLM spells it.
//
// Conventions (the same as OpenGL and glTF, so imported models need no fix-ups):
//   * Right-handed coordinates. +X right, +Y up, -Z forward (into the screen).
//   * Units are meters. Angles are radians in code (degrees only in UIs).
//   * Matrices are column-major and multiply column vectors: p' = M * p.
//     So "A * B" applies B first, then A.

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Engine {

using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;
using IVec2 = glm::ivec2;
using Mat3 = glm::mat3;
using Mat4 = glm::mat4;
using Quat = glm::quat; // a rotation; never needs re-normalizing as often as Euler angles drift

namespace Math {

inline constexpr float Pi = 3.14159265358979323846f;
inline constexpr float TwoPi = 2.0f * Pi;
inline constexpr float HalfPi = 0.5f * Pi;
// Tolerance for comparing floats. Rounding makes exact == unreliable after any math.
inline constexpr float Epsilon = 1e-5f;

// World axes, written once so code reads "Math::WorldUp" instead of a mystery Vec3.
inline const Vec3 WorldRight{1.0f, 0.0f, 0.0f};
inline const Vec3 WorldUp{0.0f, 1.0f, 0.0f};
inline const Vec3 WorldForward{0.0f, 0.0f, -1.0f};

inline constexpr float toRadians(float degrees) { return degrees * (Pi / 180.0f); }
inline constexpr float toDegrees(float radians) { return radians * (180.0f / Pi); }

template <typename T>
constexpr T clamp(T value, T low, T high) {
    return value < low ? low : (value > high ? high : value);
}

// Linear interpolation: t = 0 gives a, t = 1 gives b. Works for float, Vec2, Vec3...
template <typename T>
constexpr T lerp(const T& a, const T& b, float t) {
    return a + (b - a) * t;
}

bool nearlyEqual(float a, float b, float epsilon = Epsilon);
bool nearlyEqual(const Vec3& a, const Vec3& b, float epsilon = Epsilon);
bool nearlyEqual(const Mat4& a, const Mat4& b, float epsilon = Epsilon);
// Two quaternions q and -q are the same rotation, so both count as equal.
bool nearlyEqual(const Quat& a, const Quat& b, float epsilon = Epsilon);

// A rotation from Euler angles in degrees: x = pitch, y = yaw, z = roll.
// Yaw first, then pitch, then roll, each about the object's own (already
// turned) axes: the usual order for cameras and characters, so "turn around,
// then look up" behaves as expected.
Quat quatFromEulerDegrees(const Vec3& pitchYawRollDegrees);

// The model matrix of an object: scale it, then rotate it, then move it.
// M = T * R * S, because column vectors read right to left.
Mat4 composeTRS(const Vec3& translation, const Quat& rotation, const Vec3& scale);

// Points are affected by translation; directions (w = 0) are not.
Vec3 transformPoint(const Mat4& m, const Vec3& point);
Vec3 transformDirection(const Mat4& m, const Vec3& direction);

// The matrix that turns object-space normals into world space: the inverse
// transpose of the model matrix's upper 3x3. Using the model matrix itself
// works for rotations and uniform scale, but a non-uniform scale (a sphere
// squashed into an egg) would tilt slanted normals the wrong way, off the
// surface. Shaders still normalize the result, since scale changes its length.
Mat3 normalMatrix(const Mat4& model);

// Camera matrices for OpenGL: clip-space depth goes from -1 (near) to +1 (far).
Mat4 perspective(float fovYRadians, float aspect, float zNear, float zFar);
Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up = WorldUp);

} // namespace Math
} // namespace Engine
