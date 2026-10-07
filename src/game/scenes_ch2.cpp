// Chapter Two: the trail itself.
//
// This file contains the four screens that make up the loop, in the original's
// structure: the autopiloted travel screen, the pause menu (its nine options,
// with the original's command names preserved), the outpost store, and the
// chasm crossing.

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "game/game.h"
#include "sim/depot.h"
#include "sim/health.h"
#include "sim/daystep.h"
#include "sim/hazards.h"
#include "sim/trail.h"

namespace lt {

namespace {

// The nine actions from the travel screen. The wording keeps the original's
// numbering and structure; only the nouns change.
const char* kMenuLabels[Game::kMenuEntries] = {
    "Continue on trail",
    "Check supplies",
    "Look at route map",
    "Change pace",
    "Change rations",
    "Stop to rest",
    "Attempt to barter",
    "Talk to the crew",
    "Prospect for ice",
    "Recharge at depot",
};

}  // namespace

// ---------------------------------------------------------------- trail

void Game::updateTrail() {
    if (!hasLastDay_) {
        hasLastDay_ = true;
        return;
    }

    // Autopilot cadence: one sol every ~40 frames, and faster when you have
    // already paused this run. The original advanced roughly every two seconds.
    if (in().pressed(Key::Enter) || in().pressed(Key::Space)) {
        setScene(Scene::TrailMenu);
        trailPaused_ = true;
        audio_.play(Sfx::Click);
        return;
    }
    // Direct pace hotkeys, as advertised in the manual.
    if (in().pressed(Key::R))        runState().pace = Pace::FullBurn;
    if (in().pressed(Key::LShift))   runState().pace = Pace::Cruise;
    if (in().pressed(Key::U))        runState().pace = Pace::Sprint;
    if (in().pressed(Key::Down))     runState().pace = (runState().pace == Pace::Cruise) ? Pace::Sprint : Pace::Cruise;

    trailAutoMs_ += 1;
    if (trailAutoMs_ < 26) return;
    trailAutoMs_ = 0;

    // ---- advance one sol ----
    lastDay_ = advanceSol(runState(), rng(), balance_, true);
    hasLastDay_ = true;

    if (lastDay_.reachedLandmark) {
        pendingArrival_ = true;
        pendingLandmark_ = lastDay_.landmarkIndex;
        pendingLandmarkName_ = lastDay_.landmarkName;
        audio_.play(Sfx::Radio);
        setScene(Scene::TrailMenu);
        trailPaused_ = true;
        notify("Arrived: " + pendingLandmarkName_, kAmberHi);
        return;
    }

    // Arrived at the colony? Conclude the run.
    {
        const auto& t = trail();
        const Landmark& dest = t.back();
        if (runState().progress.kmTravelled >= dest.km - 0.01) {
            declareArrival();
            return;
        }
    }

    // Standing at the lip of a chasm. The day loop latched awaitingChasm; here we
    // hand control to the crossing module.
    if (lastDay_.atChasm && runState().progress.awaitingChasm) {
        const Landmark& next = landmarkNow();
        log().push(next.name + ": " +
                   std::to_string(static_cast<int>(next.chasmaDepthM)) + " metres down, " +
                   std::to_string(static_cast<int>(next.chasmaSpanM)) + " metres across. " +
                   (next.guidedAvailable ? "A support service operates here."
                                          : "No support service operates here."));
        chasmSelected_ = 0;
        setScene(Scene::Chasm);
        audio().play(Sfx::Radio);
    }
}

void Game::drawTrail() {
    Renderer& r = ren();
    const Balance& b = balance_;
    const bool psr = runState().progress.zone == Zone::Psr;

    // ---- terrain strip ----
    // The original shipped two backgrounds and palette-swapped them. We do the
    // same thing: one strip, relit by zone.
    const uint8_t skyA = psr ? kVoidNear : kVoidRim;
    const uint8_t skyB = psr ? kVoidBlack : kVoidDeep;
    for (int y = 0; y < 64; ++y) {
        const uint8_t rowCol = ((y / 8) % 2 == 0) ? skyA : skyB;
        r.hline(0, y, kScreenW, rowCol);
    }
    // Stars only where there is sky and no sun glare; a shadowed region shows
    // more of them, which is the honest inversion.
    for (int i = 0; i < 70; ++i) {
        const int sx = (i * 149) % kScreenW;
        const int sy = (i * 83) % 58;
        r.pixel(sx, sy, psr ? kUiWhite : kMetalDeep);
    }
    if (!psr) {
        r.fillCircle(250, 20, 8, kHiWhite);
        r.fillCircle(250, 20, 9, kAmberHi);
    }

    // Ground, lit by zone.
    for (int y = 64; y < 104; ++y) {
        const int depth = y - 64;
        uint8_t col;
        if (psr) {
            col = depth < 2 ? kVoidRim : (depth < 8 ? kVoidDeep :
                  (depth < 20 ? kVoidDark : kVoidNear));
        } else {
            col = depth < 2 ? kRegBright : (depth < 8 ? kRegMid :
                  (depth < 20 ? kRegDark : kRegDeep));
        }
        r.hline(0, y, kScreenW, col);
    }
    // Craters and boulders.
    for (int i = 0; i < 12; ++i) {
        const int cx = ((i * 61) + static_cast<int>(runState().progress.kmTravelled * 3.0)) % kScreenW;
        const int cw = 10 + (i * 5) % 16;
        for (int dx = 0; dx < cw / 2; ++dx) {
            const int hh = 3 + static_cast<int>(3.0 * (1.0 - static_cast<double>(dx) / (cw / 2.0)));
            r.vline(cx - cw / 2 + dx, 64 - hh, hh, psr ? kVoidMid : kRegMid);
        }
    }

    // The convoy.
    {
        const int cx = 150;
        const int cy = 64 - 3;
        r.fillRect(cx - 12, cy - 8, 24, 8, kMetalMid);
        r.fillRect(cx - 12, cy - 8, 24, 2, kMetalHi);
        r.fillRect(cx + 8, cy - 6, 5, 5, kMetalDark);
        r.pixel(cx + 11, cy - 4, kCyanHi);
        r.hline(cx - 16, cy, 33, kMetalDeep);
        for (int i = 0; i < 6; ++i) {
            const int wx = cx - 12 + i * 5;
            r.fillCircle(wx, cy + 2, 2, kMetalBlack);
        }
        // Exhaust plume on a chemical burn.
        if (runState().pace == Pace::FullBurn) {
            r.fillRect(cx - 26, cy - 6, 13, 4, kAmber);
            r.fillRect(cx - 32, cy - 5, 6, 2, kAmberHi);
        }
    }

    // ---- top status bar ----
    //
    // Measured slots rather than hardcoded x positions: the previous fixed
    // offsets collided once the sol count grew past two digits and the zone name
    // ran into the pace readout.
    r.fillRect(0, 0, kScreenW, 11, kUiBlack);
    const int barW = kScreenW;
    const int colSol   = 3;
    const int colZone  = colSol + Renderer::textWidth("SOL 000") + 6;
    const int colPace  = 150;
    const int colRation= colPace + 46;
    const int distW    = Renderer::textWidth("000.0 km / 000.0 km") + 6;

    r.drawText(colSol, 2, "SOL " + fmtInt(runState().progress.sol), kUiGrey);
    r.drawText(colZone, 2, zoneName(runState().progress.zone),
               psr ? kVoidRim : kCyanHi);
    r.drawText(colPace, 2, paceName(runState().pace),
               runState().pace == Pace::FullBurn ? kAmberHi : kUiWhite);
    r.drawText(colRation, 2, rationsShort(runState().rations), kUiGrey);
    r.drawTextRight(barW - 3, 2,
                    fmtKm(runState().progress.kmTravelled) + " / " + fmtKm(b.trailTotalKm),
                    kUiWhite);
    (void)distW;

    // ---- gauges ----
    //
    // Three panels of ~100px across 312px. A gauge needs about 105px for its
    // widest label/figure pair, so four panels of 70px cannot work without
    // shortening every label; three panels leaves the arithmetic honest.
    const int gx = 4, gy = 68, gh = 60;
    const int pgap = 4;

    const double wNeed = b.waterPerCrewKg * runState().livingCrew();
    const double oNeed = b.o2PerCrewKg * runState().livingCrew();

    const int pw = 100;

    // Panel 1: water and oxygen -- the two that kill.
    r.panel(gx, gy, pw, gh, kUiBlack, kMetalDeep);
    drawGauge(r, gx + 3, gy + 5, pw - 6, "WATER", fmtWater(runState().stock.waterL),
              std::min(1.0, runState().stock.waterL / (wNeed * 20.0)),
              wNeed > 0 ? runState().stock.waterL / wNeed : -1.0, kCyan,
              runState().stock.waterL < wNeed * 3.0);
    drawGauge(r, gx + 3, gy + 28, pw - 6, "OXYGEN", fmtMass(runState().stock.o2Kg),
              std::min(1.0, runState().stock.o2Kg / (oNeed * 20.0)),
              oNeed > 0 ? runState().stock.o2Kg / oNeed : -1.0, kCyanHi,
              runState().stock.o2Kg < oNeed * 3.0);

    // Panel 2: charge and propellant.
    const int px2 = gx + pw + pgap;
    r.panel(px2, gy, pw, gh, kUiBlack, kMetalDeep);
    const double dailyKwh = b.energyPerCrewKwh * runState().livingCrew() +
                            (runState().pace == Pace::FullBurn ? b.driveEnergyBurnKwh :
                             runState().pace == Pace::Sprint ? b.driveEnergySprintKwh
                                                       : b.driveEnergyCruiseKwh);
    drawGauge(r, px2 + 3, gy + 5, pw - 6, "CHARGE", fmtKwh(runState().energyKwh),
              runState().energyKwh / b.batteryCapacityKwh, runState().batterySolsLeft(b),
              kAmberHi, runState().energyKwh < dailyKwh);
    const double range = runState().burnRangeKm(b);
    drawGauge(r, px2 + 3, gy + 28, pw - 6, "PROPELLANT",
              fmtMass(runState().stock.propellantKg()),
              std::min(1.0, runState().stock.propellantKg() / 700.0),
              range * 0.5, kAmber, runState().stock.propellantKg() < 60.0);

    // Panel 3: party health, crew, and payload -- all short labels, so this one
    // genuinely does fit a narrow column.
    const int px3 = px2 + pw + pgap;
    const int p3w = kScreenW - px3 - 4;
    const HealthBand hb = runState().healthBand();
    uint8_t hbCol = kGreenHi;
    if (hb == HealthBand::Fair) hbCol = kGreen;
    if (hb == HealthBand::Poor) hbCol = kAmberHi;
    if (hb == HealthBand::VeryPoor || hb == HealthBand::Dying) hbCol = kRedHi;
    r.panel(px3, gy, p3w, gh, kUiBlack, kMetalDeep);

    r.drawText(px3 + 3, gy + 4, "PARTY", kUiGrey);
    r.drawTextRight(kScreenW - 7, gy + 4, healthBandName(hb), hbCol);
    drawBar(r, px3 + 3, gy + 12, p3w - 6, 5, runState().health / b.healthMax, hbCol,
            kUiBlack, kUiGrey, 4);

    // Crew pips: green alive, amber maladied, dark red lost. Labelled once.
    for (size_t i = 0; i < runState().crew.size() && i < 5; ++i) {
        const CrewMember& c = runState().crew[i];
        const uint8_t col = c.dead ? static_cast<uint8_t>(kRedDeep)
                                   : (c.maladied() ? static_cast<uint8_t>(kAmberHi)
                                                   : static_cast<uint8_t>(kGreenHi));
        r.fillRect(px3 + 3 + static_cast<int>(i) * 8, gy + 20, 6, 8, col);
    }
    r.drawTextRight(kScreenW - 7, gy + 21,
                    fmtInt(runState().livingCrew()) + "/5", kUiWhite);

    r.drawText(px3 + 3, gy + 32, "PAYLOAD", kUiGrey);
    r.drawTextRight(kScreenW - 7, gy + 32, fmtMass(runState().totalPayloadKg(b)),
                    runState().overPayloadCap(b) ? kRedHi : kUiWhite);
    drawBar(r, px3 + 3, gy + 40, p3w - 6, 5,
            runState().totalPayloadKg(b) / b.payloadCapKg,
            runState().overPayloadCap(b) ? static_cast<uint8_t>(kRed)
                                         : static_cast<uint8_t>(kMetalMid),
            kUiBlack, kUiGrey, 4);

    // Two short items, one per row, so neither has to share a line.
    r.drawText(px3 + 3, gy + 49,
               fmtFixed(projectedKmToday(runState(), b), 1) + " km today", kCyanHi);
    r.drawTextRight(kScreenW - 7, gy + 49,
                    fmtKm(kmToNextLandmark(runState().progress.kmTravelled)), kUiGrey);


    // ---- route map ----
    drawTrailMap(r, 4, 130, 312, 42, runState(), wreckMarkers_, kCyanHi);

    // ---- log ----
    if (hasLastDay_ && !lastDay_.log.empty()) {
        r.panel(4, 178, 312, 20, kUiBlack, kMetalDeep);
        const std::string line = lastDay_.log.back();
        r.drawText(7, 183, line, kAmberHi);
    } else {
        r.drawText(4, 180, "RETURN to size up the situation.", kRegShadow);
    }

    if (psr) {
        r.fillRect(0, 104, kScreenW, 26, kVoidBlack);
        r.drawText(4, 108, "NO SOLAR. YOU ARE ON THE BATTERY.", kVoidRim);
        r.drawText(4, 117, fmtSols(runState().batterySolsLeft(b)) + " of light left in the cells.", kRegShadow);
    }
}

// ---------------------------------------------------------------- pause menu

void Game::updateTrailMenu() {
    const Landmark next = landmarkNow();

    // Keyboard navigation over the ten options.
    if (in().pressed(Key::Up) || in().pressed(Key::W)) {
        menuSelected_ = (menuSelected_ + Game::kMenuEntries - 1) % Game::kMenuEntries;
        audio_.play(Sfx::Beep);
    }
    if (in().pressed(Key::Down) || in().pressed(Key::S)) {
        menuSelected_ = (menuSelected_ + 1) % Game::kMenuEntries;
        audio_.play(Sfx::Beep);
    }
    for (int i = 0; i < Game::kMenuEntries; ++i) {
        if (in().pressed(static_cast<Key>(static_cast<int>(Key::Digit1) + i))) {
            menuSelected_ = i;
        }
    }

    // A landmark arrival or a pending chasm redirects the first entry.
    if (next.kind == LandmarkKind::Chasm && !runState().progress.awaitingChasm) {
        menuSelected_ = 0;
    }

    const int click = drawTrailMenuRows();
    if (click >= 0) menuSelected_ = click;

    if (!in().pressed(Key::Enter) && !in().pressed(Key::Space) &&
        !in().mousePressed(MouseButton::Left)) {
        // Nothing selected; the draw pass above is the whole update.
        return;
    }

    audio_.play(Sfx::Select);
    switch (menuSelected_) {
        case 0: {
            // Context-sensitive continue: cross, or depart.
            if (next.kind == LandmarkKind::Chasm) {
                if (runState().progress.awaitingChasm) {
                    chasmSelected_ = 0;
                    setScene(Scene::Chasm);
                } else {
                    // Walk up to the crossing and start the crossing module.
                    runState().progress.awaitingChasm = true;
                    runState().progress.chasmIndex = next.index;
                    chasmSelected_ = 0;
                    setScene(Scene::Chasm);
                }
            } else if (pendingArrival_) {
                pendingArrival_ = false;
                // Store handling on arrival.
                if (next.storeIndex >= 0) {
                    shopSelected_ = 0;
                    setScene(Scene::Shop);
                } else {
                    setScene(Scene::Trail);
                    trailPaused_ = false;
                    menuSelected_ = 0;
                    trailAutoMs_ = 0;
                }
            } else {
                setScene(Scene::Trail);
                trailPaused_ = false;
                menuSelected_ = 0;
                trailAutoMs_ = 0;
            }
            break;
        }
        case 1:
            // Check supplies: read-only inventory dump into the log.
            log().push("FOOD " + fmtMass(runState().stock.foodKg) + "   WATER " +
                       fmtWater(runState().stock.waterL) + "   O2 " + fmtMass(runState().stock.o2Kg));
            log().push("FUEL " + fmtMass(runState().stock.fuelKg) + "   OXIDISER " +
                       fmtMass(runState().stock.oxidiserKg) + "   range " + fmtKm(runState().burnRangeKm(balance_)));
            log().push("SUITS " + fmtInt(runState().stock.suitSets) + "   CHARGES " +
                       fmtInt(runState().stock.cuttingCharges) + "   CARGO " +
                       fmtMass(runState().stock.cargoKg));
            log().push("SPARES  traction " + fmtInt(runState().hardware.sparesWheel) + "   bogie " +
                       fmtInt(runState().hardware.sparesBogie) + "   seal " + fmtInt(runState().hardware.sparesSeal));
            notify("Manifest written to the log.", kCyanHi);
            break;
        case 2:
            // Route map: push a few lines of route context.
            log().push("ROUTE  next: " + next.name + "  (" +
                       fmtFixed(kmToNextLandmark(runState().progress.kmTravelled), 1) + " km)");
            log().push("ZONE  " + std::string(zoneName(runState().progress.zone)) + "  -  " +
                       zoneBlurb(runState().progress.zone));
            if (runState().hasCommsOfficer()) {
                const auto fc = routeForecast(runState(), runState().seed, balance_, 6);
                for (const auto& f : fc) log().push("FORECAST  " + f);
            } else {
                log().push("FORECAST  unavailable. No working Comms Officer.");
            }
            notify("Route read out.", kCyanHi);
            break;
        case 3:
            runState().pace = (runState().pace == Pace::Cruise) ? Pace::Sprint
                     : (runState().pace == Pace::Sprint) ? Pace::FullBurn : Pace::Cruise;
            notify("Pace: " + std::string(paceName(runState().pace)), kCyanHi);
            break;
        case 4:
            runState().rations = (runState().rations == Rations::Full) ? Rations::Reduced
                          : (runState().rations == Rations::Reduced) ? Rations::Emergency : Rations::Full;
            notify("Rations: " + std::string(rationsName(runState().rations)), kCyanHi);
            break;
        case 5: {
            // Stop to rest. 1..9 sols, as in the original.
            int sols = 1;
            if (in().pressed(Key::Digit2)) sols = 2;
            if (in().pressed(Key::Digit3)) sols = 3;
            if (in().pressed(Key::Digit5)) sols = 5;
            if (in().pressed(Key::Digit9)) sols = 9;
            applyRestingRecovery(runState(), balance_, sols, nullptr);
            for (const auto& c : runState().crew) {
                if (!c.dead && c.maladied()) log().push(c.name + " is recovering.");
            }
            log().push("Stopped for " + fmtInt(sols) + " sol(s). Nothing happened, which is "
                       "the point. Health " + fmtFixed(runState().health, 0) + ".");
            notify("Rested " + fmtInt(sols) + " sol(s).", kGreenHi);
            break;
        }
        case 6: {
            BarterOffer offer;
            if (generateBarter(runState(), rng(), balance_, &offer) && offer.valid) {
                log().push(offer.text);
                notify("Barter made.", kCyanHi);
            } else {
                log().push("Sorry, but nobody here's got anything to spare.");
                audio_.play(Sfx::Deny);
            }
            runState().progress.sol += 1;   // barter costs a sol, as in the original
            break;
        }
        case 7: {
            // Talk to the crew. Each surviving crew member says something true
            // and useful, which is what the original's 60 monologues did.
            const std::vector<const char*> advice = {
                "\"Water is the budget. Everything else is negotiable.\"",
                "\"We can cut rations or we can cut days. We cannot cut both.\"",
                "\"If we enter the dark with half a battery, we have four sols. Not eight.\"",
                "\"Do not run Full Burn into a slope. We cannot tip over at speed.\"",
                "\"I would rather rest a sol than find out what a second malady does.\"",
            };
            const std::vector<int> live = [&] {
                std::vector<int> v;
                for (size_t i = 0; i < runState().crew.size(); ++i) if (!runState().crew[i].dead) v.push_back(static_cast<int>(i));
                return v;
            }();
            if (live.empty()) {
                log().push("There is nobody left to talk to.");
            } else {
                const int pick = live[rng().below(static_cast<uint32_t>(live.size()))];
                log().push(runState().crew[static_cast<size_t>(pick)].name + ": " +
                           advice[static_cast<size_t>(pick) % advice.size()]);
            }
            break;
        }
        case 8:
            shopSelected_ = 0;
            setScene(Scene::Prospect);
            break;
        case 9: {
            // Depot recharge. Free, but it costs sols, and sols are the currency
            // the south pole does not manufacture.
            const Landmark& here = landmarkNow();
            if (here.storeIndex < 0) {
                notify("There is no depot here to charge from.", kRedHi);
                audio().play(Sfx::Deny);
            } else {
                const double before = runState().energyKwh;
                if (rechargeAtDepot(runState(), here, balance(), nullptr)) {
                    const int spent = static_cast<int>(std::lround(
                        runState().energyKwh - before));
                    (void)spent;
                    log().push("Charged at " + here.name +
                               ". Batteries full. Sols are what that cost you.");
                    notify("Batteries charged.", kGreenHi);
                } else {
                    notify("Batteries are already full.", kAmberHi);
                    audio().play(Sfx::Deny);
                }
            }
            break;
        }
        default:
            break;
    }
}

// Builds the trail menu's notes and enabled flags. Shared by the update and draw
// passes: the menu used to be drawn from update, and then erased by the draw
// pass's clear, so the pause menu showed no rows at all.
int Game::drawTrailMenuRows() {
    const Balance& b = balance();
    const Landmark next = landmarkNow();

    std::vector<std::string> notes;
    std::vector<bool> enabled;
    for (int i = 0; i < kMenuEntries; ++i) {
        notes.emplace_back();
        bool ok = true;
        if (i == 8) {
            ok = zoneIcePerCrew(runState().progress.zone, b) > 0.0;
            if (!ok) notes.back() = "no ice here";
        } else if (i == 9) {
            ok = next.storeIndex >= 0 && runState().energyKwh < b.batteryCapacityKwh - 1.0;
            if (next.storeIndex < 0) notes.back() = "no depot";
            else if (!ok) notes.back() = "batteries full";
        }
        enabled.push_back(ok);
    }

    // Ten rows at 12px from y=26 reach y=146. The row height cannot go below 12:
    // drawMenu places its text at ry+4 and the glyphs are 7 tall.
    return drawMenu(ren(), 6, 26, 180,
                    std::vector<std::string>(kMenuLabels, kMenuLabels + kMenuEntries),
                    notes, menuSelected_, in(), enabled, kCyan, 12);
}

void Game::drawTrailMenu() {
    Renderer& r = ren();
    const Balance& b = balance_;
    const Landmark next = landmarkNow();

    r.fillRect(0, 0, kScreenW, kScreenH, kUiBlack);

    // Header: where you are and what is next.
    r.fillRect(0, 0, kScreenW, 22, kMetalBlack);
    r.drawText(4, 2, "SOL " + fmtInt(runState().progress.sol), kCyanHi);
    r.drawText(52, 2, zoneName(runState().progress.zone),
               runState().progress.zone == Zone::Psr ? kVoidRim : kUiGrey);
    r.drawTextRight(kScreenW - 4, 2, fmtKm(runState().progress.kmTravelled) + " / " +
                                        fmtKm(b.trailTotalKm), kUiWhite);
    r.drawText(4, 12, "NEXT", kRegShadow);
    r.drawText(30, 12, next.name,
               next.kind == LandmarkKind::Chasm ? kRedHi : kUiWhite);
    r.drawTextRight(kScreenW - 4, 12, fmtFixed(kmToNextLandmark(runState().progress.kmTravelled), 1) +
                                       " km", kUiGrey);

    // What is happening at this landmark. The blurb is a paragraph, so the panel
    // is sized for the wrapped rows rather than for a guess: at 114px of inner
    // width the longest blurb on the route wraps to eight rows, which needs a 12px
    // title strip plus 64px of text. It used to be 48px tall and the text spilled
    // far below it.
    r.titledPanel(192, 26, 122, 88, landmarkKindName(next.kind), kUiBlack, kAmber, kMetalDeep);
    r.drawTextWrapped(196, 40, 114, next.blurb, kUiGrey, 8);

    // The rows. Drawn here rather than from update: update ran first and the
    // clear above then wiped them, leaving the pause menu empty.
    drawTrailMenuRows();

    // Health strip. Three 8px rows from y=152 reach y=175; the last one used to
    // start at 194 and ran off the bottom of the screen.
    const HealthBand hb = runState().healthBand();
    uint8_t hbCol = kGreenHi;
    if (hb == HealthBand::Fair) hbCol = kGreen;
    if (hb == HealthBand::Poor) hbCol = kAmberHi;
    if (hb == HealthBand::VeryPoor || hb == HealthBand::Dying) hbCol = kRedHi;
    drawStatusLine(r, 8, 152, 304, "PARTY " + std::string(healthBandName(hb)),
                   fmtFixed(runState().health, 0) + " / " + fmtFixed(b.healthMax, 0),
                   kUiGrey, hbCol);
    drawStatusLine(r, 8, 161, 304, "CREDITS", fmtCredits(runState().credits), kUiGrey, kGreenHi);
    drawStatusLine(r, 8, 170, 304,
                   "TODAY " + fmtFixed(projectedKmToday(runState(), b), 1) + " km",
                   projectedKmReason(runState(), b), kUiGrey, kCyanHi);

    if (pendingArrival_ && next.kind != LandmarkKind::Chasm) {
        r.drawText(8, 186, "ARRIVED: " + pendingLandmarkName_ +
                           (next.storeIndex >= 0 ? "  -  STORE OPEN" : ""), kGreenHi);
    }
    // 192 + 7 = 199, the last row the screen has. At 194 it ran off the bottom.
    r.drawText(8, 192, "ENTER select    ESC resume", kRegShadow);
}

// ---------------------------------------------------------------- shop

void Game::updateShop() {
    const Landmark& here = landmarkNow();
    const int store = std::max(0, here.storeIndex);

    const Good goods[] = {
        Good::Food, Good::Water, Good::O2, Good::Fuel, Good::Oxidiser,
        Good::SuitSets, Good::CuttingCharges,
        Good::SpareWheel, Good::SpareBogie, Good::SpareSeal,
        Good::ColonyCargo,
    };
    const int count = static_cast<int>(sizeof(goods) / sizeof(goods[0]));

    if (in().pressed(Key::Up) || in().pressed(Key::W)) {
        shopSelected_ = (shopSelected_ + count - 1) % count;
        audio_.play(Sfx::Beep);
    }
    if (in().pressed(Key::Down) || in().pressed(Key::S)) {
        shopSelected_ = (shopSelected_ + 1) % count;
        audio_.play(Sfx::Beep);
    }

    const Good g = goods[shopSelected_];

    // Purchase steps scale with the unit: consumables in bulk, spares one at a time.
    double step = 1.0;
    if (g == Good::Food || g == Good::O2 || g == Good::Fuel || g == Good::Oxidiser) step = 20.0;
    else if (g == Good::Water) step = 200.0;
    else if (g == Good::ColonyCargo) step = 20.0;

    auto affordable = [&](double qty) {
        return buyCost(runState(), g, qty, store, balance_) <= runState().credits;
    };

    if (in().pressed(Key::Right) || in().pressed(Key::D)) {
        const double room = addableAmount(runState(), g, balance_);
        const double want = std::min(step, room);
        if (want <= 0.0) {
            notify(g == Good::ColonyCargo || g == Good::SuitSets ||
                   g == Good::CuttingCharges || g == Good::SpareWheel ||
                   g == Good::SpareBogie || g == Good::SpareSeal
                       ? "You are carrying the maximum." : "No room in the payload bay.",
                   kAmberHi);
            audio_.play(Sfx::Deny);
        } else if (!affordable(want)) {
            notify("Not enough credits.", kRedHi);
            audio_.play(Sfx::Deny);
        } else {
            buy(runState(), g, want, store, balance_);
            audio_.play(Sfx::Select);
        }
    }
    if (in().pressed(Key::Left) || in().pressed(Key::A)) {
        const double have = stockOf(runState(), g);
        const double give = std::min(step, have);
        if (give <= 0.0) {
            audio_.play(Sfx::Deny);
        } else {
            sell(runState(), g, give, store, balance_);
            audio_.play(Sfx::Beep);
        }
    }

    // Enter leaves the store.
    if (in().pressed(Key::Enter) || in().pressed(Key::Space)) {
        pendingArrival_ = false;
        menuSelected_ = 0;
        trailPaused_ = false;
        trailAutoMs_ = 0;
        setScene(Scene::Trail);
        audio_.play(Sfx::Select);
    }
}

void Game::drawShop() {
    Renderer& r = ren();
    const Balance& b = balance_;
    const Landmark& here = landmarkNow();
    const int store = std::max(0, here.storeIndex);

    r.fillRect(0, 0, kScreenW, kScreenH, kUiBlack);

    r.fillRect(0, 0, kScreenW, 24, kMetalBlack);
    r.drawText(4, 2, here.name, kAmberHi);
    r.drawTextRight(kScreenW - 4, 2, fmtCredits(runState().credits), kGreenHi);
    char mult[48];
    std::snprintf(mult, sizeof(mult), "PRICES x%s", fmtFixed(1.0 + b.outpostMarkupPerIndex * store, 2).c_str());
    r.drawText(4, 12, mult, store == 0 ? kUiGrey : kRedHi);
    r.drawTextRight(kScreenW - 4, 12,
                    fmtMass(runState().totalPayloadKg(b)) + " / " + fmtMass(b.payloadCapKg),
                    runState().overPayloadCap(b) ? kRedHi : kUiWhite);

    r.drawText(4, 26, "LEFT/RIGHT buy or sell    ENTER to leave", kRegShadow);
    // Kept under 52 characters so it fits the screen. The previous wording ran
    // to 335px and was cut off mid-word, losing the point of the sentence.
    r.drawText(4, 34, "Outposts buy back at depot price, not at a premium.", kRegShadow);

    const Good goods[] = {
        Good::Food, Good::Water, Good::O2, Good::Fuel, Good::Oxidiser,
        Good::SuitSets, Good::CuttingCharges,
        Good::SpareWheel, Good::SpareBogie, Good::SpareSeal,
        Good::ColonyCargo,
    };
    const int count = static_cast<int>(sizeof(goods) / sizeof(goods[0]));

    int y = 43;
    for (int i = 0; i < count; ++i) {
        const Good g = goods[i];
        const bool sel = (i == shopSelected_);
        const double price = goodPrice(g, store, b);
        const double have = stockOf(runState(), g);
        const double room = addableAmount(runState(), g, b);
        const int cap = goodCap(g, b);
        const bool atCap = cap >= 0 && have >= static_cast<double>(cap);

        if (sel) {
            r.fillRect(2, y - 2, 316, 12, kMetalBlack);
            r.rect(2, y - 2, 316, 12, kCyanHi);
        }

        r.drawText(6, y, goodName(g), sel ? kHiWhite : kUiWhite);
        char haveBuf[32];
        std::snprintf(haveBuf, sizeof(haveBuf), "%.0f %s", have, goodUnit(g));
        r.drawText(140, y, haveBuf, kUiWhite);
        r.drawTextRight(240, y, fmtCredits(price) + "/" + goodUnit(g), kAmberHi);

        if (atCap) {
            r.drawTextRight(kScreenW - 6, y, "MAX", kRedHi);
        } else if (room <= 0.0) {
            r.drawTextRight(kScreenW - 6, y, "NO ROOM", kRedHi);
        } else {
            const double canAfford = runState().credits / std::max(0.0001, price);
            char capBuf[32];
            std::snprintf(capBuf, sizeof(capBuf), "room %.0f", std::min(room, canAfford));
            r.drawTextRight(kScreenW - 6, y, capBuf, kRegShadow);
        }
        y += 12;
    }

    // 11 rows from y=43 reaches y=175, so the advice panel gets the last 21 rows.
    r.titledPanel(2, 178, 316, 20, "OUTPOST ADVICE", kUiBlack, kAmber, kMetalDeep);
    r.drawText(6, 188, "Prices rise 25% every outpost. Restock early, not late.", kAmberHi);
}

// ---------------------------------------------------------------- chasm

void Game::updateChasm() {
    const Landmark& here = landmarkNow();
    const auto opts = chasmOptions(runState(), here, balance_);
    const int count = static_cast<int>(opts.size());
    if (count == 0) return;

    if (in().pressed(Key::Up) || in().pressed(Key::W)) {
        chasmSelected_ = (chasmSelected_ + count - 1) % count;
        audio_.play(Sfx::Beep);
    }
    if (in().pressed(Key::Down) || in().pressed(Key::S)) {
        chasmSelected_ = (chasmSelected_ + 1) % count;
        audio_.play(Sfx::Beep);
    }

    const int click = drawMenu(ren(), 6, 44, 308,
                               [&] {
                                   std::vector<std::string> items, notes;
                                   for (const auto& o : opts) {
                                       items.push_back(chasmMethodName(o.method));
                                       std::string n;
                                       if (!o.available)            n = "UNAVAILABLE";
                                       else if (o.costCredits > 0)  n = fmtCredits(o.costCredits);
                                       else if (o.costSuitSets)    n = fmtInt(o.costSuitSets) + " suit sets";
                                       else if (o.costPropellantKg> 0) n = fmtMass(o.costPropellantKg);
                                       else                        n = "free";
                                       notes.push_back(n);
                                   }
                                   return items;
                               }(),
                               [&] {
                                   std::vector<std::string> notes;
                                   for (const auto& o : opts) {
                                       if (!o.available) notes.push_back("");
                                       else notes.push_back(fmtPct(o.risk) + " risk" +
                                           (o.solsCost ? "  " + fmtInt(o.solsCost) + " sol" : ""));
                                   }
                                   return notes;
                               }(),
                               chasmSelected_, in(), [&] {
                                   std::vector<bool> e;
                                   for (const auto& o : opts) e.push_back(o.available);
                                   return e;
                               }(), kCyan);
    if (click >= 0) chasmSelected_ = click;

    if (!in().pressed(Key::Enter) && !in().pressed(Key::Space) &&
        !in().mousePressed(MouseButton::Left)) return;

    const ChasmPlan& p = opts[static_cast<size_t>(chasmSelected_)];
    if (!p.available) {
        notify(p.unavailableReason, kRedHi);
        audio_.play(Sfx::Deny);
        return;
    }
    if (p.costCredits > runState().credits) {
        notify("Not enough credits for that crossing.", kRedHi);
        audio_.play(Sfx::Deny);
        return;
    }

    // A guided crossing may have to wait for a support window: up to six sols,
    // which is the original's ferry wait, and it is not free.
    if (p.method == ChasmMethod::Guided) {
        const int wait = rng().rangeInt(0, 6);
        if (wait > 0) {
            runState().progress.sol += wait;
            runState().credits -= p.costCredits;
            log().push("Waiting " + fmtInt(wait) + " sol(s) for a support window at " +
                       here.name + ". You pay whether or not they come today.");
            applyRestingRecovery(runState(), balance_, 0, nullptr);
        }
    }

    const ChasmResult res = resolveChasm(runState(), here, p.method, rng(), balance_);
    audio_.play(res.succeeded ? Sfx::Select : Sfx::Impact);

    if (res.succeeded) {
        runState().progress.kmTravelled += res.kmGained;
        runState().progress.sol += res.solsLost;
        runState().progress.awaitingChasm = false;
        log().push(res.headline + ": " + (res.lines.empty() ? "" : res.lines.front()));
    } else {
        runState().progress.sol += std::max(1, res.solsLost);
        for (const auto& l : res.lines) log().push(l);
        if (!res.succeeded) log().push("You did not cross cleanly. What you lost, you lost.");
    }
    log().push("Crossing resolved. " + std::string(res.succeeded ? "You are across." :
                                                   "Check the manifest."));

    checkLossConditions(runState(), balance_);

    pendingArrival_ = true;
    pendingLandmark_ = here.index;
    pendingLandmarkName_ = here.name;
    runState().progress.landmarkIndex = here.index + 1;
    menuSelected_ = 0;
    setScene(Scene::TrailMenu);
    trailPaused_ = true;
}

void Game::drawChasm() {
    Renderer& r = ren();
    const Landmark& here = landmarkNow();

    r.fillRect(0, 0, kScreenW, kScreenH, kUiBlack);

    // A chasma: a bright sunlit rim above, black below.
    for (int y = 0; y < 40; ++y) {
        r.hline(0, y, kScreenW, (y / 8) % 2 == 0 ? kVoidRim : kVoidDeep);
    }
    r.hline(0, 40, kScreenW, kRegBright);
    for (int y = 41; y < 44; ++y) r.hline(0, y, kScreenW, kRegMid);
    r.fillRect(0, 44, kScreenW, kScreenH - 44, kVoidBlack);

    // Crater walls converging.
    for (int i = 0; i < 40; ++i) {
        const int d = i;
        const int inset = std::min(60, d * 2);
        r.pixel(inset, 44 + i, kVoidDeep);
        r.pixel(kScreenW - 1 - inset, 44 + i, kVoidDeep);
        if (i % 7 == 0) {
            r.pixel(inset - 1, 44 + i, kVoidDark);
            r.pixel(kScreenW - inset, 44 + i, kVoidDark);
        }
    }

    r.fillRect(0, 0, kScreenW, 22, kUiBlack);
    r.drawText(4, 2, here.name, kRedHi);
    char info[80];
    std::snprintf(info, sizeof(info), "%d m deep   %d m span", static_cast<int>(here.chasmaDepthM),
                  static_cast<int>(here.chasmaSpanM));
    r.drawTextRight(kScreenW - 4, 2, info, kUiGrey);
    r.drawText(4, 12, "CHOOSE A CROSSING", kAmberHi);
    r.drawTextRight(kScreenW - 4, 12, fmtCredits(runState().credits), kGreenHi);

    // Bound the vector to a named local. Indexing a temporary and binding a
    // reference to the result leaves a dangling pointer once the statement ends.
    const std::vector<ChasmPlan> options = chasmOptions(runState(), here, balance_);
    if (options.empty()) {
        r.drawText(4, 60, "No crossing method is available here.", kRedHi);
        return;
    }

    // The list of crossings. The screen used to show only the selected option's
    // description, under a heading that said CHOOSE A CROSSING, so the player was
    // asked to choose without being able to see the choices.
    int row = 48;
    for (size_t i = 0; i < options.size(); ++i) {
        const ChasmPlan& o = options[i];
        const bool sel = static_cast<int>(i) == chasmSelected_;
        if (sel) {
            r.fillRect(6, row - 2, 308, 24, kMetalBlack);
            r.rect(6, row - 2, 308, 24, kCyanHi);
        }
        r.drawText(10, row, chasmMethodName(o.method),
                   !o.available ? kUiGrey : (sel ? kHiWhite : kUiWhite));

        // Cost and risk share a row, each right-aligned to its own column, so a
        // wide method name cannot push them off the panel.
        if (o.available) {
            char cost[64];
            std::snprintf(cost, sizeof(cost), "%d cr   %d sols",
                          static_cast<int>(o.costCredits), o.solsCost);
            r.drawTextRight(214, row, cost, kUiWhite);
            const std::string risk = "risk " + fmtPct(o.risk);
            r.drawTextRight(300, row, risk,
                            o.risk > 0.4   ? kRedHi
                            : o.risk > 0.15 ? kAmberHi
                                            : kGreenHi);
        } else {
            // An option you cannot take still has to be readable, and the reason
            // is the useful part: it tells you what to go and buy.
            r.drawTextRight(214, row, "unavailable", kRegShadow);
            r.drawTextWrapped(10, row + 10, 300, o.unavailableReason, kRegShadow, 8);
        }
        row += 26;
    }

    const ChasmPlan& p = options[static_cast<size_t>(chasmSelected_)];

    r.titledPanel(6, 152, 308, 36, chasmMethodName(p.method), kUiBlack,
                  p.available ? static_cast<uint8_t>(kCyan) : static_cast<uint8_t>(kRed),
                  kMetalDeep);
    if (p.available) {
        r.drawTextWrapped(10, 163, 300, p.description, kUiWhite, 8);
    } else {
        r.drawTextWrapped(10, 163, 300, p.unavailableReason, kRedHi, 8);
    }
}

// ---------------------------------------------------------------- prospect

void Game::updateProspect() {
    // Select a sortie length. One sol is the standard; three is a long push.
    static int chosen = 1;
    if (in().pressed(Key::Up) || in().pressed(Key::W))   { chosen = chosen == 1 ? 3 : 1; audio_.play(Sfx::Beep); }
    if (in().pressed(Key::Down) || in().pressed(Key::S)) { chosen = chosen == 3 ? 1 : 3; audio_.play(Sfx::Beep); }

    if (!in().pressed(Key::Enter) && !in().pressed(Key::Space) &&
        !in().mousePressed(MouseButton::Left)) return;

    const Balance& b = balance_;
    const Zone z = runState().progress.zone;
    const double perCrew = zoneIcePerCrew(z, b);
    const int crew = runState().livingCrew();

    if (crew == 0) {
        setScene(Scene::TrailMenu);
        return;
    }

    // The haul cap. This is the 100 lb rule preserved exactly, and it is the
    // reason a sortie is a decision rather than a button.
    const double raw = perCrew * static_cast<double>(crew) * runState().geologistBonus();
    const double capped = std::min(raw, b.sortieHaulCapKg);
    const double dropped = raw - capped;

    // Every sortie spends the day's consumables. Prospecting is not free.
    runState().progress.sol += chosen;
    runState().progress.sortiesRun += 1;
    runState().stock.foodKg  = std::max(0.0, runState().stock.foodKg  - b.foodPerCrewKg  * crew * chosen);
    runState().stock.waterL  = std::max(0.0, runState().stock.waterL  - b.waterPerCrewKg * crew * chosen);
    runState().stock.o2Kg    = std::max(0.0, runState().stock.o2Kg    - b.o2PerCrewKg   * crew * chosen);
    runState().stock.cuttingCharges = std::max(0, runState().stock.cuttingCharges - chosen);

    // Split the haul. Water to drink; the rest to electrolysis for oxygen and
    // hydrogen, and hydrogen into fuel.
    const double drink = capped * kSortieDrinkFraction;
    const double toElec = capped - drink;
    const double o2 = toElec * 0.111;
    const double h2 = toElec * 0.889;
    const double fuel = h2 / 0.333 * 0.42;   // partial Sabatier, CO2 from regolith

    runState().stock.waterL += drink;
    runState().stock.o2Kg += o2;
    runState().stock.fuelKg += fuel;
    runState().progress.iceMinedKg += capped;

    char detail[256];
    std::snprintf(detail, sizeof(detail),
                  "%d crew at %s mined %.0f kg of ice. You carried %.0f kg back "
                  "(the rest is still there, under the regolith).",
                  crew, zoneName(z), raw, capped);
    log().push(detail);

    char made[200];
    std::snprintf(made, sizeof(made),
                  "Electrolyzed %.0f kg: %.0f L of water, %.0f kg O2, and enough "
                  "hydrogen for %.0f kg of LCH4.", toElec, drink, o2, fuel);
    log().push(made);

    if (dropped > 1.0) {
        log().push("You left " + fmtFixed(dropped, 0) +
                   " kg in the face because you could not carry it. Come back for it "
                   "if you have the payload room and the days.");
    }
    if (z == Zone::Sunlit) {
        log().push("Nothing here. The sunlit rim is dry. Ice is in the shadow, and "
                   "the shadow is where the power is not.");
        audio_.play(Sfx::Deny);
    } else {
        audio_.play(Sfx::Select);
    }

    // Enforce the payload cap. The haul that will not fit stays in the ground:
    // the drill is already running and there is no crew left to carry it. Cargo
    // already aboard goes over the side only as a last resort, and the log says
    // so plainly -- silently eating the colony seed would be indefensible.
    if (runState().overPayloadCap(b)) {
        const double jettisoned = jettisonToFit(runState(), b);
        if (jettisoned > 0.5) {
            log().push("Over payload. " + fmtMass(jettisoned) +
                       " went over the side to make room for the haul. The colony "
                       "will have to do with less.");
        } else {
            log().push("Over payload, and there was nothing disposable aboard. "
                       "The last of the haul stays in the ground.");
            runState().stock.cargoKg =
                std::max(0.0, runState().stock.cargoKg -
                              (runState().totalPayloadKg(b) - b.payloadCapKg));
        }
    }

    checkLossConditions(runState(), balance_);
    menuSelected_ = 0;
    setScene(Scene::TrailMenu);
    trailPaused_ = true;
}

void Game::drawProspect() {
    Renderer& r = ren();
    const Balance& b = balance_;
    const Zone z = runState().progress.zone;
    const int crew = runState().livingCrew();

    r.fillRect(0, 0, kScreenW, kScreenH, kUiBlack);

    // Look at the ground you are standing on.
    const bool psr = z == Zone::Psr;
    for (int y = 0; y < 78; ++y) {
        const int band = (y / 8) % 2;
        r.hline(0, y, kScreenW, band == 0 ? (psr ? kVoidNear : kVoidRim)
                                         : (psr ? kVoidBlack : kVoidDeep));
    }
    for (int y = 78; y < kScreenH; ++y) {
        const int d = y - 78;
        const uint8_t col = psr ? (d < 2 ? kVoidRim : (d < 8 ? kVoidDeep : kVoidDark))
                                : (d < 2 ? kRegBright : (d < 10 ? kRegMid : kRegDeep));
        r.hline(0, y, kScreenW, col);
    }

    // The drill and, if the ground has any, a bright ice seam.
    const int dx = 60;
    r.fillRect(dx, 66, 18, 12, kMetalMid);
    r.fillRect(dx + 18, 70, 12, 3, kMetalDark);
    r.vline(dx + 30, 73, 8, kMetalHi);
    if (!psr) {
        for (int i = 0; i < 40; ++i) {
            const int ix = 180 + (i * 13) % 120;
            const int iy = 96 + (i * 7) % 80;
            r.pixel(ix, iy, kCyanHi);
            if (i % 3 == 0) r.pixel(ix + 1, iy, kUiWhite);
        }
        r.drawText(176, 88, "NO ICE HERE", kRedHi);
    } else {
        for (int i = 0; i < 200; ++i) {
            const int ix = 170 + (i * 13) % 140;
            const int iy = 90 + (i * 7) % 90;
            r.pixel(ix, iy, (i % 4 == 0) ? kHiWhite : kCyanHi);
        }
        r.drawText(176, 82, "ICE SEAM", kCyanHi);
    }

    // Header carries the zone name only. The blurb is a sentence, and drawing it
    // here ran it back over the title: "ICE PROSPECTING" and "Near-terminator
    // ridge. Full sun. No ice." were printed on top of each other.
    r.fillRect(0, 0, kScreenW, 20, kUiBlack);
    r.drawText(4, 2, "ICE PROSPECTING", kAmberHi);
    r.drawTextRight(kScreenW - 4, 2, zoneName(z), psr ? kVoidRim : kUiGrey);

    // 24 + 86 = 110: five 9px rows from y=36 reach y=79, and the three note rows
    // at 86/94/102 reach y=109. The panels used to be 76 tall, so the notes were
    // drawn outside their own border and over the rule panel below.
    r.titledPanel(4, 24, 154, 86, "SORTIE PLANNING", kUiBlack, kCyan, kMetalDeep);
    drawStatusLine(r, 8, 36, 146, "Crew available", fmtInt(crew) + " / 5", kUiGrey, kUiWhite);
    drawStatusLine(r, 8, 45, 146, "Zone yield",
                     fmtFixed(zoneIcePerCrew(z, b), 0) + " kg/crew", kUiGrey,
                     zoneIcePerCrew(z, b) > 0 ? kCyanHi : kRedHi);
    drawStatusLine(r, 8, 54, 146, "Raw yield",
                     fmtFixed(zoneIcePerCrew(z, b) * crew * runState().geologistBonus(), 0) + " kg",
                     kUiGrey, kUiWhite);
    drawStatusLine(r, 8, 63, 146, "Haul cap",
                     fmtFixed(b.sortieHaulCapKg, 0) + " kg", kUiGrey, kAmberHi);
    drawStatusLine(r, 8, 72, 146, "Charges needed", "1 / sortie", kUiGrey, kUiWhite);
    r.drawTextWrapped(8, 86, 146, zoneBlurb(z), kRegShadow, 8);
    // Kept to one 146px row. The longer sentence was cut off at the panel edge,
    // losing the word that said what the sol buys.
    r.drawText(8, 102, "Costs 1 sol and rations.", kRegShadow);

    r.titledPanel(162, 24, 154, 86, "WHAT YOU GET BACK", kUiBlack, kGreen, kMetalDeep);
    const double raw = zoneIcePerCrew(z, b) * crew * runState().geologistBonus();
    const double capped = std::min(raw, b.sortieHaulCapKg);
    const double drink = capped * kSortieDrinkFraction;
    const double toElec = capped - drink;
    drawStatusLine(r, 166, 36, 146, "Carried", fmtFixed(capped, 0) + " kg", kUiGrey, kHiWhite);
    drawStatusLine(r, 166, 45, 146, "Left in the face", fmtFixed(raw - capped, 0) + " kg",
                     kUiGrey, raw > capped ? kRedHi : kRegShadow);
    drawStatusLine(r, 166, 54, 146, "Water", fmtFixed(drink, 0) + " L", kUiGrey, kCyan);
    drawStatusLine(r, 166, 63, 146, "Oxygen", fmtFixed(toElec * 0.111, 1) + " kg", kUiGrey, kCyanHi);
    drawStatusLine(r, 166, 72, 146, "LCH4 fuel",
                     fmtFixed(toElec * 0.889 / 0.333 * 0.42, 1) + " kg", kUiGrey, kAmberHi);
    // Two rows at 8px from y=86 reach y=101. Longer wording needed a fourth row
    // and spilled past the panel border.
    r.drawTextWrapped(166, 86, 146, "Mine water and fuel. Buy LOX.", kAmberHi, 8);

    // Each line here is under 52 characters, which is 311px: the earlier wording
    // ran to 455px and was cut off at the screen edge.
    r.titledPanel(4, 114, 312, 40, "THE RULE", kUiBlack, kAmber, kMetalDeep);
    r.drawText(8, 128, "You can mine far more than you can carry.", kAmberHi);
    r.drawText(8, 136, "The cap is the whole point. The original imposed", kRegShadow);
    r.drawText(8, 144, "the same limit on hunting, for the same reason.", kRegShadow);

    if ((static_cast<int>(frame_) / 26) % 2 == 0) {
        r.drawTextCentered(kScreenW / 2, 162, "ENTER to run the sortie", kCyanHi);
    }
    r.drawTextCentered(kScreenW / 2, 176, "ESC to cancel", kRegShadow);
}

// ---------------------------------------------------------------- helpers

Landmark Game::landmarkNow() const {
    const auto& t = trail();
    const size_t idx = std::min<size_t>(static_cast<size_t>(runState().progress.landmarkIndex),
                                        t.size() - 1);
    return t[idx];
}

}  // namespace lt
