// The hazard table.
//
// The original's manual lists 23 random events and states plainly that "the
// probability of these events occurring is not fixed but depends upon the
// current circumstances." Bill Heinemann's contribution to that design was
// tying event likelihood to geography, so that cold-weather events were more
// likely in the mountains and theft more likely on the plains. That is
// reproduced here: every event carries a weight function, and the weight is
// evaluated against the run's current state, not rolled flat.
//
// Note also what is absent, because its absence is load-bearing: the 1985
// design REMOVED the original's hostile-riders combat concept. There is no
// combat in Lunar Trail. Every hazard is something you cannot fight, only
// prepare for. The only counterplay is what you loaded at the depot.

#pragma once

#include <string>
#include <vector>

#include "core/rng.h"
#include "sim/balance.h"
#include "sim/convoy.h"

namespace lt {

enum class HazardId {
    None,
    // Weather / environment
    DustStorm,
    SolarParticleEvent,
    MmodImpact,
    ElectrolysisStackFailure,
    O2LoopContamination,
    ThermalCycling,
    // Navigation
    NavBeaconLost,
    LostTheTrail,
    WrongTrail,
    BoulderField,
    ImpassableSlope,
    // Hardware
    WheelFailure,
    BogieFailure,
    SealBreach,
    GyroDrift,
    BatteryRunaway,
    // Crew
    CrewMemberLost,
    CrewInjury,
    // Gains and losses
    BayFire,
    ScavengerRaid,
    DerelictCache,
    PriorWreck,
    Count
};

struct Hazard {
    HazardId id = HazardId::None;
    std::string name;
    // Weight per sol. Evaluated against live state; these are the base numbers.
    double baseWeight = 1.0;
    // Optional context multipliers.
    bool psrOnly = false;
    bool sunlitOnly = false;
};

const std::vector<Hazard>& hazardTable();

// Display name for a hazard id, for logging and the balance harness.
const char* hazardName(HazardId id);

// Rolls at most one hazard for the sol. Returns HazardId::None when the roll
// comes up empty, which is common -- the original fires an event most days, not
// all of them.
HazardId rollHazard(RunState& s, Rng& rng, const Balance& b);

// Applies a hazard. Appends human-readable lines to `log` exactly as the
// original's event notices read.
void applyHazard(RunState& s, HazardId id, Rng& rng, const Balance& b,
                 std::vector<std::string>* log);

// The mercy valve. Fires only when a resource is at literally zero, and only
// while travelling between landmarks -- the original's "An Indian helped you
// find some food", +30 lbs, which reliably rescues runs that were one sol from
// an unwinnable position. Preserved.
// Writes a single sentence into `outMessage` when it fires.
bool rollMercyValve(RunState& s, Rng& rng, const Balance& b, std::string* outMessage);

// Hazards do not fire while resting. Preserved from the original, and a genuine
// exploit that every veteran knows about.
void applyRestingRecovery(RunState& s, const Balance& b, int sols, std::vector<std::string>* log);

}  // namespace lt
