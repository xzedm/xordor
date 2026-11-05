// The Xordor editor.
//
//   editor                                     opens assets/scenes/sandbox.scene.json
//   editor scenes/other.scene.json             opens another scene
//   editor --select Ball --gizmo rotate        start with something selected
//   editor --frames 120 --screenshot out.png   run 120 frames, save the last, quit (for checks)

#include "core/Log.h"
#include "editor/EditorApp.h"

#include <cstdlib>
#include <exception>
#include <string>

using namespace Engine;

int main(int argc, char** argv) {
    EditorOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--frames" && hasValue) {
            options.frames = std::atoll(argv[++i]);
        } else if (arg == "--screenshot" && hasValue) {
            options.screenshotPath = argv[++i];
        } else if (arg == "--select" && hasValue) {
            options.select = argv[++i];
        } else if (arg == "--gizmo" && hasValue) {
            options.gizmo = argv[++i];
        } else if (!arg.empty() && arg[0] != '-') {
            options.scenePath = arg;
        } else {
            ENGINE_ERROR("Unknown option ", arg,
                         ". Usage: editor [scene.scene.json] [--select NAME] [--gizmo move|rotate|scale] "
                         "[--frames N --screenshot FILE.png]");
            return 2;
        }
    }
    try {
        ApplicationSettings settings;
        settings.window.title = "Xordor Editor";
        settings.window.width = 1600;
        settings.window.height = 900;
        settings.maxFrames = options.frames;
        settings.logFile = "editor.log";
        EditorApp app(settings, options);
        return app.run();
    } catch (const std::exception& error) {
        ENGINE_ERROR("Fatal: ", error.what());
        return 1;
    }
}
