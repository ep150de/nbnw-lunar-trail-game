// The daily advance -- the heart of Chapter 2.
//
// The order of operations follows the original's inner loop precisely:
//
//   1. weather for (date, zone)
//   2. accumulate the environmental state (snow, soil moisture, water table)
//   3. compute health modifiers, roll illness
//   4. apply ration consumption
//   5. roll random events
//   6. compute distance travelled
//   7. advance the calendar
//
// That ordering is not cosmetic. Illness is rolled BEFORE rations are consumed
// and BEFORE events fire, which means a sol where you are about to run dry can
// still kill somebody first, and it means the day's distance is computed from
// the resources you actually had rather than the ones you were about to lose.

#pragma once

#include <string>
#include <vector>

#include "core/rng.h"
#include "sim/balance.h"
#include "sim/convoy.h"

namespace lt {

// What happened during one sol. Everything the UI needs to narrate it.
struct DayReport {
    int sol = 0;
    double kmStart = 0.0;
    double kmGained = 0.0;
    double kmEnd = 0.0;

    double solarKwhGained = 0.0;
    double energyKwhUsed = 0.0;
    double energyKwhEnd = 0.0;

    double propellantUsedKg = 0.0;
    double foodUsedKg = 0.0;
    double waterUsedL = 0.0;
    double o2UsedKg = 0.0;

    double healthBefore = 0.0;
    double healthAfter = 0.0;

    bool stalled = false;         // made no progress at all
    bool powerStarved = false;    // battery ran flat
    bool reachedLandmark = false;
    int landmarkIndex = -1;
    const char* landmarkName = "";

    // Set when the sol's travel stopped at the lip of a chasma. The crossing
    // itself is a separate decision (see sim/trail.h), so the day loop's job is
    // only to report that the convoy is standing at the edge and cannot advance
    // until someone chooses how to get across.
    bool atChasm = false;
    int chasmIndex = -1;

    std::vector<std::string> log;
};

// Advances one sol. Mutates `s`.
//
// `traveling` false means the convoy is holding position: it still consumes
// oxygen, water, food and life-support power, still accrues health damage, and
// still recovers from malady -- but generates no solar-driven charging beyond
// what the array collects in place, moves no distance, and fires no hazards.
DayReport advanceSol(RunState& s, Rng& rng, const Balance& b, bool traveling);

// Distance the rig is CAPABLE of covering today at the current pace, before the
// arrival clamp. Separated from projectedKmToday because the two numbers carry
// different information: the unclamped figure is what the pace setting buys you,
// the clamped figure is what you actually travel before hitting a landmark. The
// trail screen shows both, which is what turns pace choice from a guess into a
// calculation.
double projectedKmCapability(const RunState& s, const Balance& b);

// Distance that would actually be travelled today: the capability above, clamped
// to the distance remaining to the next landmark. Zero if blocked, out of power
// in the dark, or without usable propellant at Full Burn.
double projectedKmToday(const RunState& s, const Balance& b);

// A one-line summary of why the projected distance is what it is, for the UI.
std::string projectedKmReason(const RunState& s, const Balance& b);

// How a prospecting haul is split. Most of it is water to drink; the rest goes
// through electrolysis for oxygen and hydrogen, and the hydrogen into LCH4.
constexpr double kSortieDrinkFraction = 0.55;

// The hazard forecast: what the seeded route will throw at this convoy over the
// next `n` sols. Only meaningful with a Comms Officer, which is exactly the
// trade the original never offered.
std::vector<std::string> routeForecast(const RunState& s, uint64_t seed, const Balance& b, int sols);

}  // namespace lt
