#include "sim/hazards.h"

#include "sim/health.h"

#include <algorithm>
#include <cmath>

namespace lt {

const char* hazardName(HazardId id) {
    for (const auto& h : hazardTable()) {
        if (h.id == id) return h.name.c_str();
    }
    return "None";
}

const std::vector<Hazard>& hazardTable() {
    static const std::vector<Hazard> t = {
        {HazardId::DustStorm,               "Dust Storm",               3.2, false, false},
        {HazardId::SolarParticleEvent,      "Solar Particle Event",      0.5, false, true},
        {HazardId::MmodImpact,              "Micrometeorite Impact",     2.0, false, false},
        {HazardId::ElectrolysisStackFailure,"Electrolysis Stack Failure",1.6, false, false},
        {HazardId::O2LoopContamination,     "O2 Loop Contamination",     1.8, false, false},
        {HazardId::ThermalCycling,          "Thermal Cycling",           2.2, false, false},

        {HazardId::NavBeaconLost,           "Nav Beacon Lost",           1.4, false, false},
        {HazardId::LostTheTrail,            "Lost The Trail",            1.5, false, false},
        {HazardId::WrongTrail,              "Wrong Trail",               1.0, false, false},
        {HazardId::BoulderField,            "Boulder Field",             2.6, false, false},
        {HazardId::ImpassableSlope,         "Impassable Slope",          1.1, false, false},

        {HazardId::WheelFailure,            "Traction Unit Failure",     4.0, false, false},
        {HazardId::BogieFailure,            "Drive Bogie Failure",       2.2, false, false},
        {HazardId::SealBreach,              "Seal Breach",               1.7, false, false},
        {HazardId::GyroDrift,               "Gyro Drift",                1.3, false, false},
        {HazardId::BatteryRunaway,          "Battery Thermal Runaway",   1.2, false, false},

        {HazardId::CrewMemberLost,          "Crew Member Lost",          0.9, false, false},
        {HazardId::CrewInjury,              "Crew Injury",               2.4, false, false},

        {HazardId::BayFire,                 "Bay Fire",                  1.0, false, false},
        {HazardId::ScavengerRaid,           "Scavenger Raid",            1.3, false, false},
        {HazardId::DerelictCache,           "Derelict Cache",            2.8, false, false},
        {HazardId::PriorWreck,              "Prior Convoy Wreck",        2.0, false, false},
    };
    return t;
}

namespace {

// Context weighting, in the spirit of Heinemann's geography-tied events.
double weightFor(const RunState& s, const Hazard& h) {
    double w = h.baseWeight;

    switch (h.id) {
        case HazardId::SolarParticleEvent:
            // The sun never shines inside a shadowed region, so neither does its
            // particle flux reach you.
            w *= (s.progress.zone == Zone::Psr) ? 0.15 : 1.6;
            break;

        case HazardId::ThermalCycling:
            // Worst at the terminator, where the shadow line sweeps across you.
            w *= (s.progress.zone == Zone::Transition) ? 1.9 : 0.7;
            break;

        case HazardId::ElectrolysisStackFailure:
            // Only matters if you own the stacks, and only breaks where it is
            // being used hard.
            w *= (s.stock.fuelKg > 0.0 || s.stock.o2Kg > 0.0) ? 1.0 : 0.4;
            w *= (s.progress.sortiesRun > 3) ? 1.3 : 0.8;
            break;

        case HazardId::BatteryRunaway:
            // Deep cycles and hard chemical burns cook batteries.
            w *= (s.pace == Pace::FullBurn) ? 2.6 : 0.7;
            w *= (s.energyKwh < 60.0) ? 1.5 : 1.0;
            break;

        case HazardId::DustStorm:
        case HazardId::O2LoopContamination:
            // Regolith gets everywhere, and it is worse where you have been
            // driving hard through it.
            w *= (s.pace == Pace::Sprint) ? 1.4 : 1.0;
            break;

        case HazardId::SealBreach:
            // A chasm crossing is hard on seals.
            w *= s.progress.awaitingChasm ? 2.0 : 1.0;
            break;

        case HazardId::WheelFailure:
            w *= (s.pace == Pace::FullBurn) ? 1.7 : 1.0;
            // Mechanic halves it.
            w *= s.hasMechanic() ? 0.5 : 1.0;
            break;

        case HazardId::BogieFailure:
            w *= s.hasMechanic() ? 0.5 : 1.0;
            break;

        case HazardId::ScavengerRaid:
            // Near a depot, where there are people to raid. Isolated, nobody
            // comes out this far.
            w *= (s.progress.kmTravelled < 30.0) ? 1.7 : 0.6;
            break;

        case HazardId::BoulderField:
        case HazardId::ImpassableSlope:
            // Terrain hazards, scaled by how hard you are pushing.
            w *= (s.pace == Pace::FullBurn) ? 1.5 : 1.0;
            break;

        case HazardId::NavBeaconLost:
        case HazardId::GyroDrift:
            // A working Comms Officer rules these out entirely -- that is what
            // the perk is for.
            w *= s.hasCommsOfficer() ? 0.15 : 1.0;
            break;

        case HazardId::CrewInjury:
            w *= (s.rations == Rations::Emergency) ? 1.8 : 1.0;
            break;

        default:
            break;
    }

    if (h.psrOnly && s.progress.zone != Zone::Psr) w = 0.0;
    if (h.sunlitOnly && s.progress.zone != Zone::Sunlit) w = 0.0;
    return std::max(0.0, w);
}

void lose(RunState& s, const std::string& what, double frac) {
    const double amount = [&] {
        if (what == "water")      return s.stock.waterL * frac;
        if (what == "o2")         return s.stock.o2Kg * frac;
        if (what == "food")       return s.stock.foodKg * frac;
        if (what == "fuel")       return s.stock.fuelKg * frac;
        if (what == "oxidiser")   return s.stock.oxidiserKg * frac;
        if (what == "cargo")      return s.stock.cargoKg * frac;
        return 0.0;
    }();
    if (what == "water")     s.stock.waterL     = std::max(0.0, s.stock.waterL     - amount);
    if (what == "food")      s.stock.foodKg     = std::max(0.0, s.stock.foodKg     - amount);
    if (what == "o2")        s.stock.o2Kg       = std::max(0.0, s.stock.o2Kg       - amount);
    if (what == "fuel")      s.stock.fuelKg     = std::max(0.0, s.stock.fuelKg     - amount);
    if (what == "oxidiser")  s.stock.oxidiserKg = std::max(0.0, s.stock.oxidiserKg - amount);
    if (what == "cargo") {
        s.stock.cargoKg = std::max(0.0, s.stock.cargoKg - amount);
        s.progress.cargoLostKg += amount;
    }
}

}  // namespace

HazardId rollHazard(RunState& s, Rng& rng, const Balance& b) {
    if (!rng.chance(b.baseHazardChance)) return HazardId::None;

    std::vector<double> weights;
    std::vector<HazardId> ids;
    double total = 0.0;

    for (const auto& h : hazardTable()) {
        const double w = weightFor(s, h);
        if (w <= 0.0) continue;
        weights.push_back(w);
        ids.push_back(h.id);
        total += w;
    }
    if (ids.empty()) return HazardId::None;

    double roll = rng.unit() * total;
    for (size_t i = 0; i < ids.size(); ++i) {
        roll -= weights[i];
        if (roll <= 0.0) return ids[i];
    }
    return ids.back();
}

void applyHazard(RunState& s, HazardId id, Rng& rng, const Balance& b,
                 std::vector<std::string>* log) {
    auto say = [log](const std::string& line) { if (log) log->push_back(line); };
    auto killOne = [&s, &say](const std::string& cause) {
        for (auto& c : s.crew) {
            if (!c.dead) {
                c.dead = true;
                c.causeOfDeath = cause;
                say(c.name + " is gone.");
                return;
            }
        }
    };

    switch (id) {
        case HazardId::None:
            break;

        case HazardId::DustStorm:
            say("A dust storm. Visibility drops to nothing and the seals take a "
                "beating.");
            lose(s, "water", rng.range(0.05, 0.18));
            s.stock.suitSets = std::max(0, s.stock.suitSets - 1);
            say("Lost water and a suit set to the grit.");
            break;

        case HazardId::SolarParticleEvent:
            say("Solar particle event. You ride it out inside the cab with the "
                "shields down.");
            // Dose is only survivable because you are indoors; outside it would
            // not be. Costs time, not crew.
            say("The cabin is warm and unpleasant for a sol. Nobody is happy.");
            break;

        case HazardId::MmodImpact:
            say("A micrometeorite punches through the hull. Small hole, poor place "
                "to have one.");
            lose(s, "o2", rng.range(0.15, 0.40));
            s.energyKwh = std::max(0.0, s.energyKwh - rng.range(10.0, 35.0));
            say("The cabin has been repressurised. You are down on oxygen and charge.");
            break;

        case HazardId::ElectrolysisStackFailure:
            say("An electrolysis stack fails. Whatever it was making, it is not "
                "making it now.");
            lose(s, "o2", rng.range(0.08, 0.22));
            break;

        case HazardId::O2LoopContamination:
            say("Regolith dust in the O2 loop. The alarm goes off at three in the "
                "morning.");
            lose(s, "o2", rng.range(0.20, 0.45));
            say("Suit cartridges scrubbed. You have lost a lot of breathing gas.");
            break;

        case HazardId::ThermalCycling:
            say("You drive through the shadow line and the temperature swings forty "
                "degrees in an hour.");
            say("The cab heaters work hard. Charge is down.");
            s.energyKwh = std::max(0.0, s.energyKwh - rng.range(15.0, 40.0));
            break;

        case HazardId::NavBeaconLost:
            say("Nav beacon lost. You dead reckon until it comes back.");
            s.progress.kmTravelled = std::max(0.0, s.progress.kmTravelled - rng.range(0.2, 0.9));
            say("You have lost ground you had already covered.");
            break;

        case HazardId::LostTheTrail:
            say("You have lost the trail.");
            s.progress.kmTravelled = std::max(0.0, s.progress.kmTravelled - rng.range(0.5, 2.0));
            say("A sol and a half of driving, gone.");
            break;

        case HazardId::WrongTrail:
            say("Wrong trail. The survey stakes are from a different season's "
                "baseline.");
            s.progress.kmTravelled = std::max(0.0, s.progress.kmTravelled - rng.range(1.5, 3.5));
            s.energyKwh = std::max(0.0, s.energyKwh - rng.range(5.0, 15.0));
            break;

        case HazardId::BoulderField:
            say("Boulder field. You pick your way through it for most of a sol.");
            s.energyKwh = std::max(0.0, s.energyKwh - rng.range(12.0, 30.0));
            if (rng.chance(0.25)) {
                say("A traction unit is down to a slow rotation. It will still turn.");
                if (s.hardware.tractionSick < s.hardware.tractionUnits) {
                    ++s.hardware.tractionSick;
                }
            }
            break;

        case HazardId::ImpassableSlope:
            // The hard stop. You do not pass until you get off the slope, and
            // getting off costs sols, charge, and possibly cargo.
            say("The slope ahead is not passable. You are not going up it today, or "
                "tomorrow, or with this load.");
            s.progress.sol += 2;
            s.energyKwh = std::max(0.0, s.energyKwh - rng.range(25.0, 55.0));
            lose(s, "cargo", rng.range(0.04, 0.12));
            say("Two sols off-rig, unloading by hand. You got the rig around.");
            break;

        case HazardId::WheelFailure: {
            say("A traction unit has failed.");
            // The original's three-way branch, verbatim:
            //   1. carry a spare  -> replace it, instantly
            //   2. no spare, can repair -> repair, costs a day
            //   3. neither -> hard stop
            if (s.hardware.sparesWheel > 0) {
                --s.hardware.sparesWheel;
                say("Replaced it from spares. Out of the dark, back on the road.");
            } else if (s.hasEngineer() || rng.chance(0.45)) {
                say("You strip it, patch the actuator, and fit it back. That is a "
                    "day's work.");
                s.progress.sol += 1;
                s.energyKwh = std::max(0.0, s.energyKwh - rng.range(8.0, 20.0));
            } else {
                say("No spare and no way to fix it here. That unit is finished until "
                    "you fit a replacement.");
                s.hardware.tractionDead = std::min(s.hardware.tractionDead + 1,
                                                   s.hardware.tractionUnits);
                s.hardware.needsWheelRepair = true;
                if (s.hardware.immobile()) {
                    say("That was the last one that would turn. The rig is not going "
                        "anywhere.");
                } else {
                    say("The convoy limps on the units it has left. It will be slower "
                        "until you find a spare.");
                }
            }
            break;
        }

        case HazardId::BogieFailure:
            say("A drive bogie has failed. Everything is twice as hard now.");
            if (s.hardware.sparesBogie > 0) {
                --s.hardware.sparesBogie;
                say("Replaced from spares. The drive is even again.");
            } else if (s.hasEngineer()) {
                say("Stripped, reground, refitted. The Engineer does not enjoy it.");
                s.progress.sol += 2;
            } else {
                say("No spare, and it is not a repair you can make in the field. "
                    "That bogie is done until someone grinds it out.");
                s.hardware.bogies = std::max(0, s.hardware.bogies - 1);
                s.hardware.needsBogieRepair = true;
                if (s.hardware.bogies <= 0) {
                    say("That was the last one. There is no drive left.");
                }
            }
            break;

        case HazardId::SealBreach:
            say("A seal on a suit locker has gone. Pressure is bleeding out of the "
                "cargo bay.");
            lose(s, "water", rng.range(0.03, 0.10));
            if (s.hardware.sparesSeal > 0) {
                --s.hardware.sparesSeal;
                say("You fit a spare seal kit and stop the leak.");
            } else {
                say("No kit. You patch it with suit material and it will not hold "
                    "again.");
                s.energyKwh = std::max(0.0, s.energyKwh - rng.range(6.0, 18.0));
            }
            break;

        case HazardId::GyroDrift:
            say("Gyro drift. Your position estimate is now a guess with a number "
                "attached.");
            say("You spend a sol on a reference fix.");
            s.progress.sol += 1;
            break;

        case HazardId::BatteryRunaway:
            say("A battery cell has gone into thermal runaway.");
            s.energyKwh = std::max(0.0, s.energyKwh - rng.range(40.0, 110.0));
            lose(s, "water", rng.range(0.04, 0.12));  // coolant loss
            say("You vent the stack. You are down a great deal of charge and the "
                "cab is very warm.");
            break;

        case HazardId::CrewMemberLost:
            say("One crew member is not answering. A sol of searching turns up "
                "nothing.");
            killOne("lost off the route");
            break;

        case HazardId::CrewInjury:
            say("Someone has hurt themselves badly.");
            if (s.hasPhysician()) {
                say("The Physician stabilises them. It should not have been close.");
                break;
            }
            if (rng.chance(0.25)) {
                killOne("an injury aboard");
            } else {
                for (auto& c : s.crew) {
                    if (!c.dead && c.malady == Malady::None) {
                        c.malady = rng.chance(0.5) ? Malady::Fracture : Malady::Crush;
                        c.recoverSol = maladyRecoverySols(c.malady, b);
                        say(c.name + " has a " + maladyName(c.malady) + ".");
                        break;
                    }
                }
            }
            break;

        case HazardId::BayFire:
            say("Fire in the pressurized bay.");
            lose(s, "food", rng.range(0.20, 0.55));
            s.stock.suitSets = std::max(0, s.stock.suitSets - rng.rangeInt(1, 4));
            s.energyKwh = std::max(0.0, s.energyKwh - rng.range(10.0, 40.0));
            say("You get it out. Not everything comes back.");
            break;

        case HazardId::ScavengerRaid:
            say("Scavengers came up in the dark and helped themselves.");
            lose(s, "o2", rng.range(0.10, 0.30));
            lose(s, "fuel", rng.range(0.05, 0.20));
            if (s.hardware.sparesWheel > 0 && rng.chance(0.35)) {
                --s.hardware.sparesWheel;
                say("They took a spare traction unit, at least. You noticed.");
            }
            break;

        case HazardId::DerelictCache: {
            // The positive counterpart to the original's abandoned wagon. Worth
            // about what hunting would yield, which is exactly the intent.
            const double water = rng.range(30.0, 70.0);
            s.stock.waterL += water;
            s.stock.o2Kg += water * rng.range(0.05, 0.12);
            if (rng.chance(0.4)) s.stock.cargoKg += rng.range(4.0, 14.0);
            if (rng.chance(0.25)) ++s.hardware.sparesWheel;
            char cacheLine[128];
            std::snprintf(cacheLine, sizeof(cacheLine),
                          "An unlogged cache, sealed and intact. %.0f litres of water, "
                          "and some of it is yours now.", water);
            say(cacheLine);
            break;
        }

        case HazardId::PriorWreck:
            // The tombstone mechanic. Somebody got this far and stopped.
            say("You pass the wreck of an earlier convoy. It is not very old.");
            say("You record what you can. There is not much to record.");
            if (rng.chance(0.30)) {
                s.stock.suitSets += rng.rangeInt(1, 3);
                say("They left suits. You take them.");
            }
            break;

        default:
            break;
    }
}

bool rollMercyValve(RunState& s, Rng& rng, const Balance& b, std::string* outMessage) {
    // Faithful to the original: this fires ONLY when a resource is at zero, and
    // only between landmarks while travelling. It is the one thing standing
    // between a bad run and a dead one.
    const bool starving = (s.stock.foodKg <= 0.0) || (s.stock.waterL <= 0.0) ||
                          (s.stock.o2Kg <= 0.0) || (s.energyKwh <= 0.0);
    if (!starving) return false;

    // Roughly one in three. Generous, exactly as generous as the original was.
    if (!rng.chance(0.34)) return false;

    const auto& roster = crewRoster();
    const char* who = roster[rng.below(static_cast<uint32_t>(roster.size()))].name;

    if (s.stock.foodKg <= 0.0 || s.stock.waterL <= 0.0) {
        const double water = rng.range(40.0, 90.0);
        s.stock.waterL += water;
        s.stock.foodKg += water * rng.range(0.10, 0.22);
        if (outMessage != nullptr) {
            *outMessage = std::string(who) +
                          " flagged a salvage cache forty kilometres back along the "
                          "route. There is still sealed water in it.";
        }
    } else if (s.stock.o2Kg <= 0.0) {
        s.stock.o2Kg += rng.range(6.0, 14.0);
        if (outMessage != nullptr) {
            *outMessage = std::string(who) +
                          " came back on the emergency channel with a filled cylinder "
                          "and no explanation of where it came from.";
        }
    } else {
        s.energyKwh += rng.range(40.0, 90.0);
        if (outMessage != nullptr) {
            *outMessage = std::string(who) +
                          " walked the array down the flank and left a charge cable on "
                          "the manifest. Do not ask whose it was.";
        }
    }
    (void)b;
    return true;
}

void applyRestingRecovery(RunState& s, const Balance& b, int sols,
                          std::vector<std::string>* log) {
    for (int i = 0; i < sols; ++i) {
        s.progress.sol += 1;
        // Resting is the only reliable healing in the game, because hazards do
        // not fire while stationary. That is the original's design and it is
        // preserved here deliberately.
        s.health = std::max(0.0, s.health * b.healthRecoveryFraction);
        const HealthMods m = baseDailyMods(s, b);
        s.health += m.total();
        s.health = std::min(s.health, b.healthMax);
        tickRecovery(s, b, log);
    }
}

}  // namespace lt
