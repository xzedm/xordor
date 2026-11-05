#pragma once
// Engine key and mouse button codes.
//
// The numbers deliberately match GLFW's (GLFW_KEY_A is 65, and so on), so the
// window layer can convert with a cast instead of a lookup table, while engine
// and game code never include GLFW. Window.cpp checks that they still match
// with static_asserts.
//
// Codes describe the *position* of a key on a US layout, not the character it
// types: Key::W is the key left of E on any layout, which is what WASD movement
// wants. Text input (typing names) will use a separate character event later.

namespace Engine {

enum class Key : int {
    Unknown = -1,
    Space = 32, Apostrophe = 39, Comma = 44, Minus = 45, Period = 46, Slash = 47,
    Num0 = 48, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    Semicolon = 59, Equal = 61,
    A = 65, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    LeftBracket = 91, Backslash = 92, RightBracket = 93, GraveAccent = 96,
    Escape = 256, Enter, Tab, Backspace, Insert, Delete, Right, Left, Down, Up, PageUp, PageDown, Home, End,
    CapsLock = 280, ScrollLock, NumLock, PrintScreen, Pause,
    F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    KeypadAdd = 334, KeypadSubtract = 333,
    LeftShift = 340, LeftControl, LeftAlt, LeftSuper, RightShift, RightControl, RightAlt, RightSuper, Menu,
};

// Number of slots needed to store state for every key code (highest code + 1).
inline constexpr int KeyCount = static_cast<int>(Key::Menu) + 1;

enum class MouseButton : int { Left = 0, Right = 1, Middle = 2 };
inline constexpr int MouseButtonCount = 8; // GLFW supports up to 8 buttons

} // namespace Engine
