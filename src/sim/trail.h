// The route: 16 landmarks over 120 km, and the chasm-crossing module.
//
// Direct structural translation of the original's 18-landmark / 20-segment
// network, including the branch point (South Pass -> Green River or Fort
// Bridger) and the fact that some crossings cannot be avoided at all.
//
// The chasm crossings use the original's river algorithm shape exactly:
//
//   depth <  2.5      low risk
//   2.5 .. 3.0        swamped: no loss, but one sol lost drying out
//   >  3.0            loss probability rises LINEARLY with depth
//
// with 'depth' reinterpreted as chasma depth in metres. The original's ferries
// exist at only two of its four rivers; here a guided crossing is available at
// only two of our five chasms, so at least two crossings are always a gamble
// or a purchase.

#pragma once

#include <string>
#include <vector>

#include "core/rng.h"
#include "sim/balance.h"
#include "sim/convoy.h"

namespace lt {

struct Landmark {
    int index = 0;
    std::string name;
    LandmarkKind kind = LandmarkKind::Waypoint;
    double km = 0.0;          // cumulative distance from the landing site
    Zone zone = Zone::Sunlit;
    int storeIndex = -1;      // >=0 => has a store at this markup tier
    double chasmaDepthM = 0.0;// for chasms
    double chasmaSpanM = 0.0;
    bool branch = false;      // player chooses the next landmark here
    int altNext = -1;         // the other branch's next landmark
    bool guidedAvailable = false;  // "ferry"
    std::string blurb;
};

const std::vector<Landmark>& trail();

// The landmark whose km is >= `km`.
const Landmark& landmarkAtOrBefore(double km);

// Distance from `km` to the next landmark, in km.
double kmToNextLandmark(double km);

// Zone for a given distance along the route. Used when nothing else has set it.
Zone zoneAtKm(double km);

// ---- depot services ----

// Charges the batteries at a manned relay depot. Free, but it costs sols, and
// sols are the only resource the south pole does not run out of early. This is
// the reason a depot stop is a real decision rather than a shop visit.
//
// Returns true if anything was actually recharged.
bool rechargeAtDepot(RunState& s, const Landmark& lm, const Balance& b,
                     std::vector<std::string>* log);

// Sols required to bring the batteries from where they are to full.
int depotRechargeSolsNeeded(const RunState& s, const Balance& b);

// ---- chasm crossings ----

struct ChasmPlan {
    ChasmMethod method = ChasmMethod::None;
    bool available = false;
    std::string unavailableReason;
    double costCredits = 0.0;
    int costSuitSets = 0;
    double costPropellantKg = 0.0;
    double energyKwh = 0.0;
    int cuttingCharges = 0;
    int solsCost = 0;
    double risk = 0.0;        // probability of an accident
    std::string description;
};

// Enumerates every crossing method available for `lm`, annotated with cost and
// risk for the current run. The UI renders this list verbatim.
std::vector<ChasmPlan> chasmOptions(const RunState& s, const Landmark& lm, const Balance& b);

struct ChasmResult {
    bool succeeded = false;
    bool catastrophic = false;   // total loss, possible drowning/stranding
    std::string headline;
    std::vector<std::string> lines;
    double cargoLostKg = 0.0;
    double waterLostL = 0.0;
    double o2LostKg = 0.0;
    double fuelLostKg = 0.0;
    double oxidiserLostKg = 0.0;
    int crewLost = 0;
    int solsLost = 0;
    double kmGained = 0.0;      // full chasm, or 0 if blocked
};

// Resolves a crossing attempt, mutating the run. Never returns without either
// crossing or stranding the convoy.
ChasmResult resolveChasm(RunState& s, const Landmark& lm, const ChasmMethod method,
                         Rng& rng, const Balance& b);

}  // namespace lt
