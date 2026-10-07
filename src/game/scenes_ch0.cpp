// Boot splash, main menu, Top Ten, and settings.

#include <SDL.h>

#include <cmath>
#include <cstdio>

#include "game/game.h"

namespace lt {

void Game::updateBoot() {
    // ~2.2 seconds, skippable.
    if (frame_ > 132 || in().pressed(Key::Enter) || in().pressed(Key::Space) ||
        in().mousePressed(MouseButton::Left)) {
        setScene(Scene::MainMenu);
        audio_.play(Sfx::Select);
    }
}

void Game::drawBoot() {
    Renderer& r = ren();
    const int t = static_cast<int>(frame_);

    drawBackdrop(r, t, kVoidDark, kVoidNear, kMetalDeep);

    const int cx = kScreenW / 2;
    r.drawTextCenteredScaled(cx, 34, "LUNAR TRAIL", kUiWhite, 3);

    const int fade = std::min(120, t);
    if (fade > 60) {
        r.drawTextCentered(cx, 68, "THE OREGON TRAIL, REFLIGHT", kCyanHi);
    }
    if (fade > 90) {
        r.drawTextCentered(cx, 84, "One hundred and twenty kilometres from the colony site.", kUiGrey);
    }
    if (fade > 110) {
        r.drawTextCentered(cx, 94, "Most convoys do not arrive.", kRegShadow);
    }

    r.hline(50, 108, kScreenW - 100, kMetalDeep);

    r.drawTextCentered(cx, 128, "SDL2  -  320x200  -  Apache-2.0", kRegShadow);
    r.drawTextCentered(cx, 140, "All artwork CC0 / OFL. No NASA material is included.", kRegShadow);

    if ((t / 20) % 2 == 0) {
        r.drawTextCentered(cx, 166, "PRESS ANY KEY", kAmberHi);
    }
}

void Game::updateMainMenu() {
    const int itemCount = kMenuRows;
    trailUnlockedHint_ = data_.trailUnlocked() ? "  [RESUME AVAILABLE]" : "";

    // Mouse selection. Rendering lives in drawMainMenu; this only handles the
    // click, because update() runs on frames where draw() may not (and vice
    // versa, in a capture that steps frames independently).
    const int click = menuClickTarget();
    if (click >= 0) {
        menuSelected_ = click;
        audio_.play(Sfx::Select);
    }

    int sel = menuSelected_;
    if (in().pressed(Key::Up) || in().pressed(Key::W)) {
        sel = (sel - 1 + itemCount) % itemCount;
        audio_.play(Sfx::Beep);
    }
    if (in().pressed(Key::Down) || in().pressed(Key::S)) {
        sel = (sel + 1) % itemCount;
        audio_.play(Sfx::Beep);
    }
    menuSelected_ = sel;

    if (in().pressed(Key::Enter) || in().pressed(Key::Space)) {
        audio_.play(Sfx::Select);
        switch (sel) {
            case 0: {
                // Either a fresh Chapter One run, or a seeded replay.
                const uint64_t seed = rng_.nextU32();
                runState() = newRun(seed, Profession::MissionSpecialist, balance_);
                runState().seedText = formatSeed(seed);
                runState().energyKwh = balance_.batteryCapacityKwh;
                log().clear();
                ascent_ = AscentPlan{};
                setScene(Scene::Manifest);
                break;
            }
            case 1:
                if (data_.trailUnlocked()) {
                    // Resume: land with a default surface manifest so Chapter 2
                    // can be played immediately.
                    const uint64_t seed = rng_.nextU32();
                    beginSurfaceRun(seed);
                } else {
                    manualPage_ = 0;
                    manualScroll_ = 0;
                    setScene(Scene::Manual);
                }
                break;
            case 2:
                setScene(Scene::TopTen);
                break;
            case 3:
                setScene(Scene::Settings);
                break;
            case 4:
                quit();
                break;
            default:
                break;
        }
    }
}

void Game::drawMainMenu() {
    Renderer& r = ren();
    const int t = static_cast<int>(frame_);

    drawBackdrop(r, t, kVoidDark, kVoidNear, kMetalDeep);

    // Horizon line and a suggestion of regolith, high enough to clear the menu
    // block (which starts at kMenuY and is five rows of kMenuRowH).
    const int horizon = 30;
    r.hline(0, horizon, kScreenW, kMetalDeep);
    for (int i = 0; i < 40; ++i) {
        const int x = (i * 17) % kScreenW;
        const int h = 1 + (i % 3);
        r.fillRect(x, horizon, 8, h, kRegDeep);
    }

    const int cx = kScreenW / 2;
    r.drawTextCenteredScaled(cx, 34, "LUNAR TRAIL", kUiWhite, 2);
    r.drawTextCentered(cx, 58, "SOUTH POLE SURFACE LOGISTICS", kCyanHi);
    r.hline(70, 68, kScreenW - 140, kMetalDeep);

    r.drawText(4, 190, "v0.1.0", kRegShadow);
    r.drawTextRight(kScreenW - 4, 190, "F11 fullscreen   M sound", kRegShadow);

    // The menu itself. Starts at y=54 so the first row clears the title block,
    // and uses an 18px row height for five rows down to y=144.
    std::vector<std::string> items = {
        "Travel The Trail",
        "Learn About The Trail",
        "The Top Ten",
        "Settings",
        "Quit",
    };
    std::vector<std::string> notes(5, std::string());
    std::vector<bool> enabled(5, true);
    notes[0] = trailUnlockedHint_;
    notes[3] = audio_.enabled() ? "SND" : "OFF";
    if (data_.trailUnlocked()) {
        items[1] = "Resume Trail (skip to surface)";
        notes[1] = "DEBUG";
    }
    drawMenu(r, kMenuX, kMenuY, kMenuW, items, notes, menuSelected_, in(), enabled,
             kCyan, kMenuRowH);
}

void Game::updateTopTen() {
    if (in().pressed(Key::Enter) || in().pressed(Key::Escape) ||
        in().mousePressed(MouseButton::Left)) {
        setScene(Scene::MainMenu);
        audio_.play(Sfx::Click);
    }
}

void Game::drawTopTen() {
    Renderer& r = ren();
    r.fillRect(0, 0, kScreenW, kScreenH, kUiBlack);
    drawBackdrop(r, static_cast<int>(frame_), kVoidNear, kVoidDark, kMetalDeep);

    r.drawTextShadow(kScreenW / 2, 8, "THE TOP TEN", kAmberHi, kAmberDeep);
    r.drawTextCentered(kScreenW / 2, 20, "EXPEDITIONS THAT MADE IT", kRegShadow);

    // Two 7px rows per entry at a 16px pitch. The strip behind each entry is 15px
    // tall, not 11: at 11 it stopped halfway through the sub-line, so the detail
    // row appeared to be colliding with the entry above it.
    const auto& board = data_.topTen();
    int y = 28;
    for (size_t i = 0; i < board.size() && i < 10; ++i) {
        const auto& e = board[i];
        const bool isPlayer = e.seedText != "-";

        r.fillRect(12, y - 2, kScreenW - 24, 15, kMetalBlack);
        r.hline(12, y - 2, kScreenW - 24, kMetalDeep);

        r.drawText(15, y, fmtInt(static_cast<int>(i) + 1) + ".", kRegShadow);
        r.drawText(28, y, e.crewName, isPlayer ? kCyanHi : kUiWhite);
        r.drawTextRight(kScreenW - 15, y, fmtInt(e.score), isPlayer ? kCyanHi : kAmberHi);

        const std::string prof = professionName(static_cast<Profession>(
            std::min(2, std::max(0, e.professionIndex))));
        const std::string seedStr = e.seedText != "-" ? ("  seed " + e.seedText) : std::string();
        const std::string sub = fmtInt(e.sol) + " sol  " + fmtKm(e.km) + "  " + prof + seedStr;
        // Legible grey, not the background shadow colour: at kRegShadow the sol,
        // distance and profession were effectively unreadable.
        r.drawText(40, y + 8, sub, kUiGrey);

        y += 16;
    }

    // The tenth entry's sub-line ends at y=187, so the prompt goes below that.
    // At kScreenH-14 it was drawn straight through it.
    r.hline(12, y - 2, kScreenW - 24, kMetalDeep);
    r.drawTextCentered(kScreenW / 2, kScreenH - 10, "ENTER to return", kRegShadow);
}

void Game::updateSettings() {
    if (in().pressed(Key::Escape) || in().pressed(Key::Enter)) {
        saveData();
        setScene(Scene::MainMenu);
        audio_.play(Sfx::Click);
    }
    if (in().pressed(Key::S) || in().pressed(Key::M)) {
        audio_.setEnabled(!audio_.enabled());
        data_.setSoundEnabled(audio_.enabled());
        saveData();
        audio_.play(Sfx::Beep);
    }
    if (in().pressed(Key::V)) {
        data_.setVsync(!data_.vsyncEnabled());
        ren().setVsync(data_.vsyncEnabled());
        saveData();
        audio_.play(Sfx::Beep);
    }
    if (in().pressed(Key::F11)) ren().toggleFullscreen();
}

void Game::drawSettings() {
    Renderer& r = ren();
    r.fillRect(0, 0, kScreenW, kScreenH, kUiBlack);

    r.drawTextShadow(kScreenW / 2, 12, "SETTINGS", kAmberHi, kAmberDeep);

    int y = 40;
    struct Row { const char* label; std::string value; const char* hint; };
    const std::vector<Row> rows = {
        {"SOUND",          audio_.enabled() ? "ON" : "OFF",           "press S"},
        {"VSYNC",          data_.vsyncEnabled() ? "ON" : "OFF",        "press V"},
        {"FULLSCREEN",     ren().fullscreen() ? "ON" : "OFF",          "F11"},
        {"USER DATA",      userDir(),                                 ""},
        {"ASSET DIRECTORY",assetDir(),                                ""},
    };

    for (const auto& row : rows) {
        r.drawText(30, y, row.label, kUiGrey);
        r.drawText(130, y, row.value, kCyan);
        r.drawTextRight(kScreenW - 30, y, row.hint, kRegShadow);
        y += 12;
    }

    r.hline(30, y + 6, kScreenW - 60, kMetalDeep);
    r.drawTextCentered(kScreenW / 2, y + 14,
                       "Progress is stored as plain text in the user directory.", kRegShadow);
    r.drawTextCentered(kScreenW / 2, y + 24,
                       "Wrecks and the Top Ten can be edited or deleted there.", kRegShadow);

    if ((static_cast<int>(frame_) / 24) % 2 == 0) {
        r.drawTextCentered(kScreenW / 2, kScreenH - 16, "ESC to return", kAmberHi);
    }
}

}  // namespace lt
