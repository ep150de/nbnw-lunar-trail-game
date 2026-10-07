// Chapter One: manifest, LEO depot, TLI, cruise, LLOI, and the descent scene.

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "game/descent.h"
#include "game/game.h"

namespace lt {

// ---------------------------------------------------------------- manifest

void Game::updateManifest() {
    // Profession is chosen with Left/Right; crew with Up/Down + Enter.
    if (in().pressed(Key::Left) || in().pressed(Key::A) || in().pressed(Key::Right) ||
        in().pressed(Key::D)) {
        const int delta = (in().pressed(Key::Left) || in().pressed(Key::A)) ? -1 : 1;
        const int p = (static_cast<int>(runState().profession) + delta + 3) % 3;
        runState().profession = static_cast<Profession>(p);
        runState().credits = professionCredits(runState().profession, balance_);
        audio_.play(Sfx::Beep);
    }

    if (in().pressed(Key::Down) || in().pressed(Key::S)) {
        manifestCrewSlot_ = (manifestCrewSlot_ + 1) % 5;
        audio_.play(Sfx::Beep);
    }
    if (in().pressed(Key::Up) || in().pressed(Key::W)) {
        manifestCrewSlot_ = (manifestCrewSlot_ + 4) % 5;
        audio_.play(Sfx::Beep);
    }

    // Swap the selected slot's name with the next roster entry. This is the
    // roster selector; swapping permutes within the roster so no name repeats.
    if (in().pressed(Key::Enter)) {
        const auto& roster = crewRoster();
        const size_t n = roster.size();
        for (size_t i = 0; i < n; ++i) {
            if (roster[i].name == runState().crew[static_cast<size_t>(manifestCrewSlot_)].name) {
                const size_t next = (i + 1) % n;
                runState().crew[static_cast<size_t>(manifestCrewSlot_)].perk = roster[next].perk;
                runState().crew[static_cast<size_t>(manifestCrewSlot_)].name  = roster[next].name;
                audio_.play(Sfx::Select);
                break;
            }
        }
    }

    if (in().pressed(Key::Space)) {
        ascent_ = AscentPlan{};
        ch1Committed_ = true;
        setScene(Scene::Depot);
        audio_.play(Sfx::Select);
    }
}

void Game::drawManifest() {
    Renderer& r = ren();
    const Balance& b = balance_;
    r.fillRect(0, 0, kScreenW, kScreenH, kUiBlack);
    drawBackdrop(r, static_cast<int>(frame_), kVoidDark, kVoidNear, kMetalDeep);

    r.drawTextShadow(6, 4, "MISSION MANIFEST", kAmberHi, kAmberDeep);
    r.drawTextRight(kScreenW - 6, 4, "SEED " + runState().seedText, kCyan);

    // ---- profession ----
    // Three rows inside a 44px panel: title strip 11px, then 24px of content.
    // The blurb is 40 characters (240px) and does not fit in 146px, so it is
    // wrapped rather than centred off the edge.
    // The blurb is 40 characters and wraps to three lines at 22px per line, so
    // the panel is 62px tall to hold title strip + 2 label rows + 3 blurb rows.
    r.titledPanel(6, 14, 150, 60, "ASSIGNMENT", kUiBlack, kCyan, kMetalDeep);
    r.drawTextCentered(81, 27, professionName(runState().profession), kUiWhite);
    r.drawTextCentered(81, 36, fmtCredits(runState().credits) + "   score x" +
                                    fmtFixed(professionScoreMult(runState().profession, b), 0),
                       kCyanHi);
    r.drawTextWrapped(10, 47, 142, professionBlurb(runState().profession), kRegShadow, 8);

    // ---- crew ----
    // Rows at 13px pitch from y=27 reach y=92; the panel ends at 106, leaving
    // room for the hint line.
    // Rows at 13px from y=27 reach y=92; the panel ends at 120, so the hint fits.
    r.titledPanel(162, 14, 152, 108, "CREW OF FIVE", kUiBlack, kCyan, kMetalDeep);

    // The perk column is placed from the longest name actually in the roster,
    // not from a hardcoded x. It used to sit at 226 on the assumption that the
    // longest name was nine characters; Lindqvist is ten, so its perk ran into
    // the name with no gap between them.
    int longestName = 0;
    for (const CrewMember& c : runState().crew) {
        longestName = std::max(longestName, Renderer::textWidth(c.name));
    }
    // Clamped so the longest label still clears the panel's right edge, for
    // rosters whose names are all short the column simply sits further right.
    int widestPerk = 0;
    for (const CrewMember& c : runState().crew) {
        widestPerk = std::max(widestPerk, Renderer::textWidth(perkName(c.perk)));
    }
    const int perkX = std::max(167 + longestName + 6, 165 + 146 - widestPerk - 3);

    int y = 27;
    for (size_t i = 0; i < runState().crew.size() && i < 5; ++i) {
        const bool sel = static_cast<int>(i) == manifestCrewSlot_;
        if (sel) {
            r.fillRect(165, y - 2, 146, 12, kMetalBlack);
            r.rect(165, y - 2, 146, 12, kCyanHi);
        }
        r.drawText(167, y, runState().crew[i].name, sel ? kHiWhite : kUiWhite);
        r.drawText(perkX, y, perkName(runState().crew[i].perk), sel ? kCyanHi : kCyan);
        y += 13;
    }
    r.drawText(165, 96, "ENTER changes the highlighted member.", kRegShadow);
    r.drawText(165, 106, "Specialities change rules, not stats.", kRegShadow);

    // ---- vehicle ----
    // Rows at 9px pitch from y=71 reach y=98; the panel ends at 108.
    // Four rows at 9px from y=93 reach y=120; the panel ends at 124.
    r.titledPanel(6, 78, 150, 44, "SURFACE CONVOY", kUiBlack, kCyan, kMetalDeep);
    drawStatusLine(r, 10, 89, 142, "Dry mass", fmtMass(b.dryMassKg), kUiGrey, kUiWhite);
    drawStatusLine(r, 10, 97, 142, "Payload cap", fmtMass(b.payloadCapKg), kUiGrey, kAmberHi);
    drawStatusLine(r, 10, 105, 142, "Traction", fmtInt(b.tractionUnits), kUiGrey, kUiWhite);
    drawStatusLine(r, 10, 113, 142, "Batteries", fmtKwh(b.batteryCapacityKwh), kUiGrey,
                   kUiWhite);

    // ---- what the manifest commits you to ----
    // Four columns of 76px each. A label plus its widest value reaches 84px in a
    // 76px column, so each cell carries the label on one line and the value
    // beneath it rather than trying to fit both across.
    r.titledPanel(6, 126, 308, 50, "CONSUMPTION PER SOL  (5 CREW)", kUiBlack, kAmber,
                  kMetalDeep);
    const double crew = 5.0;
    struct Cell { const char* label; double value; uint8_t colour; };
    const Cell cells[4] = {
        {"FOOD",   b.foodPerCrewKg  * crew, kUiWhite},
        {"WATER",  b.waterPerCrewKg * crew, kCyanHi},
        {"OXYGEN", b.o2PerCrewKg   * crew, kCyanHi},
        {"ENERGY", b.energyPerCrewKwh * crew, kAmberHi},
    };
    for (int i = 0; i < 4; ++i) {
        const int cx = 10 + i * 76;
        r.drawText(cx, 138, cells[i].label, kUiGrey);
        const std::string v = (i == 1) ? fmtWater(cells[i].value)
                            : (i == 3) ? fmtKwh(cells[i].value)
                                       : fmtMass(cells[i].value);
        r.drawText(cx, 146, v, cells[i].colour);
        r.drawText(cx + Renderer::textWidth(v) + 2, 146, "/sol", kRegShadow);
        if (i > 0) r.vline(cx - 3, 136, 18, kMetalDeep);
    }

    const double dailyKg = (b.foodPerCrewKg + b.waterPerCrewKg + b.o2PerCrewKg) * crew;
    // Two short lines rather than one long one: 62 characters is 372px and the
    // panel interior is 300px.
    r.drawText(10, 156, "A " + fmtInt(b.typicalSols) + " sol run needs " +
                            fmtMass(dailyKg * b.typicalSols) + ".", kRegShadow);
    r.drawText(10, 164, "The bay is " + fmtMass(b.payloadCapKg) +
                            ". You will have to mine.", kAmberHi);

    // ---- prompts ----
    r.fillRect(0, 178, kScreenW, 22, kUiBlack);
    if ((static_cast<int>(frame_) / 26) % 2 == 0) {
        r.drawTextCentered(kScreenW / 2, 181, "SPACE to commit to the ascent stack", kCyanHi);
    }
    r.drawTextCentered(kScreenW / 2, 191,
                       "L/R profession   U/D slot   ENTER crew", kRegShadow);
}

// ---------------------------------------------------------------- depot

void Game::updateDepot() {
    // Three refuel windows. Each has an escalating price and a hard propellant
    // capacity; the last window is the expensive one and it closes.
    const int window = depotRow_ / 4;   // 0..2 windows, 4 rows each
    (void)window;

    const double scale = 1.0 + balance_.tliWindowEscalation * static_cast<double>(depotRow_ / 4);
    const double fuelPrice = balance_.depotPricePerKgFuel * scale;
    const double o2Price = balance_.depotPricePerKgOxidiser * scale;

    const double maxFuel = balance_.leoWetMassKg * 0.42;
    const double maxO2 = balance_.leoWetMassKg * 0.42;

    const double stepFuel = 200.0;
    const double stepO2 = 1200.0;

    if (in().pressed(Key::Up) || in().pressed(Key::W)) {
        depotRow_ = (depotRow_ + 3) % 4;
        audio_.play(Sfx::Beep);
    }
    if (in().pressed(Key::Down) || in().pressed(Key::S)) {
        depotRow_ = (depotRow_ + 1) % 4;
        audio_.play(Sfx::Beep);
    }

    auto buyFuel = [&](double delta) {
        const double want = std::clamp(ascent_.tliFuelKg + delta, 0.0, maxFuel);
        const double cost = (want - ascent_.tliFuelKg) * fuelPrice;
        if (cost <= runState().credits && want >= ascent_.tliFuelKg) {
            runState().credits -= cost;
            ascent_.tliFuelKg = want;
            audio_.play(Sfx::Select);
        } else {
            audio_.play(Sfx::Deny);
        }
    };
    auto buyO2 = [&](double delta) {
        const double want = std::clamp(ascent_.tliOxidiserKg + delta, 0.0, maxO2);
        const double cost = (want - ascent_.tliOxidiserKg) * o2Price;
        if (cost <= runState().credits && want >= ascent_.tliOxidiserKg) {
            runState().credits -= cost;
            ascent_.tliOxidiserKg = want;
            audio_.play(Sfx::Select);
        } else {
            audio_.play(Sfx::Deny);
        }
    };

    switch (depotRow_) {
        case 0: if (in().pressed(Key::Left)  || in().pressed(Key::A)) buyFuel(-stepFuel);
                if (in().pressed(Key::Right) || in().pressed(Key::D)) buyFuel( stepFuel);
                break;
        case 1: if (in().pressed(Key::Left)  || in().pressed(Key::A)) buyO2(-stepO2);
                if (in().pressed(Key::Right) || in().pressed(Key::D)) buyO2( stepO2);
                break;
        case 2: if (in().pressed(Key::Left)  || in().pressed(Key::A)) {
                    if (ascent_.loxReserveKg > 0.0) {
                        ascent_.loxReserveKg -= 100.0;
                        ascent_.tliOxidiserKg += 100.0;
                        audio_.play(Sfx::Select);
                    } else audio_.play(Sfx::Deny);
                }
                if (in().pressed(Key::Right) || in().pressed(Key::D)) {
                    const double want = std::min(ascent_.loxReserveKg + 100.0,
                                                 maxO2 - ascent_.tliOxidiserKg);
                    if (want > ascent_.loxReserveKg) {
                        ascent_.loxReserveKg = want;
                        ascent_.tliOxidiserKg -= 100.0;
                        audio_.play(Sfx::Select);
                    } else audio_.play(Sfx::Deny);
                }
                break;
        case 3: break;
        default: break;
    }

    // Track the resulting wet mass so the payload consequence is visible.
    ascent_.wetMassKg = ascent_.dryMassKg + ascent_.tliFuelKg + ascent_.tliOxidiserKg;
    if (ascent_.dryMassKg <= 0.0) {
        // A nominal dry stack; heavier tanks cost dry mass, which the manifest
        // screen already implied.
        ascent_.dryMassKg = balance_.leoWetMassKg - 11000.0;
    }

    if (in().pressed(Key::Space) || in().pressed(Key::Enter)) {
        if (ascent_.tliFuelKg < 4000.0) {
            notify("You cannot buy a translunar injection with that.", kRedHi);
            audio_.play(Sfx::Deny);
        } else if (ascent_.loxReserveKg < 200.0) {
            notify("Reserve at least 200 kg LOX or you cannot land.", kRedHi);
            audio_.play(Sfx::Deny);
        } else {
            ascent_.tliWindow = 0;
            tliSol_ = balance_.tliWindowOpenSol;
            tliDone_ = false;
            setScene(Scene::Tli);
            audio_.play(Sfx::Select);
        }
    }
}

void Game::drawDepot() {
    Renderer& r = ren();
    const Balance& b = balance_;
    r.fillRect(0, 0, kScreenW, kScreenH, kUiBlack);
    drawBackdrop(r, static_cast<int>(frame_), kVoidDark, kVoidNear, kMetalDeep);

    const int window = depotRow_ / 4;
    const double scale = 1.0 + b.tliWindowEscalation * static_cast<double>(window);

    r.drawTextShadow(6, 4, "LEO REFUELLING DEPOT", kAmberHi, kAmberDeep);
    r.drawTextRight(kScreenW - 6, 4, fmtCredits(runState().credits), kGreenHi);

    char win[64];
    std::snprintf(win, sizeof(win), "REFUEL WINDOW %d/3   PRICE x%s", window + 1,
                  fmtFixed(scale, 2).c_str());
    r.drawTextCentered(kScreenW / 2, 15, win,
                       window == 2 ? kRedHi : (window == 1 ? kAmberHi : kUiGrey));

    if (ascent_.dryMassKg <= 0.0) ascent_.dryMassKg = b.leoWetMassKg - 11000.0;
    const double maxProp = b.leoWetMassKg * 0.42;
    const double wet = ascent_.dryMassKg + ascent_.tliFuelKg + ascent_.tliOxidiserKg;
    const double massFrac = wet / b.leoWetMassKg;

    // Rows.
    struct Row { const char* label; double val; double cap; const char* unit; uint8_t col; };
    const Row rows[3] = {
        {"TLI FUEL  (LCH4)", ascent_.tliFuelKg,  maxProp, "kg", kAmberHi},
        {"TLI OXIDISER (LOX)", ascent_.tliOxidiserKg, maxProp, "kg", kCyanHi},
        {"LOX HELD BACK FOR DESCENT", ascent_.loxReserveKg, maxProp, "kg", kGreenHi},
    };

    int y = 26;
    for (int i = 0; i < 3; ++i) {
        const bool sel = (depotRow_ == i);
        if (sel) {
            r.fillRect(10, y - 3, 300, 28, kMetalBlack);
            r.rect(10, y - 3, 300, 28, kCyanHi);
        }
        r.drawText(14, y, rows[i].label, sel ? kHiWhite : kUiWhite);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.0f %s", rows[i].val, rows[i].unit);
        r.drawTextRight(306, y, buf, rows[i].col);

        drawBar(r, 14, y + 10, 292, 7, rows[i].val / std::max(1.0, rows[i].cap), rows[i].col,
                kUiBlack, kUiGrey, 8);
        y += 30;
    }

    // Mass consequence readout -- the point of the whole screen.
    r.titledPanel(10, 118, 300, 46, "STACK MASS", kUiBlack, kAmber, kMetalDeep);
    drawStatusLine(r, 14, 130, 292, "Dry stack", fmtMass(ascent_.dryMassKg), kUiGrey, kUiWhite);
    drawStatusLine(r, 14, 139, 292, "Propellant acquired",
                     fmtMass(ascent_.tliFuelKg + ascent_.tliOxidiserKg), kUiGrey, kAmberHi);
    drawStatusLine(r, 14, 148, 292, "Wet mass at TLI",
                     fmtMass(ascent_.dryMassKg + ascent_.tliFuelKg + ascent_.tliOxidiserKg) +
                         "  /  " + fmtMass(b.leoWetMassKg),
                     kUiGrey, massFrac > 0.95 ? kRedHi : kUiWhite);
    drawBar(r, 14, 157, 292, 5, massFrac, massFrac > 0.95 ? kRed : kMetalMid, kUiBlack, kUiGrey);

    // Warnings stack rather than share a row. At x=14 and x=180 the two of them
    // met in the middle and each ran off the side of the screen.
    //
    // The stack mass panel ends at y=164 and the screen at y=200, so the two
    // warning rows, the key hints and the prompt have 36px between them. The
    // escalation hint went: the header already reads REFUEL WINDOW n/3.
    int warnY = 166;
    if (ascent_.tliFuelKg < 4000.0) {
        r.drawText(14, warnY, "NEED 4,000 kg FUEL FOR TLI.", kRedHi);
        warnY += 9;
    }
    if (ascent_.loxReserveKg < 200.0) {
        r.drawText(14, warnY, "RESERVE 200 kg LOX TO LAND.", kRedHi);
    }

    r.drawTextCentered(kScreenW / 2, 184, "UP/DOWN select   LEFT/RIGHT buy", kRegShadow);
    if ((static_cast<int>(frame_) / 26) % 2 == 0) {
        r.drawTextCentered(kScreenW / 2, 193, "SPACE to commit", kCyanHi);
    }
}

// ---------------------------------------------------------------- TLI

void Game::updateTli() {
    if (!tliDone_) {
        if (in().anyPressed()) tliDone_ = true;
        return;
    }
    if (in().pressed(Key::Enter) || in().pressed(Key::Space) ||
        in().mousePressed(MouseButton::Left)) {
        cruiseDay_ = 0;
        setScene(Scene::Cruise);
        audio_.play(Sfx::Select);
    }
}

void Game::drawTli() {
    Renderer& r = ren();
    const Balance& b = balance_;
    r.fillRect(0, 0, kScreenW, kScreenH, kUiBlack);
    drawBackdrop(r, static_cast<int>(frame_), kVoidDark, kVoidNear, kCyanDeep);

    r.drawTextCenteredScaled(kScreenW / 2, 16, "TRANS-LUNAR", kUiWhite, 2);
    r.drawTextCentered(kScreenW / 2, 40, "INJECTION WINDOW", kCyanHi);

    // Window band.
    const int wx = 30, wy = 58, ww = 260;
    r.rect(wx, wy, ww, 22, kMetalDeep);
    r.fillRect(wx + 1, wy + 1, ww - 2, 20, kUiBlack);
    const int total = b.tliWindowCloseSol - b.tliWindowOpenSol + 1;
    for (int i = 0; i < total; ++i) {
        const int cx = wx + 8 + (i * (ww - 16)) / total;
        r.fillRect(cx, wy + 4, (ww - 16) / total - 2, 14, kGreenDeep);
        r.drawText(cx + 3, wy + 8,
                   "D" + fmtInt(b.tliWindowOpenSol + i), kGreenHi);
    }
    r.drawTextRight(kScreenW - 6, wy - 10, "CLOSES SOL " + fmtInt(b.tliWindowCloseSol), kAmberHi);

    // Burn arithmetic, shown plainly.
    r.titledPanel(20, 90, 280, 56, "BURN PLAN", kUiBlack, kCyan, kMetalDeep);
    drawStatusLine(r, 24, 102, 272, "Fuel expended",
                     fmtMass(ascent_.tliFuelKg), kUiGrey, kAmberHi);
    drawStatusLine(r, 24, 111, 272, "Oxidiser expended",
                     fmtMass(ascent_.tliOxidiserKg), kUiGrey, kCyanHi);
    drawStatusLine(r, 24, 120, 272, "Held back for descent",
                     fmtMass(ascent_.loxReserveKg), kUiGrey, kGreenHi);
    drawStatusLine(r, 24, 129, 272, "Arrival mass",
                     fmtMass(ascent_.loxReserveKg + 600.0), kUiGrey, kUiWhite);
    r.drawText(24, 138, "Whatever you spend here is what you do not land with.", kRegShadow);

    if (!tliDone_) {
        if ((static_cast<int>(frame_) / 22) % 2 == 0) {
            r.drawTextCentered(kScreenW / 2, 160, "MAIN ENGINE IGNITION", kAmberHi);
        }
        r.drawTextCentered(kScreenW / 2, 176, "press any key", kRegShadow);
    } else {
        if ((static_cast<int>(frame_) / 24) % 2 == 0) {
            r.drawTextCentered(kScreenW / 2, 160, "WINDOW CAUGHT. BURN COMPLETE.", kGreenHi);
        }
        r.drawTextCentered(kScreenW / 2, 176, "ENTER to continue", kRegShadow);
    }
}

// ---------------------------------------------------------------- cruise

void Game::updateCruise() {
    cruiseDay_ += 1;
    if (in().pressed(Key::Enter) || in().pressed(Key::Space)) {
        cruiseDay_ = 4;
    }
    if (cruiseDay_ >= 4) {
        lloiStage_ = 0;
        ascent_.llotMassKg = ascent_.loxReserveKg * 0.55;
        setScene(Scene::Lloi);
    }
}

void Game::drawCruise() {
    Renderer& r = ren();
    r.fillRect(0, 0, kScreenW, kScreenH, kVoidNear);
    drawBackdrop(r, static_cast<int>(frame_), kVoidDark, kVoidNear, kMetalDeep);

    r.drawTextShadow(6, 4, "CISLUNAR TRANSIT", kAmberHi, kAmberDeep);
    r.drawTextRight(kScreenW - 6, 4, "DAY " + fmtInt(std::min(4, cruiseDay_)) + " / 4", kCyan);

    // Telemetry. 16 + 76 = 92: six 9px rows from y=28 reach y=73 and the LOX
    // line at y=82 reaches y=89. At 66 tall the panel ended at y=82 and the LOX
    // line was drawn across its border.
    r.titledPanel(8, 16, 150, 76, "TELEMETRY", kUiBlack, kCyan, kMetalDeep);
    const char* phase = cruiseDay_ < 2 ? "OUTBOUND" : "INBOUND";
    drawStatusLine(r, 12, 28, 142, "Phase", phase, kUiGrey, kUiWhite);
    drawStatusLine(r, 12, 37, 142, "Range",
                     fmtFixed(380000.0 - cruiseDay_ * 90000.0, 0) + " km", kUiGrey, kUiWhite);
    drawStatusLine(r, 12, 46, 142, "Closing",
                     fmtFixed(600.0 + cruiseDay_ * 140.0, 0) + " m/s", kUiGrey, kCyanHi);
    drawStatusLine(r, 12, 55, 142, "Crew",
                     fmtInt(runState().livingCrew()) + " nominal", kUiGrey, kGreenHi);
    drawStatusLine(r, 12, 64, 142, "Attitude",
                     cruiseDay_ < 2 ? "S-BAND" : "X-BAND", kUiGrey, kUiWhite);
    r.drawText(12, 82, "LOX reserve " + fmtMass(ascent_.loxReserveKg), kRegShadow);

    // Crew chatter. Short, in the register of the original's monologue screens,
    // and each line carries a fact the player needs.
    static const char* kLines[] = {
        "Okonkwo: \"Four days is nothing. It is the fifteen hundred after this that"
            " kills people.\"",
        "Reyes:    \"Water is the whole budget. Everything else we can live without"
            " for a while. Water, no.\"",
        "Lindqvist:\"If we are short on landing propellant, we are going to find out"
            " at fifteen kilometres up. That is not the place to find out.\"",
        "Nakamura: \"Seed is logged. Route forecast is in your inbox. Memorise it.\"",
        "All:      \"One hundred and twenty kilometres. Walked, not driven. Walked.\"",
    };
    const int idx = std::min(4, std::max(0, cruiseDay_ - 1));
    // Matched to the telemetry panel beside it: 16 + 76 = 92, and the longest
    // line of chatter wraps to eight 8px rows from y=28, reaching y=91.
    r.titledPanel(164, 16, 148, 76, "CREW COMMS", kUiBlack, kCyan, kMetalDeep);
    r.drawTextWrapped(168, 28, 140, kLines[idx], kUiWhite, 8);

    drawBar(r, 8, 98, 304, 8, static_cast<double>(cruiseDay_) / 4.0, kCyan, kUiBlack, kUiGrey);
    r.drawTextCentered(kScreenW / 2, 112, "LUNAR ORBIT INSERTION IMMINENT", kAmberHi);

    if ((static_cast<int>(frame_) / 24) % 2 == 0) {
        r.drawTextCentered(kScreenW / 2, 130, "ENTER to skip ahead", kRegShadow);
    }
}

// ---------------------------------------------------------------- LLOI

void Game::updateLloi() {
    if (in().pressed(Key::Enter) || in().pressed(Key::Space)) {
        const double need = 900.0;
        if (ascent_.loxReserveKg < need + ascent_.llotMassKg) {
            lloiStage_ = 2;
            audio_.play(Sfx::Deny);
            notify("Not enough propellant to insert and still land.", kRedHi);
        } else {
            ++lloiStage_;
            audio_.play(Sfx::Select);
        }
    }
    if (lloiStage_ >= 2) {
        // Commit to descent with whatever cargo made it down.
        const double cargo = std::min(ascent_.descentCargoKg, ascent_.loxReserveKg * 0.25);
        runState().credits = 1800.0;
        runState().stock.cargoKg = cargo;
        runState().stock.fuelKg = 60.0;
        runState().stock.oxidiserKg = 240.0;
        descent().reset(balance_, cargo);
        setScene(Scene::Descent);
    }
}

void Game::drawLloi() {
    Renderer& r = ren();
    r.fillRect(0, 0, kScreenW, kScreenH, kUiBlack);
    drawBackdrop(r, static_cast<int>(frame_), kVoidNear, kVoidDark, kVoidDeep);

    r.drawTextCenteredScaled(kScreenW / 2, 14, "LUNAR ORBIT", kUiWhite, 2);
    r.drawTextCentered(kScreenW / 2, 36, "INSERTION", kCyanHi);

    r.titledPanel(20, 50, 280, 66, "CAPTURE BURN", kUiBlack, kCyan, kMetalDeep);
    const double need = 900.0;
    drawStatusLine(r, 24, 62, 272, "Target orbit", "100 x 100 km polar", kUiGrey, kUiWhite);
    drawStatusLine(r, 24, 71, 272, "Burn propellant required", fmtMass(need), kUiGrey, kAmberHi);
    drawStatusLine(r, 24, 80, 272, "Allocated", fmtMass(ascent_.llotMassKg), kUiGrey, kUiWhite);
    drawStatusLine(r, 24, 89, 272, "LOX remaining after", fmtMass(ascent_.loxReserveKg - need),
                     kUiGrey, kGreenHi);
    r.drawText(24, 100, "Skipping insertion costs 300 kg more at touchdown.", kRegShadow);

    const char* stage = lloiStage_ == 0 ? "PRESS ENTER TO BURN"
                       : lloiStage_ == 1 ? "INSERTION COMPLETE. 100 x 100 km."
                                         : "INSUFFICIENT PROPELLANT";
    r.drawTextCentered(kScreenW / 2, 126, stage,
                       lloiStage_ == 1 ? kGreenHi : (lloiStage_ == 2 ? kRedHi : kAmberHi));

    if ((static_cast<int>(frame_) / 24) % 2 == 0) {
        r.drawTextCentered(kScreenW / 2, 148, "ENTER to continue", kRegShadow);
    }
}

}  // namespace lt
