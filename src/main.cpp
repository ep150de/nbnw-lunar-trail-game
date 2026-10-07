// Lunar Trail -- entry point.
//
// Deliberately thin: argument handling and Game construction. Everything with a
// rule attached lives in sim/ (no SDL) so that it can be tested and, more
// usefully, swept by the balance harness in tools/.

#include <SDL.h>

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

#include "game/game.h"

namespace {

void printUsage(const char* exe) {
    std::cout <<
        "Lunar Trail -- an Oregon Trail survival logistics game, reflight for\n"
        "the lunar south pole.\n"
        "\n"
        "Usage: " << exe << " [options]\n"
        "\n"
        "Options:\n"
        "  --assets <dir>     Load art from <dir> instead of searching for assets/\n"
        "  --userdata <dir>   Read and write progress under <dir>\n"
        "  --seed <hex>       Start with a specific route seed (64-bit hex)\n"
        "  --novsync          Disable vsync\n"
        "  --nosound          Disable sound\n"
        "  --fullscreen       Start fullscreen\n"
        "  --help             Show this message\n"
        "\n"
        "Controls are listed in the game's Field Manual (main menu, option 2).\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::string title = "Lunar Trail";
    std::string assetOverride;
    std::string userOverride;
    std::string seedArg;
    bool vsync = true;
    bool sound = true;
    bool fullscreen = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&](const char* what) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "error: " << what << " requires a value\n";
                std::exit(2);
            }
            return argv[++i];
        };

        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--assets") {
            assetOverride = next("--assets");
        } else if (arg == "--userdata") {
            userOverride = next("--userdata");
        } else if (arg == "--seed") {
            seedArg = next("--seed");
        } else if (arg == "--novsync") {
            vsync = false;
        } else if (arg == "--nosound") {
            sound = false;
        } else if (arg == "--fullscreen") {
            fullscreen = true;
        } else {
            std::cerr << "error: unrecognised option '" << arg << "'\n\n";
            printUsage(argv[0]);
            return 2;
        }
    }

    // Parsed up front so a bad seed is an error before a window appears.
    uint64_t forcedSeed = 0;
    bool haveForcedSeed = false;
    if (!seedArg.empty()) {
        if (!lt::parseSeed(seedArg, &forcedSeed)) {
            std::cerr << "error: --seed expects hex digits, e.g. 8A3F1C09DE2B7745\n";
            return 2;
        }
        haveForcedSeed = true;
    }

    {
        lt::Game game;
        if (!game.init(title, assetOverride, userOverride)) {
            std::cerr << "error: could not start. SDL said: " << SDL_GetError() << "\n";
            return 1;
        }
        game.audio().setEnabled(sound);
        game.ren().setVsync(vsync);
        if (fullscreen) game.ren().toggleFullscreen();
        if (haveForcedSeed) game.rng().seed(forcedSeed);

        const int rc = game.runMainLoop();
        game.saveData();
        return rc;
    }
}
