// Game state and scene dispatch.
//
// The scene model is a flat switch rather than a class hierarchy: this is a
// menu-driven game with a dozen screens and no deep object graphs, and a switch
// keeps every transition visible in one place.
//
// Two nested loops, as in the original:
//   OUTER  landmark to landmark  (~16 traversals in a normal run)
//   INNER  one sol at a time
// The trail screen autopilots between landmarks; the player may pause at any
// time and gets auto-paused on any major event.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/audio.h"
#include "core/input.h"
#include "core/persist.h"
#include "core/renderer.h"
#include "core/rng.h"
#include "game/ui.h"
#include "sim/balance.h"
#include "sim/convoy.h"
#include "sim/daystep.h"
#include "sim/trail.h"

namespace lt {

enum class Scene {
    Boot,
    MainMenu,
    Manual,
    Manifest,
    Depot,        // Chapter 1: LEO refuelling
    Tli,          // Chapter 1: trans-lunar injection window
    Cruise,       // Chapter 1: cislunar transit
    Lloi,         // Chapter 1: lunar orbit insertion
    Descent,      // Chapter 1: powered descent minigame
    Trail,        // Chapter 2: the autopilot travel screen
    TrailMenu,    // Chapter 2: the action menu
    Shop,         // Chapter 2: outpost store
    Chasm,        // Chapter 2: crossing selection
    Prospect,     // Chapter 2: ice prospecting
    Arrival,      // ending: success
    Death,        // ending: failure
    TopTen,
    Settings,
};

// Chapter 1 ascent configuration, carried from the manifest to the descent.
struct AscentPlan {
    double tliFuelKg = 0.0;
    double tliOxidiserKg = 0.0;
    double loxReserveKg = 0.0;
    double descentCargoKg = 0.0;
    double surfaceManifestKg = 0.0;
    double dryMassKg = 0.0;
    double wetMassKg = 0.0;
    int tliWindow = 0;
    double llotMassKg = 0.0;
};

// The powered descent simulation. Lives in descent.h; declared here because the
// scene owns it.
struct DescentSim;

class Game {
public:
    Game();
    ~Game();

    bool init(const std::string& title, const std::string& assetOverride,
              const std::string& userDirOverride);
    int runMainLoop();

    // --- accessors used by the scene files ---
    Renderer& ren() { return renderer_; }
    Input& in() { return input_; }
    Audio& audio() { return audio_; }
    Balance& balance() { return balance_; }
    const Balance& balance() const { return balance_; }
    PlayerData& data() { return data_; }
    RunState& runState() { return run_; }
    const RunState& runState() const { return run_; }
    Rng& rng() { return rng_; }
    LogPanel& log() { return log_; }

    int time() const { return timeMs_; }
    uint64_t frame() const { return frame_; }
    const std::string& assetDir() const { return assetDir_; }
    const std::string& userDir() const { return userDir_; }

    // Advances exactly one frame: update, draw, present. Used by the capture
    // tool to step the real game deterministically.
    void step();

    void setScene(Scene s);

    // Manual navigation, exposed for the capture tool.
    void manualNext();
    void manualPrev();

    // Starts a fresh Chapter One run with the given seed, as the main menu's
    // first entry does. Exposed so tooling can reach a genuinely populated state
    // rather than a default-constructed RunState.
    void startNewRun(uint64_t seed, Profession p);

    // Returns the index of the main-menu row the cursor is over, or -1. The menu
    // is drawn in drawMainMenu and hit-tested here, so the two must agree on
    // geometry -- which is why the constants live in one place.
    int menuClickTarget() const;
    static constexpr int kMenuX = 60;
    static constexpr int kMenuY = 76;
    static constexpr int kMenuW = 200;
    static constexpr int kMenuRowH = 18;
    static constexpr int kMenuRows = 5;
    // Marks the run as won and shows the arrival screen. Set when the convoy
    // reaches the colony landmark with surviving crew and delivered cargo.
    void declareArrival();
    Scene scene() const { return scene_; }
    void quit() { running_ = false; }
    void saveData() { data_.save(); }

    AscentPlan& ascent() { return ascent_; }
    const AscentPlan& ascent() const { return ascent_; }
    DescentSim& descent();

    // Notification line shown at the top of the screen for a few seconds.
    void notify(const std::string& text, uint8_t colour = kAmberHi);
    bool consumeNotify(std::string* out, uint8_t* colour);

    // The action menu: the original's nine commands in their original order,
    // plus "Recharge at depot" appended at the end. The recharge entry has no
    // original counterpart because the original had no batteries; everything
    // else maps one-to-one onto an Oregon Trail command with a different name
    // and the same reason to exist.
    static constexpr int kMenuEntries = 10;

    // Draws the trail pause menu's rows. Returns the clicked row, or -1. Called
    // from the draw pass so the rows are not erased by the screen clear.
    int drawTrailMenuRows();

    // Recomputes the route map's wreck markers.
    void refreshWreckMarkers();

    // Starts Chapter 2 directly at the landing depot with a default surface
    // manifest. Used by the "Resume Trail" entry, which is gated on having
    // survived a powered descent at least once.
    void beginSurfaceRun(uint64_t seed);

private:
    bool loadAssets();

    // Scene implementations, one per file.
    void enterScene(Scene s);
    void updateBoot();
    void updateMainMenu();
    void updateManual();
    void updateManifest();
    void updateDepot();
    void updateTli();
    void updateCruise();
    void updateLloi();
    void updateDescent();
    void updateTrail();
    void updateTrailMenu();
    void updateShop();
    void updateChasm();
    void updateProspect();
    void updateArrival();
    void updateDeath();
    void updateTopTen();
    void updateSettings();

    void drawBoot();
    void drawMainMenu();
    void drawManual();
    void drawManifest();
    void drawDepot();
    void drawTli();
    void drawCruise();
    void drawLloi();
    void drawDescent();
    void drawTrail();
    void drawTrailMenu();
    void drawShop();
    void drawChasm();
    void drawProspect();
    void drawArrival();
    void drawDeath();
    void drawTopTen();
    void drawSettings();

    void update();
    void draw();

    // ---- persistent ----
    Renderer renderer_;
    Input input_;
    Audio audio_;
    Balance balance_;
    PlayerData data_;
    std::string assetDir_;
    std::string userDir_;
    std::string contentDir_;
    RunState run_;
    Rng rng_;
    AscentPlan ascent_;
    std::unique_ptr<DescentSim> descent_;
    LogPanel log_;

    Scene scene_ = Scene::Boot;
    Scene returnScene_ = Scene::MainMenu;
    bool running_ = true;
    int timeMs_ = 0;
    uint64_t frame_ = 0;
    uint64_t lastFrameTicks_ = 0;

    // Notification
    std::string notifyText_;
    uint8_t notifyColour_ = kAmberHi;
    uint64_t notifyUntilFrame_ = 0;

    // Derived helpers shared by scenes
    std::vector<double> wreckMarkers_;

    // ---- Chapter 2 transient state ----
    int menuSelected_ = 0;
    std::string trailUnlockedHint_;
    int shopSelected_ = 0;
    int chasmSelected_ = 0;
    bool trailPaused_ = false;
    int trailAutoMs_ = 0;
    DayReport lastDay_;
    bool hasLastDay_ = false;
    std::vector<std::string> pendingLines_;
    bool pendingArrival_ = false;
    int pendingLandmark_ = -1;
    std::string pendingLandmarkName_;
    bool pendingChasm_ = false;
    bool pendingMercy_ = false;
    std::string mercyLine_;

    // ---- Chapter 1 transient state ----
    int manifestField_ = 0;
    int manifestCrewSlot_ = 0;
    int depotSelected_ = 0;
    int depotRow_ = 0;
    bool depotBought_ = false;
    int tliSol_ = 0;
    bool tliDone_ = false;
    int cruiseDay_ = 0;
    int lloiStage_ = 0;
    bool ch1Committed_ = false;

    // ---- Manual ----
    int manualPage_ = 0;
    int manualScroll_ = 0;

    // ---- Endings ----
    std::string endingEpitaph_;
    bool endingRecorded_ = false;
    int endingEpitaphIndex_ = 0;

    // The landmark the convoy is heading for or standing at. Shared by the
    // trail, menu, shop and chasm scenes.
    Landmark landmarkNow() const;
};

// Wraps a run: on death, records the wreck and the epitaph. Returns true if the
// run should now show the Death scene.
bool concludeIfDead(Game& g);

}  // namespace lt
