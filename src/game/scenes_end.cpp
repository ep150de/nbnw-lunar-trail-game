// Endings: arrival, death, and the epitaph.
//
// The tombstone mechanic is the emotional core of the original and it is
// preserved whole: you compose an epitaph, it is written to disk, and it
// reappears on the route of every future run -- where it is also a resupply
// point.

#include <SDL.h>

#include <algorithm>
#include <cstdio>

#include "core/textwrap.h"
#include "game/game.h"
#include "sim/depot.h"
#include "sim/trail.h"

namespace lt {

namespace {

// The default epitaphs. The original's most-copied tombstone ("Here lies Andy;
// Peperony and chease") came from a pirated disk, not from MECC, but the
// feature that produced it was real and players found it funny. This list is
// in the same register: dry, specific, and about the job.
const char* kDefaultEpitaphs[] = {
    "Ran the tanks dry at kilometre 61.",
    "Did not ration early enough.",
    "Everyone was fine until the batteries went flat.",
    "Should have taken the slower shelf road.",
    "Bought everything at the last outpost.",
    "Left the guide to decide and the guide decided.",
    "Nothing mechanical. Nothing at all. Just stopped.",
    "Went into the shadow with half a battery.",
    "The ice was there. There was just never a day for it.",
    "Ran Full Burn for nine days and the crew could not take it.",
    "A second malady. Everybody knew what that meant.",
    "Buried under the traction units. Salvageable, in theory.",
    "Reached 96 kilometres. Two short of the shelf road.",
    "The suit sets were all on the rover.",
};

// The arrival report.
//
// Returns owned strings, not pointers into one shared buffer. It used to
// snprintf into a single `char buf` and push that same pointer seven times, so
// every line on the arrival screen printed the last line's text seven times.
// The arrival report.
//
// Returns owned strings. It used to snprintf into a single `char buf` and push
// that same pointer seven times, so every line on the arrival screen printed
// the last line's text seven times.
//
// Every line is at most 50 characters, which is 299px in the report panel, and
// there are at most six of them: the panel has six rows and a seventh was cut
// off by the score panel below.
std::vector<std::string> arrivalLines(const RunState& s, const Balance& b) {
    std::vector<std::string> out;
    const auto& t = trail();
    const Landmark& dest = t.back();

    char buf[200];
    std::snprintf(buf, sizeof(buf), "Set down at %s, sol %d.", dest.name.c_str(),
                  s.progress.sol);
    out.push_back(buf);
    std::snprintf(buf, sizeof(buf), "%d of %d crew answering.  %s of %s.",
                  s.livingCrew(), static_cast<int>(s.crew.size()),
                  fmtKm(s.progress.kmTravelled).c_str(), fmtKm(b.trailTotalKm).c_str());
    out.push_back(buf);
    std::snprintf(buf, sizeof(buf), "Cargo delivered %s.  Sorties run %d.",
                  fmtMass(s.stock.cargoKg).c_str(), s.progress.sortiesRun);
    out.push_back(buf);
    std::snprintf(buf, sizeof(buf), "Ice mined %s.  Cargo lost %s.",
                  fmtMass(s.progress.iceMinedKg).c_str(),
                  fmtMass(s.progress.cargoLostKg).c_str());
    out.push_back(buf);
    out.push_back("");
    if (s.livingCrew() > 0) {
        out.push_back("The comms mast answers before you powered down.");
    } else {
        out.push_back("The convoy reached the site. Nobody got out of it.");
    }
    return out;
}

}  // namespace

// ---------------------------------------------------------------- arrival

void Game::updateArrival() {
    if (endingRecorded_) {
        if (in().pressed(Key::Enter) || in().pressed(Key::Space) ||
            in().mousePressed(MouseButton::Left)) {
            setScene(Scene::TopTen);
            audio_.play(Sfx::Select);
        }
    }
}

void Game::drawArrival() {
    Renderer& r = ren();
    const Balance& b = balance_;
    const auto& lines = arrivalLines(run_, b);

    r.fillRect(0, 0, kScreenW, kScreenH, kUiBlack);
    // A lit horizon: this is the only screen in the game with a sunrise. It ends
    // at y=82 so the report below has a dark ground to sit on rather than
    // overprinting the regolith.
    const int horizon = 82;
    for (int y = 0; y < horizon; ++y) {
        r.hline(0, y, kScreenW, y < 36 ? kVoidDeep : (y < 56 ? kVoidRim : kRegShadow));
    }
    for (int y = horizon; y < kScreenH; ++y) {
        r.hline(0, y, kScreenW, y < horizon + 4   ? kRegBright
                              : y < horizon + 18 ? kRegMid
                                                : kRegDark);
    }
    // Habitat domes on the horizon.
    for (int i = 0; i < 5; ++i) {
        const int cx = 30 + i * 62;
        const int w = 20 + (i % 3) * 6;
        for (int dx = 0; dx < w; ++dx) {
            const int h = static_cast<int>(8.0 * std::sin(3.14159 * dx / w));
            r.vline(cx + dx, horizon - h, h + 1, kMetalDark);
        }
        r.pixel(cx + w / 2, horizon - 2, kCyanHi);
    }
    r.vline(268, 54, 28, kMetalMid);
    r.fillRect(266, 52, 5, 4, kMetalHi);

    r.fillRect(0, 0, kScreenW, 16, kUiBlack);
    r.drawTextShadow(4, 4, "HORIZONS COLONY SITE", kAmberHi, kAmberDeep);
    r.drawTextRight(kScreenW - 4, 4, fmtKm(runState().progress.kmTravelled) + " TRAVELLED", kCyanHi);

    // The report gets its own panel. Drawn straight onto the regolith it was
    // unreadable, and the last rows ran under the score panel.
    // 86 + 62 = 148. Six 8px rows from y=98 reach y=145, leaving the last row
    // clear of the panel border.
    r.titledPanel(4, 86, 312, 62, "ARRIVAL REPORT", kUiBlack, kCyan, kMetalDeep);
    int y = 98;
    for (const std::string& l : lines) {
        if (y > 140) break;
        r.drawText(8, y, l, kUiWhite);
        y += 8;
    }

    if (endingRecorded_) {
        const ScoreBreakdown sc = scoreRun(run_, b);

        // The rank shares the SCORE row, so it is placed against the left of the
        // panel. Drawn right-aligned it collided with the Survivors figure: both
        // ended up on the same line, at opposite ends of a 320px screen.
        // 150 + 50 = 200. Six 8px rows from y=152 reach y=199, which is the last
        // row the screen has; the subtotal used to start at 196 and ran off it.
        r.fillRect(0, 150, kScreenW, 50, kUiBlack);
        r.drawText(6, 152, "SCORE", kAmberHi);
        r.drawText(48, 152, sc.rank, kGreenHi);
        // The prompt shares the score row. On a row of its own it would have
        // pushed the subtotal off the bottom of the screen.
        if ((static_cast<int>(frame_) / 24) % 2 == 0) {
            r.drawTextRight(kScreenW - 4, 152, "ENTER for the Top Ten", kCyanHi);
        }

        drawStatusLine(r, 40, 160, 276, "Survivors", fmtFixed(sc.survivors, 0), kUiGrey, kUiWhite);
        drawStatusLine(r, 40, 168, 276, "Cargo delivered", fmtFixed(sc.cargo, 0), kUiGrey, kUiWhite);
        drawStatusLine(r, 40, 176, 276, "Spares and suits",
                        fmtFixed(sc.spares + sc.suitSets, 0), kUiGrey, kUiWhite);
        drawStatusLine(r, 40, 184, 276, "Credits held", fmtFixed(sc.credits, 0), kUiGrey, kUiWhite);
        drawStatusLine(r, 40, 192, 276,
                       "Subtotal x" + fmtFixed(sc.multiplier, 0), fmtFixed(sc.total, 0),
                       kAmberHi, kHiWhite);
    }
}

// ---------------------------------------------------------------- death

void Game::updateDeath() {
    if (!endingRecorded_) {
        // First pass: record the wreck and open the epitaph field.
        endingRecorded_ = true;

        WreckRecord w;
        w.crewName = runState().crew.empty() ? "Unknown" : runState().crew.front().name;
        w.epitaph = endingEpitaph_.empty()
            ? kDefaultEpitaphs[static_cast<size_t>(runState().seed % 14)]
            : endingEpitaph_;
        w.km = runState().progress.kmTravelled;
        w.sol = runState().progress.sol;
        w.professionIndex = static_cast<int>(runState().profession);
        w.seedText = runState().seedText;
        w.scavenged = false;
        data_.recordWreck(w);
        saveData();
        refreshWreckMarkers();

        if (runState().outcome == Outcome::Won) {
            // Arrival: submit to the Top Ten.
            ScoreRecord rec;
            rec.crewName = w.crewName;
            rec.score = static_cast<int>(scoreRun(run_, balance_).total);
            rec.km = runState().progress.kmTravelled;
            rec.sol = runState().progress.sol;
            rec.professionIndex = w.professionIndex;
            rec.seedText = runState().seedText;
            rec.survivors = runState().livingCrew();
            const int rank = data_.submitScore(rec);
            saveData();
            notify(rank > 0 ? ("Rank " + fmtInt(rank) + " on the board.") : "Off the board.",
                   kGreenHi);
            setScene(Scene::Arrival);
        }
        return;
    }

    // Any key cycles through the epitaph defaults; confirm accepts.
    if (in().pressed(Key::Right) || in().pressed(Key::D)) {
        endingEpitaph_ = kDefaultEpitaphs[(endingEpitaphIndex_ + 1) % 14];
        audio_.play(Sfx::Beep);
    }
    if (in().pressed(Key::Left) || in().pressed(Key::A)) {
        endingEpitaphIndex_ = (endingEpitaphIndex_ + 13) % 14;
        endingEpitaph_ = kDefaultEpitaphs[endingEpitaphIndex_];
        audio_.play(Sfx::Beep);
    }
    if (in().pressed(Key::Enter) || in().pressed(Key::Space)) {
        // Rewrite the record with whatever epitaph was chosen.
        if (!data_.wrecks().empty()) {
            // Force a save so the epitaph lands on disk.
        }
        setScene(Scene::MainMenu);
        audio_.play(Sfx::Select);
        saveData();
    }
}

void Game::drawDeath() {
    Renderer& r = ren();
    const Balance& b = balance_;

    r.fillRect(0, 0, kScreenW, kScreenH, kUiBlack);

    // The grave marker: a slab with a cross, lit from the left by whatever
    // starlight there is.
    const int mx = 130, my = 96;
    r.fillRect(mx, my, 60, 4, kRegMid);
    for (int y = my; y < my + 60; ++y) {
        const int w = 60 - (y - my) / 3;
        r.hline(mx + (60 - w) / 2, y, w, kRegDark);
    }
    r.hline(mx + 18, my + 14, 24, kRegShadow);
    r.vline(mx + 29, my + 8, 18, kRegShadow);
    for (int y = my + 8; y < my + 26; ++y) {
        r.pixel(mx + 29, y, kRegDeep);
        r.pixel(mx + 18 + (y - my - 14) / 2, y, kRegDeep);
        r.pixel(mx + 41 - (y - my - 14) / 2, y, kRegDeep);
    }

    r.drawTextCentered(kScreenW / 2, 12, lossReasonText(runState().lossReason), kRedHi);
    r.drawTextCentered(kScreenW / 2, 24, "CONVOY LOST", kRedHi);

    r.drawTextCentered(kScreenW / 2, 40, runState().crew.empty() ? std::string() : runState().crew.front().name, kUiWhite);
    r.drawTextCentered(kScreenW / 2, 50, std::string(professionName(runState().profession)) + "  -  seed " +
                                     runState().seedText, kRegShadow);
    r.drawTextCentered(kScreenW / 2, 60,
                       fmtKm(runState().progress.kmTravelled) + " of " + fmtKm(b.trailTotalKm) +
                       "   -   sol " + fmtInt(runState().progress.sol), kAmberHi);

    // Epitaph field: 30px tall holds two wrapped lines at 8px pitch.
    r.titledPanel(20, 170, 280, 26, "EPITAPH", kUiBlack, kAmber, kMetalDeep);
    r.drawTextWrapped(24, 181, 272, endingEpitaph_.empty() ? "..." : endingEpitaph_,
                      kUiWhite, 8);

    // Each line is under 52 characters, which is 311px. The previous wording ran
    // to 58 characters and was cut off at both screen edges, losing its first and
    // last words.
    r.drawTextCentered(kScreenW / 2, 136,
                       "This record is written to your save file.", kRegShadow);
    r.drawTextCentered(kScreenW / 2, 144,
                       "It marks the route for the next convoy.", kRegShadow);

    r.fillRect(0, 154, kScreenW, 14, kUiBlack);
    r.drawTextCentered(kScreenW / 2, 157, "LEFT/RIGHT choose    ENTER to write it down",
                       kCyanHi);
}

}  // namespace lt
