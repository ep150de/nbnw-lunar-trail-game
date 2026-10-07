#include "game/game.h"

#include <SDL.h>

#include <algorithm>
#include <filesystem>
#include <iostream>

#include "game/descent.h"
#include "sim/trail.h"

namespace lt {

namespace fs = std::filesystem;

Game::Game() = default;
Game::~Game() = default;

bool Game::init(const std::string& title, const std::string& assetOverride,
                const std::string& userDirOverride) {
    if (!renderer_.init(title)) return false;

    assetDir_ = findAssetDir(assetOverride);
    userDir_ = userDirOverride.empty() ? (assetDir_ + "/../userdata") : userDirOverride;

    // content/ sits next to assets/ in the repo, but is staged separately.
    {
        std::error_code ec;
        const fs::path p = fs::path(assetDir_).parent_path() / "content";
        if (fs::exists(p, ec)) {
            contentDir_ = p.string();
        } else {
            contentDir_ = assetDir_ + "/../content";
        }
    }

    balance_.load(contentDir_ + "/balance.json");

    std::string err;
    if (!data_.load(userDir_, &err)) {
        std::cerr << "player data: " << err << " (progress will not persist)\n";
    }

    audio_.init(assetDir_);
    audio_.setEnabled(data_.soundEnabled());
    audio_.setVolume(70);
    renderer_.setVsync(data_.vsyncEnabled());
    audio_.loadBank(assetDir_);

    if (!loadAssets()) {
        std::cerr << "warning: some art is missing. Run tools/gen_art.py to "
                     "regenerate assets/sprites.\n";
    }

    // One random seed for a fresh journey. The player sees it, can note it, and
    // can type it back in to replay the identical route.
    rng_.seed(static_cast<uint64_t>(SDL_GetTicks()) ^
              (static_cast<uint64_t>(SDL_GetPerformanceCounter()) << 13));

    refreshWreckMarkers();
    setScene(Scene::Boot);
    return true;
}

bool Game::loadAssets() {
    bool all = true;
    const char* required[] = {
        "sprites/rover.png",
        "sprites/terrain_sunlit.png",
        "sprites/terrain_transition.png",
        "sprites/terrain_psr.png",
        "sprites/starfield.png",
        "sprites/outpost.png",
        "sprites/dome.png",
        "sprites/crew_a.png",
        "sprites/crew_b.png",
        "sprites/ice_seam.png",
        "sprites/stake.png",
    };
    for (const char* p : required) {
        if (renderer_.load(assetDir_ + "/" + p) == nullptr) all = false;
    }
    return all;
}

DescentSim& Game::descent() {
    if (descent_ == nullptr) {
        descent_ = std::make_unique<DescentSim>();
    }
    return *descent_;
}

void Game::step() {
    input_.beginFrame();
    const Uint64 now = SDL_GetTicks();
    timeMs_ = static_cast<int>(now - lastFrameTicks_);
    if (timeMs_ > 100) timeMs_ = 100;
    lastFrameTicks_ = now;
    ++frame_;
    update();
    draw();
    renderer_.present();
    input_.endFrame();
}

int Game::menuClickTarget() const {
    if (!input_.mousePressed(MouseButton::Left)) return -1;
    const int rel = input_.mouseY() - kMenuY;
    if (rel < 0 || input_.mouseX() < kMenuX || input_.mouseX() >= kMenuX + kMenuW) return -1;
    const int idx = rel / kMenuRowH;
    return (idx < 0 || idx >= kMenuRows) ? -1 : idx;
}

void Game::startNewRun(uint64_t seed, Profession p) {
    runState() = newRun(seed, p, balance_);
    runState().seedText = formatSeed(seed);
    runState().energyKwh = balance_.batteryCapacityKwh;
    log().clear();
    ascent_ = AscentPlan{};
    menuSelected_ = 0;
    manifestField_ = 0;
    manifestCrewSlot_ = 0;
}

void Game::manualNext() { ++manualPage_; manualScroll_ = 0; }

void Game::manualPrev() { if (manualPage_ > 0) --manualPage_; }

void Game::setScene(Scene s) {
    if (scene_ != s) {
        scene_ = s;
        lastFrameTicks_ = SDL_GetTicks();
    }
}

void Game::notify(const std::string& text, uint8_t colour) {
    notifyText_ = text;
    notifyColour_ = colour;
    notifyUntilFrame_ = frame_ + 180;   // ~3 seconds at 60fps
}

bool Game::consumeNotify(std::string* out, uint8_t* colour) {
    if (notifyText_.empty() || frame_ > notifyUntilFrame_) {
        notifyText_.clear();
        return false;
    }
    if (out != nullptr) *out = notifyText_;
    if (colour != nullptr) *colour = notifyColour_;
    notifyText_.clear();
    return true;
}

void Game::beginSurfaceRun(uint64_t seed) {
    run_ = newRun(seed, Profession::MissionSpecialist, balance_);
    run_.seedText = formatSeed(seed);
    run_.energyKwh = balance_.batteryCapacityKwh;
    run_.progress.zone = Zone::Sunlit;

    // A deliberately mediocre opening manifest. The point of the Resume entry
    // is to get to the trail quickly, not to hand the player a free win.
    const Balance& b = balance_;
    run_.credits = 2400.0;
    run_.stock.foodKg = 90.0;
    run_.stock.waterL = 700.0;
    run_.stock.o2Kg = 60.0;
    run_.stock.fuelKg = 60.0;
    run_.stock.oxidiserKg = 360.0;
    run_.stock.suitSets = 8;
    run_.stock.cuttingCharges = 20;
    run_.stock.cargoKg = 120.0;
    run_.hardware.sparesWheel = 2;
    run_.hardware.sparesBogie = 1;
    run_.hardware.sparesSeal = 1;

    // Clamp to the payload cap rather than silently carrying an over-weight rig.
    if (run_.totalPayloadKg(b) > b.payloadCapKg) {
        const double excess = run_.totalPayloadKg(b) - b.payloadCapKg;
        const double take = std::min(excess, run_.stock.waterL);
        run_.stock.waterL -= take;
    }

    ascent_ = AscentPlan{};
    log().clear();
    menuSelected_ = 0;
    shopSelected_ = 0;
    trailPaused_ = true;
    hasLastDay_ = false;
    pendingArrival_ = false;
    pendingChasm_ = false;
    notify("Surface run resumed. You are at Shackleton Rim Depot.", kCyanHi);
    setScene(Scene::TrailMenu);
}

void Game::declareArrival() {
    if (runState().outcome == Outcome::Playing) {
        runState().outcome = Outcome::Won;
    }
    // A win is recorded the same way a loss is: through the Death scene, which
    // is really the "run concluded" scene. Keeps one recording path instead of
    // two that can drift apart.
    endingRecorded_ = false;
    endingEpitaph_.clear();
    setScene(Scene::Death);
}

void Game::refreshWreckMarkers() {
    wreckMarkers_.clear();
    for (const auto& w : data_.wrecks()) {
        if (!w.scavenged) wreckMarkers_.push_back(w.km);
    }
}

int Game::runMainLoop() {
    lastFrameTicks_ = SDL_GetTicks();

    while (running_) {
        input_.beginFrame();

        const Uint64 now = SDL_GetTicks();
        timeMs_ = static_cast<int>(now - lastFrameTicks_);
        // Clamp: a stall (window drag, breakpoint) must not fast-forward the sim.
        if (timeMs_ > 100) timeMs_ = 100;
        lastFrameTicks_ = now;
        ++frame_;

        if (input_.pressed(Key::Escape) && scene_ != Scene::MainMenu &&
            scene_ != Scene::Boot && scene_ != Scene::Manual && scene_ != Scene::Death &&
            scene_ != Scene::Arrival) {
            // Escape backs out one level. On the trail it opens the pause menu,
            // which is the original's "Press RETURN to size up the situation".
            if (scene_ == Scene::TrailMenu) {
                setScene(Scene::Trail);
                trailPaused_ = false;
            } else if (scene_ == Scene::Shop || scene_ == Scene::Chasm ||
                       scene_ == Scene::Prospect) {
                setScene(Scene::TrailMenu);
            } else if (scene_ == Scene::Trail) {
                setScene(Scene::TrailMenu);
                trailPaused_ = true;
            } else {
                returnScene_ = scene_;
                setScene(Scene::MainMenu);
            }
            audio_.play(Sfx::Click);
        }

        update();
        draw();

        renderer_.present();
        input_.endFrame();

        if (input_.quitRequested()) running_ = false;
    }

    saveData();
    return 0;
}

void Game::update() {
    switch (scene_) {
        case Scene::Boot:       updateBoot(); break;
        case Scene::MainMenu:   updateMainMenu(); break;
        case Scene::Manual:     updateManual(); break;
        case Scene::Manifest:   updateManifest(); break;
        case Scene::Depot:      updateDepot(); break;
        case Scene::Tli:        updateTli(); break;
        case Scene::Cruise:     updateCruise(); break;
        case Scene::Lloi:       updateLloi(); break;
        case Scene::Descent:    updateDescent(); break;
        case Scene::Trail:      updateTrail(); break;
        case Scene::TrailMenu:  updateTrailMenu(); break;
        case Scene::Shop:       updateShop(); break;
        case Scene::Chasm:      updateChasm(); break;
        case Scene::Prospect:   updateProspect(); break;
        case Scene::Arrival:    updateArrival(); break;
        case Scene::Death:      updateDeath(); break;
        case Scene::TopTen:     updateTopTen(); break;
        case Scene::Settings:   updateSettings(); break;
    }
    if (concludeIfDead(*this)) setScene(Scene::Death);
}

void Game::draw() {
    renderer_.beginFrame();

    switch (scene_) {
        case Scene::Boot:       drawBoot(); break;
        case Scene::MainMenu:   drawMainMenu(); break;
        case Scene::Manual:     drawManual(); break;
        case Scene::Manifest:   drawManifest(); break;
        case Scene::Depot:      drawDepot(); break;
        case Scene::Tli:        drawTli(); break;
        case Scene::Cruise:     drawCruise(); break;
        case Scene::Lloi:       drawLloi(); break;
        case Scene::Descent:    drawDescent(); break;
        case Scene::Trail:      drawTrail(); break;
        case Scene::TrailMenu:  drawTrailMenu(); break;
        case Scene::Shop:       drawShop(); break;
        case Scene::Chasm:      drawChasm(); break;
        case Scene::Prospect:   drawProspect(); break;
        case Scene::Arrival:    drawArrival(); break;
        case Scene::Death:      drawDeath(); break;
        case Scene::TopTen:     drawTopTen(); break;
        case Scene::Settings:   drawSettings(); break;
    }

    // Transient notification banner, drawn last so it sits above everything.
    std::string note;
    uint8_t noteColour = kAmberHi;
    if (consumeNotify(&note, &noteColour)) {
        const int w = Renderer::textWidth(note) + 12;
        const int x = (kScreenW - w) / 2;
        renderer_.panel(x, 2, w, 13, kMetalBlack, noteColour);
        renderer_.drawText(x + 6, 5, note, noteColour);
    }

    renderer_.present();
}

bool concludeIfDead(Game& g) {
    // Only a loss diverts to the epitaph screen. This used to divert on any
    // outcome that was not Playing, so the moment a run was won the game threw
    // the player out of the arrival screen and into the death screen: the
    // winning ending could never be seen.
    if (g.runState().outcome != Outcome::Lost) return false;

    // And never out of a screen that is already showing an ending.
    switch (g.scene()) {
        case Scene::Death:
        case Scene::Arrival:
        case Scene::TopTen:
            return false;
        default:
            return true;
    }
}

}  // namespace lt
