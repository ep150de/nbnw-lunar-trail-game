#include "sim/convoy.h"

#include <algorithm>
#include <cmath>

namespace lt {

// ---------------------------------------------------------------- enums

const char* professionName(Profession p) {
    switch (p) {
        case Profession::FlightDirector:     return "Flight Director";
        case Profession::MissionSpecialist:  return "Mission Specialist";
        case Profession::PayloadTechnician:  return "Payload Technician";
    }
    return "?";
}

const char* professionBlurb(Profession p) {
    switch (p) {
        case Profession::FlightDirector:
            return "Full budget. The easy run.";
        case Profession::MissionSpecialist:
            return "Half budget. You will run short of something.";
        case Profession::PayloadTechnician:
            return "Almost nothing. You will mine most of your propellant.";
    }
    return "";
}

double professionCredits(Profession p, const Balance& b) {
    switch (p) {
        case Profession::FlightDirector:    return b.creditsFlightDirector;
        case Profession::MissionSpecialist: return b.creditsMissionSpecialist;
        case Profession::PayloadTechnician: return b.creditsPayloadTechnician;
    }
    return 0.0;
}

double professionScoreMult(Profession p, const Balance& b) {
    switch (p) {
        case Profession::FlightDirector:    return b.scoreMultFlightDirector;
        case Profession::MissionSpecialist: return b.scoreMultMissionSpecialist;
        case Profession::PayloadTechnician: return b.scoreMultPayloadTechnician;
    }
    return 1.0;
}

const char* perkName(Perk p) {
    switch (p) {
        case Perk::None:       return "Generalist";
        case Perk::Physician:  return "Physician";
        case Perk::Geologist:  return "Geologist";
        case Perk::Engineer:   return "Engineer";
        case Perk::Mechanic:   return "Mechanic";
        // Abbreviated for the manifest's crew column: the roster panel is 152px
        // wide and holds both the name and this label on one row. The manual
        // spells the specialty out in full.
        case Perk::Comms:      return "Comms Off.";
    }
    return "?";
}

const char* perkBlurb(Perk p) {
    switch (p) {
        case Perk::None:
            return "No special training.";
        case Perk::Physician:
            return "A second malady is survivable. Once.";
        case Perk::Geologist:
            return "Prospecting sorties yield 40% more ice.";
        case Perk::Engineer:
            return "Repairs almost always succeed.";
        case Perk::Mechanic:
            return "Hardware failures happen half as often.";
        case Perk::Comms:
            return "Reads the route forecast. No surprises.";
    }
    return "";
}

const char* maladyName(Malady m) {
    switch (m) {
        case Malady::None:                   return "nominal";
        case Malady::Exhaustion:             return "exhaustion";
        case Malady::Hypoxia:                return "hypobaric hypoxia";
        case Malady::Toxicity:               return "oxygen toxicity";
        case Malady::DecompressionSickness:  return "decompression sickness";
        case Malady::DustPneumoconiosis:     return "dust pneumoconiosis";
        case Malady::Fracture:               return "fractured limb";
        case Malady::Sprain:                 return "sprained joint";
        case Malady::Crush:                  return "crush injury";
        case Malady::Puncture:               return "suit puncture";
    }
    return "?";
}

bool maladyIsInjury(Malady m) {
    return m == Malady::Fracture || m == Malady::Sprain ||
           m == Malady::Crush    || m == Malady::Puncture;
}

int maladyRecoverySols(Malady m, const Balance& b) {
    return maladyIsInjury(m) ? b.injuryRecoverySols : b.ailmentRecoverySols;
}

const char* healthBandName(HealthBand h) {
    switch (h) {
        case HealthBand::Good:     return "Nominal";
        case HealthBand::Fair:     return "Degraded";
        case HealthBand::Poor:     return "Stressed";
        case HealthBand::VeryPoor: return "Critical";
        case HealthBand::Dying:    return "Failing";
    }
    return "?";
}

const char* paceName(Pace p) {
    switch (p) {
        case Pace::Cruise:    return "Cruise";
        case Pace::Sprint:    return "Sprint";
        case Pace::FullBurn:  return "Full Burn";
    }
    return "?";
}

const char* paceBlurb(Pace p) {
    switch (p) {
        case Pace::Cruise:
            return "Electric. 3 km/sol. Light on power, gentle on the rig.";
        case Pace::Sprint:
            return "Electric, hard. 6 km/sol. Heavy draw, wears drive gear.";
        case Pace::FullBurn:
            return "Chemical. 14 km/sol. Eats propellant and battery both.";
    }
    return "";
}

double paceFactor(Pace p, const Balance& b) {
    switch (p) {
        case Pace::Cruise:   return b.paceCruise;
        case Pace::Sprint:   return b.paceSprint;
        case Pace::FullBurn: return b.paceFullBurn;
    }
    return 1.0;
}

const char* rationsName(Rations r) {
    switch (r) {
        case Rations::Full:      return "Full Rations";
        case Rations::Reduced:   return "Reduced Rations";
        case Rations::Emergency: return "Emergency Rations";
    }
    return "?";
}

const char* rationsShort(Rations r) {
    switch (r) {
        case Rations::Full:      return "FULL";
        case Rations::Reduced:   return "REDUCED";
        case Rations::Emergency: return "EMERGENCY";
    }
    return "?";
}

const char* rationsBlurb(Rations r) {
    switch (r) {
        case Rations::Full:
            return "Everything the manifest calls for. No health cost.";
        case Rations::Reduced:
            return "Two thirds. Everyone is thinner. Slow health damage.";
        case Rations::Emergency:
            return "A quarter. Starving. Serious health damage.";
    }
    return "";
}

double rationsMultiplier(Rations r) {
    switch (r) {
        case Rations::Full:      return 1.0;
        case Rations::Reduced:   return 0.6;
        case Rations::Emergency: return 0.25;
    }
    return 1.0;
}

const char* zoneName(Zone z) {
    switch (z) {
        case Zone::Sunlit:     return "Sunlit Rim";
        case Zone::Transition: return "Transition Slope";
        case Zone::Psr:        return "Shadowed Region";
    }
    return "?";
}

const char* zoneBlurb(Zone z) {
    switch (z) {
        case Zone::Sunlit:
            return "Near-terminator ridge. Full sun. No ice.";
        case Zone::Transition:
            return "Between. Some sun, some ice. The compromise.";
        case Zone::Psr:
            return "Never sees the sun. Rich in ice. Zero solar.";
    }
    return "";
}

double zoneTerrainFactor(Zone z, const Balance& b) {
    switch (z) {
        case Zone::Sunlit:     return b.zoneTerrainSunlit;
        case Zone::Transition: return b.zoneTerrainTransition;
        case Zone::Psr:        return b.zoneTerrainPsr;
    }
    return 1.0;
}

double zoneSolarKwh(Zone z, const Balance& b) {
    switch (z) {
        case Zone::Sunlit:     return b.zoneSolarSunlitKwh;
        case Zone::Transition: return b.zoneSolarTransitionKwh;
        case Zone::Psr:        return b.zoneSolarPsrKwh;
    }
    return 0.0;
}

double zoneIcePerCrew(Zone z, const Balance& b) {
    switch (z) {
        case Zone::Sunlit:     return b.zoneIceSunlit;
        case Zone::Transition: return b.zoneIceTransition;
        case Zone::Psr:        return b.zoneIcePsr;
    }
    return 0.0;
}

const char* landmarkKindName(LandmarkKind k) {
    switch (k) {
        case LandmarkKind::Depot:    return "Relay Depot";
        case LandmarkKind::Chasm:    return "Chasma Crossing";
        case LandmarkKind::Waypoint: return "Survey Waypoint";
        case LandmarkKind::Colony:   return "Colony Site";
    }
    return "?";
}

const char* chasmMethodName(ChasmMethod m) {
    switch (m) {
        case ChasmMethod::Ramp:   return "Ramp Traverse";
        case ChasmMethod::Winch:  return "Grapple And Winch";
        case ChasmMethod::Hop:    return "Hop Assist";
        case ChasmMethod::Guided: return "Guided Crossing";
        case ChasmMethod::Guide:  return "Local Guide";
        case ChasmMethod::None:   return "--";
    }
    return "?";
}

const char* lossReasonText(LossReason r) {
    switch (r) {
        case LossReason::None:               return "";
        case LossReason::PartyDead:          return "No surviving crew.";
        case LossReason::StrandedNoPower:    return "Batteries depleted in the dark.";
        case LossReason::StrandedNoTraction: return "No usable traction. The convoy is immobile.";
        case LossReason::OutOfPropellant:    return "Out of propellant and nowhere to make more.";
        case LossReason::TooLate:            return "The crew rotation window closed. Nobody is coming.";
        case LossReason::DescentCrash:       return "The descent stage was lost.";
        case LossReason::Aborted:            return "The mission was scrubbed.";
    }
    return "";
}

// ---------------------------------------------------------------- derived

int RunState::livingCrew() const {
    int n = 0;
    for (const auto& c : crew) if (!c.dead) ++n;
    return n;
}

int RunState::maladiedCrew() const {
    int n = 0;
    for (const auto& c : crew) if (!c.dead && c.maladied()) ++n;
    return n;
}

double RunState::propellantPerKm(const Balance& b) const {
    // If the mixture is off, a burn wastes whatever it cannot use. This is the
    // oxidiser-lock mechanic: burning fuel you cannot oxidise costs you both.
    const double total = stock.propellantKg();
    if (total <= 0.0) return b.kgPropellantPerKm;

    const double ratio = b.oxidiserToFuelRatio;
    // A usable mixture has fuel:oxidiser == 1:ratio. Anything else is partially
    // stranded, so charge more per km.
    const double fuelFrac = stock.fuelKg / total;
    const double idealFuelFrac = 1.0 / (1.0 + ratio);
    const double imbalance = std::fabs(fuelFrac - idealFuelFrac);
    const double penalty = 1.0 + imbalance * 2.0;
    return b.kgPropellantPerKm * penalty;
}

double RunState::burnRangeKm(const Balance& b) const {
    if (stock.propellantKg() <= 0.0) return 0.0;
    return stock.propellantKg() / propellantPerKm(b);
}

bool RunState::hasProportions() const {
    // Both sides must be present. A tank of LOX with an empty fuel tank is dead
    // weight -- this is the single most common way players lose to themselves.
    return stock.fuelKg > 1.0 && stock.oxidiserKg > 1.0;
}

double RunState::o2DaysLeft(const Balance& b) const {
    const double rate = b.o2PerCrewKg * static_cast<double>(livingCrew());
    if (rate <= 0.0) return -1.0;
    return stock.o2Kg / rate;
}

double RunState::waterDaysLeft(const Balance& b) const {
    const double rate = b.waterPerCrewKg * static_cast<double>(livingCrew());
    if (rate <= 0.0) return -1.0;
    return stock.waterL / rate;
}

double RunState::foodDaysLeft(const Balance& b) const {
    const double rate = b.foodPerCrewKg * static_cast<double>(livingCrew()) *
                        rationsMultiplier(rations);
    if (rate <= 0.0) return -1.0;
    return stock.foodKg / rate;
}

HealthBand RunState::healthBand() const {
    if (health < 35.0)  return HealthBand::Good;
    if (health < 70.0)  return HealthBand::Fair;
    if (health < 105.0) return HealthBand::Poor;
    if (health < 140.0) return HealthBand::VeryPoor;
    return HealthBand::Dying;
}

double RunState::batterySolsLeft(const Balance& b) const {
    double driveKwh = b.driveEnergyCruiseKwh;
    if (pace == Pace::Sprint)   driveKwh = b.driveEnergySprintKwh;
    if (pace == Pace::FullBurn) driveKwh = b.driveEnergyBurnKwh;
    const double daily = driveKwh + b.energyPerCrewKwh * static_cast<double>(livingCrew());
    if (daily <= 0.0) return -1.0;
    return energyKwh / daily;
}

bool RunState::hasCommsOfficer() const {
    for (const auto& c : crew) if (c.perk == Perk::Comms && !c.dead) return true;
    return false;
}
bool RunState::hasEngineer() const {
    for (const auto& c : crew) if (c.perk == Perk::Engineer && !c.dead) return true;
    return false;
}
bool RunState::hasPhysician() const {
    for (const auto& c : crew) if (c.perk == Perk::Physician && !c.dead) return true;
    return false;
}
bool RunState::hasMechanic() const {
    for (const auto& c : crew) if (c.perk == Perk::Mechanic && !c.dead) return true;
    return false;
}

double RunState::geologistBonus() const {
    for (const auto& c : crew) if (c.perk == Perk::Geologist && !c.dead) return 1.4;
    return 1.0;
}

double RunState::totalPayloadKg(const Balance& b) const {
    double kg = stock.payloadKg();
    kg += (hardware.sparesWheel + hardware.sparesBogie + hardware.sparesSeal) * 24.0;
    (void)b;
    return kg;
}

bool RunState::overPayloadCap(const Balance& b) const {
    return totalPayloadKg(b) > b.payloadCapKg;
}

// ---------------------------------------------------------------- construction

const std::vector<CrewOption>& crewRoster() {
    static const std::vector<CrewOption> roster = {
        {"Okonkwo",  Perk::Physician},
        {"Reyes",    Perk::Geologist},
        {"Lindqvist",Perk::Engineer},
        {"Abadi",    Perk::Mechanic},
        {"Nakamura", Perk::Comms},
        {"Vance",    Perk::None},
        {"Delacroix",Perk::None},
        {"Osei",     Perk::None},
        {"Petrov",   Perk::None},
        {"Ferrante", Perk::None},
    };
    return roster;
}

RunState newRun(uint64_t seed, Profession p, const Balance& b) {
    RunState s;
    s.seed = seed;
    s.profession = p;
    s.credits = professionCredits(p, b);

    // The default manifest is the first five of the roster, which happens to be
    // one of every perk. Players may swap, but the default is a strong one --
    // the difficulty should come from the economy, not from a bad roster draw.
    const auto& roster = crewRoster();
    for (int i = 0; i < 5 && i < static_cast<int>(roster.size()); ++i) {
        CrewMember c;
        c.name = roster[static_cast<size_t>(i)].name;
        c.perk = roster[static_cast<size_t>(i)].perk;
        s.crew.push_back(c);
    }

    s.hardware.tractionUnits = b.tractionUnits;
    s.hardware.bogies = b.bogies;
    s.health = 0.0;
    s.energyKwh = b.batteryCapacityKwh;
    return s;
}

}  // namespace lt
