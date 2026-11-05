#include "core/Paths.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

#ifndef XE_ASSET_DIR
#define XE_ASSET_DIR "assets"
#endif

namespace Engine {

std::string assetDirectory() {
    if (const char* overridePath = std::getenv("XORDOR_ASSETS"); overridePath && *overridePath) {
        return overridePath;
    }
    return XE_ASSET_DIR;
}

std::string assetPath(const std::string& relative) {
    return assetDirectory() + "/" + relative;
}

bool readTextFile(const std::string& path, std::string& out) {
    out.clear();
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    std::ostringstream text;
    text << file.rdbuf();
    out = text.str();
    return true;
}

} // namespace Engine
