// The health model, ported from the original as closely as the theme allows.
//
// Oregon Trail's Appendix B specifies: health is a value from 0 (ideal) to 140
// (death), and each day the value is first DECREMENTED by 10% (natural
// recovery) and THEN the day's modifiers are added. That ordering is not
// incidental -- because recovery is applied before additions, a party at 140
// that survives one tick lands at 126 rather than dying instantly, which is
// what creates the "within a few days" window the manual describes.
//
// The hardest rule in the original is preserved verbatim: if a crewmember is
// already malady-afflicted and suffers another, they die. The one escape is
// the Physician, which is that crewmember's whole value proposition.
#pragma once

#include <string>
#include <vector>

#include "core/rng.h"
#include "sim/balance.h"
#include "sim/convoy.h"

namespace lt {

// Accumulates one day's worth of health modifiers, then applies them.
struct HealthMods {
    double pace = 0.0;
    double rations = 0.0;
    double thermal = 0.0;
    double o2Shortage = 0.0;
    double waterShortage = 0.0;
    double foodShortage = 0.0;
    double powerFailure = 0.0;
    double propellantStarvation = 0.0;  // e.g. a chasm that could not be crossed
    double eventDamage = 0.0;

    double total() const {
        return pace + rations + thermal + o2Shortage + waterShortage + foodShortage +
               powerFailure + propellantStarvation + eventDamage;
    }
};

// Applies one day's health update in the original's exact order.
// Returns the band the party ended the day in.
HealthBand applyDailyHealth(RunState& s, const HealthMods& mods, const Balance& b);

// Rolls for a new malady on one randomly chosen living, healthy crewmember.
//
// Odds rise from 0 at perfect health to illnessChanceMax at worst, matching the
// documented 0-40% band. The lethal rule: an already-afflicted crewmember who
// rolls a malady dies, unless a Physician is alive.
//
// Returns true if somebody was newly afflicted, injured, or died.
bool rollMalady(RunState& s, Rng& rng, const Balance& b, std::string* report);

// Ticks recovery timers. A crewmember recovering this sol returns to nominal.
void tickRecovery(RunState& s, const Balance& b, std::vector<std::string>* log);

// Builds the standard per-sol modifiers from current state.
HealthMods baseDailyMods(const RunState& s, const Balance& b);

// Checks the hard loss conditions and sets outcome/lossReason if one fires.
void checkLossConditions(RunState& s, const Balance& b);

}  // namespace lt
