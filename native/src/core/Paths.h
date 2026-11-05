#pragma once
// Where to find engine files at runtime (shaders, textures).
//
// During development the build bakes in the absolute path of the source
// "assets" folder (XE_ASSET_DIR, set by CMake), so a program finds its files
// wherever it is started from, and edited shaders are picked up without
// copying anything. Setting the XORDOR_ASSETS environment variable overrides it.
//
// TODO(Phase 4): copy assets next to the executable when packaging a game,
// and let the asset registry resolve GUIDs instead of paths.

#include <string>

namespace Engine {

// The folder that holds shaders/, textures/ and so on (no trailing slash).
std::string assetDirectory();

// assetPath("shaders/lit.vert") -> "/path/to/native/assets/shaders/lit.vert"
std::string assetPath(const std::string& relative);

// Reads a whole text file. Returns false (and leaves `out` empty) if it can't.
bool readTextFile(const std::string& path, std::string& out);

} // namespace Engine
