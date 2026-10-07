// The persistent state of a run: crew, manifest, hardware, stock, and progress.
//
// Everything the simulation reads or writes lives here. Scenes mutate it; the
// sim layer reads it. Keeping it in one flat struct (rather than scattering
// state across subsystems) is what makes a headless balance harness possible --
// see tools/balance_report.py and tests/.
#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "sim/balance.h"

namespace lt {

// ---------------------------------------------------------------- enums

enum class Profession {
    FlightDirector,      // 4200 cr, x1 -- the easy mode
    MissionSpecialist,   // 2100 cr, x2
    PayloadTechnician,   //  900 cr, x3 -- must mine most of its propellant
};

const char* professionName(Profession p);
const char* professionBlurb(Profession p);
double professionCredits(Profession p, const Balance& b);
double professionScoreMult(Profession p, const Balance& b);

enum class Perk {
    None,
    Physician,   // malady is survivable a second time
    Geologist,   // prospecting yield bonus
    Engineer,    // repairs succeed
    Mechanic,    // hardware failure rates halved
    Comms,       // reveals the route seed and hazard forecast
};

const char* perkName(Perk p);
const char* perkBlurb(Perk p);

enum class Malady {
    None,
    // Ailments and diseases: 10 sols to recover.
    Exhaustion, Hypoxia, Toxicity, DecompressionSickness, DustPneumoconiosis,
    // Injuries: 30 sols.
    Fracture, Sprain, Crush, Puncture,
};

const char* maladyName(Malady m);
// Returns false for Malady::None.
bool maladyIsInjury(Malady m);
int maladyRecoverySols(Malady m, const Balance& b);

enum class HealthBand { Good, Fair, Poor, VeryPoor, Dying };
const char* healthBandName(HealthBand h);

enum class Pace { Cruise, Sprint, FullBurn };
const char* paceName(Pace p);
const char* paceBlurb(Pace p);
double paceFactor(Pace p, const Balance& b);

enum class Rations { Full, Reduced, Emergency };
const char* rationsName(Rations r);
// Abbreviated for tight status bars.
const char* rationsShort(Rations r);
const char* rationsBlurb(Rations r);
double rationsMultiplier(Rations r);

enum class Zone { Sunlit, Transition, Psr };
const char* zoneName(Zone z);
const char* zoneBlurb(Zone z);
double zoneTerrainFactor(Zone z, const Balance& b);
double zoneSolarKwh(Zone z, const Balance& b);
double zoneIcePerCrew(Zone z, const Balance& b);

enum class LandmarkKind { Depot, Chasm, Waypoint, Colony };
const char* landmarkKindName(LandmarkKind k);

enum class ChasmMethod { Ramp, Winch, Hop, Guided, Guide, None };
const char* chasmMethodName(ChasmMethod m);

enum class Outcome { Playing, Won, Lost };

enum class LossReason {
    None,
    PartyDead,
    StrandedNoPower,
    StrandedNoTraction,
    OutOfPropellant,
    TooLate,          // past the hard sol limit
    DescentCrash,
    Aborted,
};
const char* lossReasonText(LossReason r);

// ---------------------------------------------------------------- crew

struct CrewMember {
    std::string name;
    Perk perk = Perk::None;
    Malady malady = Malady::None;
    int recoverSol = 0;      // sols remaining until recovered
    bool dead = false;
    std::string causeOfDeath;

    bool active() const { return !dead; }
    bool maladied() const { return malady != Malady::None; }
};

// ---------------------------------------------------------------- hardware

struct Hardware {
    int tractionUnits = 6;     // wheels fitted; <6 costs speed
    int tractionSick = 0;      // still turning, but slow: each counts as half
    int tractionDead = 0;      // failed outright and not yet replaced
    int bogies = 3;
    int bogiesSick = 0;
    int sparesWheel = 0;
    int sparesBogie = 0;
    int sparesSeal = 0;

    // Set when a unit failed and could not be repaired in the field. This is not
    // automatically fatal: the convoy limps on whatever wheels it still has, and
    // fitting a spare at the next depot clears it. This mirrors the original,
    // where an unrepairable break stopped you dead but a part bought from a
    // trader got you moving again -- the trade option existed precisely so that
    // a break was a money problem rather than a run-ending one.
    bool needsWheelRepair = false;
    bool needsBogieRepair = false;

    // A complete loss of drive is fatal, exactly as "You have no more oxen."
    bool immobile() const {
        const int usable = tractionUnits - tractionDead;
        return usable <= 0 || bogies <= 0;
    }

    // Effective traction, matching the original's rule that a sick ox counts as
    // half a healthy one and that fewer than the recommended number costs
    // speed proportionally.
    //
    // tractionSick is a count of units that are PRESENT but impaired, so the
    // healthy population is (total - sick) + 0.5 * sick.
    double effectiveTraction(int idealUnits) const {
        const int sick = std::min(tractionSick, std::max(0, tractionUnits - tractionDead));
        const double healthy = static_cast<double>(std::max(0, tractionUnits - tractionDead - sick)) +
                               0.5 * static_cast<double>(sick);
        const double ideal = static_cast<double>(idealUnits);
        return healthy >= ideal ? 1.0 : healthy / ideal;
    }
};

// ---------------------------------------------------------------- stock

struct Stock {
    double foodKg = 0.0;
    double waterL = 0.0;
    double o2Kg = 0.0;
    double fuelKg = 0.0;      // LCH4
    double oxidiserKg = 0.0;  // LOX
    int suitSets = 0;
    int cuttingCharges = 0;
    double cargoKg = 0.0;     // colony seed cargo; the thing you are actually delivering

    // Mass of the loose cargo in the bay. Spares are carried in the spares
    // rack, so totalPayloadKg adds them separately.
    double payloadKg() const {
        return foodKg + waterL + o2Kg + fuelKg + oxidiserKg + cargoKg +
               suitSets * 18.0 + cuttingCharges * 1.2;
    }

    // Total propellant, used for display and for the burn check.
    double propellantKg() const { return fuelKg + oxidiserKg; }
};

// ---------------------------------------------------------------- progress

struct Landmark;

struct Progress {
    double kmTravelled = 0.0;
    int sol = 0;                  // sols since landing
    int landmarkIndex = 0;        // next landmark to reach
    Zone zone = Zone::Sunlit;
    int sortiesRun = 0;
    double iceMinedKg = 0.0;
    double propellantBurnedKg = 0.0;
    double cargoLostKg = 0.0;
    int landmarksReached = 0;
    bool awaitingChasm = false;
    int chasmIndex = 0;
};

// ---------------------------------------------------------------- run state

struct RunState {
    // --- identity ---
    uint64_t seed = 0;
    std::string seedText;
    Profession profession = Profession::MissionSpecialist;
    std::vector<CrewMember> crew;

    // --- resources ---
    double credits = 0.0;
    Stock stock;
    Hardware hardware;
    double energyKwh = 0.0;     // battery state of charge

    // --- health ---
    // Oregon Trail's scale: 0 is ideal, 140 is death.
    double health = 0.0;

    // --- settings ---
    Pace pace = Pace::Cruise;
    Rations rations = Rations::Full;

    // --- progress ---
    Progress progress;

    // --- outcome ---
    Outcome outcome = Outcome::Playing;
    LossReason lossReason = LossReason::None;
    std::string epitaph;

    // ---- derived helpers ----

    int livingCrew() const;
    int maladiedCrew() const;
    // Remaining fuel-limited range at Full Burn, in km. 0 when out of propellant.
    double burnRangeKm(const Balance& b) const;
    // Propellant actually consumed per km at the given mixture.
    double propellantPerKm(const Balance& b) const;
    // Days of O2 remaining at current consumption, or -1 if unbounded.
    double o2DaysLeft(const Balance& b) const;
    double waterDaysLeft(const Balance& b) const;
    double foodDaysLeft(const Balance& b) const;
    HealthBand healthBand() const;
    // Rough forecast of how many more sols the batteries last at the current pace.
    double batterySolsLeft(const Balance& b) const;
    bool hasCommsOfficer() const;
    bool hasEngineer() const;
    bool hasPhysician() const;
    bool hasMechanic() const;
    double geologistBonus() const;
    // Total payload actually carried, including spares.
    double totalPayloadKg(const Balance& b) const;
    bool overPayloadCap(const Balance& b) const;
    bool hasProportions() const;   // enough of both fuel and oxidiser to burn
    double daysOfSunscreen() const;
};

// Builds a fresh run at sol 0 with the given profession and seed.
RunState newRun(uint64_t seed, Profession p, const Balance& b);

// The five default crew names offered on the manifest screen, with the perk
// each one carries. Deliberately not random: named crew make death land.
struct CrewOption {
    const char* name;
    Perk perk;
};
const std::vector<CrewOption>& crewRoster();

}  // namespace lt
