#include "sim/daystep.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "sim/hazards.h"
#include "sim/health.h"
#include "sim/trail.h"

namespace lt {

namespace {

double driveEnergyFor(Pace p, const Balance& b) {
    switch (p) {
        case Pace::Cruise:   return b.driveEnergyCruiseKwh;
        case Pace::Sprint:   return b.driveEnergySprintKwh;
        case Pace::FullBurn: return b.driveEnergyBurnKwh;
    }
    return b.driveEnergyCruiseKwh;
}

}  // namespace

double projectedKmCapability(const RunState& s, const Balance& b) {
    if (s.progress.awaitingChasm) return 0.0;

    const double paceF = paceFactor(s.pace, b);
    const double zoneF = zoneTerrainFactor(s.progress.zone, b);
    const double tract = s.hardware.effectiveTraction(b.tractionUnits);

    // Bogies: each failed one roughly halves the drive. The original's oxen
    // rule ("a sick ox counts as half") is reused here.
    double bogieF = 1.0;
    const int healthyBogies = s.hardware.bogies - s.hardware.bogiesSick;
    const double bogieEff = healthyBogies + 0.5 * s.hardware.bogiesSick;
    bogieF = std::min(1.0, bogieEff / static_cast<double>(std::max(1, b.bogies)));

    // Maladies cost 10% speed each, as documented for sick party members.
    const double healthF = std::max(0.1, 1.0 - 0.10 * static_cast<double>(s.maladiedCrew()));

    // An impassable slope or a dead battery stops you dead.
    if (s.energyKwh <= 0.0 && s.progress.zone == Zone::Psr) return 0.0;

    // Full Burn cannot happen without both propellants.
    if (s.pace == Pace::FullBurn && !s.hasProportions()) return 0.0;

    return std::max(0.0, b.baseKmPerSol * paceF * zoneF * tract * bogieF * healthF);
}

double projectedKmToday(const RunState& s, const Balance& b) {
    double km = projectedKmCapability(s, b);
    if (km <= 0.0) return 0.0;

    // Never travel past the next landmark. The original snapped arrival to the
    // landmark and stopped there, so a fast pace into a nearby landmark
    // legitimately yields less than a nominal full day. This is real behaviour,
    // not an artefact, and the trail screen shows the clamped figure because it
    // is the honest answer: that is how far you go today.
    const double remaining = kmToNextLandmark(s.progress.kmTravelled);
    if (remaining > 0.0) km = std::min(km, remaining);

    return std::max(0.0, km);
}

std::string projectedKmReason(const RunState& s, const Balance& b) {
    std::string why;
    if (s.progress.awaitingChasm) why = "blocked at a chasma";
    else if (s.energyKwh <= 0.0 && s.progress.zone == Zone::Psr) why = "no power in the dark";
    else if (s.pace == Pace::FullBurn && !s.hasProportions()) why = "no usable propellant";
    else if (s.hardware.tractionUnits == 0) why = "no traction";
    else if (s.pace == Pace::FullBurn) why = "burning " + std::to_string(static_cast<int>(s.burnRangeKm(b))) + " km of propellant";
    else if (s.progress.zone == Zone::Psr) why = "no solar here";
    else why = "clear";
    return why;
}

DayReport advanceSol(RunState& s, Rng& rng, const Balance& b, bool traveling) {
    DayReport r;
    r.sol = s.progress.sol;
    r.kmStart = s.progress.kmTravelled;
    r.healthBefore = s.health;

    const int crew = s.livingCrew();

    // ---- 1 & 2. environment: solar collection and battery state ----
    //
    // Solar is only collected in zones that have sun. The sunlit rim produces
    // more than a cruising convoy needs, so a convoy there slowly tops up; the
    // transition zone is roughly break-even; the shadowed regions produce
    // exactly nothing and the convoy is on the battery alone.
    const double solar = zoneSolarKwh(s.progress.zone, b);
    r.solarKwhGained = solar;
    if (solar > 0.0) {
        s.energyKwh = std::min(b.batteryCapacityKwh, s.energyKwh + solar);
    }

    const double lifeSupportKwh = b.energyPerCrewKwh * static_cast<double>(crew);
    const double driveKwh = traveling ? driveEnergyFor(s.pace, b) : 0.0;
    r.energyKwhUsed = lifeSupportKwh + driveKwh;

    if (s.energyKwh >= r.energyKwhUsed) {
        s.energyKwh -= r.energyKwhUsed;
    } else {
        // Not enough charge. Life support is prioritised and the drive stops.
        // This is a survivable state, not a death sentence: a convoy that
        // coasts in the dark is slower but still moving, and a depot ahead will
        // recharge it. It only becomes fatal if the convoy also has nowhere to
        // go and nothing left to spend.
        const double deficit = r.energyKwhUsed - s.energyKwh;
        s.energyKwh = 0.0;
        r.powerStarved = true;
        if (traveling && driveKwh > 0.0) {
            r.log.push_back("Power short. The drive is shut down and you coast.");
            traveling = false;
        }
        r.log.push_back("Batteries are at reserve. Life support is on emergency.");
        (void)deficit;
    }
    r.energyKwhEnd = s.energyKwh;

    // ---- 3. health modifiers and illness roll, before consumption ----
    HealthMods mods = baseDailyMods(s, b);

    // ---- propellant burn, before distance, since distance depends on pace ----
    double kmThisSol = 0.0;
    if (traveling) kmThisSol = projectedKmToday(s, b);

    if (traveling && s.pace == Pace::FullBurn && kmThisSol > 0.0 && s.hasProportions()) {
        const double needed = kmThisSol * s.propellantPerKm(b);
        // Burn proportionally so the mixture stays consistent, and clamp if the
        // tanks run dry mid-burn.
        const double available = s.stock.propellantKg();
        const double burn = std::min(needed, available);
        const double fuelFrac = s.stock.fuelKg / std::max(1e-9, available);
        s.stock.fuelKg     = std::max(0.0, s.stock.fuelKg     - burn * fuelFrac);
        s.stock.oxidiserKg = std::max(0.0, s.stock.oxidiserKg - burn * (1.0 - fuelFrac));
        s.progress.propellantBurnedKg += burn;
        r.propellantUsedKg = burn;
        if (burn < needed - 0.01) {
            r.log.push_back("Propellant ran out partway through the burn.");
        }
    }

    if (s.stock.oxidiserKg <= 0.0 && s.stock.fuelKg > 1.0) {
        r.log.push_back("You are burning fuel with no oxidiser. Nothing will light.");
    }

    // ---- 4. consumptions ----
    const double rmult = rationsMultiplier(s.rations);
    r.foodUsedKg  = b.foodPerCrewKg  * static_cast<double>(crew) * rmult;
    r.waterUsedL = b.waterPerCrewKg * static_cast<double>(crew) * rmult;
    r.o2UsedKg   = b.o2PerCrewKg   * static_cast<double>(crew);

    s.stock.foodKg  = std::max(0.0, s.stock.foodKg  - r.foodUsedKg);
    s.stock.waterL  = std::max(0.0, s.stock.waterL  - r.waterUsedL);
    s.stock.o2Kg    = std::max(0.0, s.stock.o2Kg    - r.o2UsedKg);

    // MMOD suits wear out. Never consumed fast, never decisive on its own --
    // same as clothing in the original, which functioned as a cold threshold
    // and a trade good rather than a consumable.
    const double suitWear = b.suitSetsPerCrewPerSol * static_cast<double>(crew);
    if (rng.chance(suitWear)) s.stock.suitSets = std::max(0, s.stock.suitSets - 1);

    // ---- illness roll, still before events, as in the original ----
    std::string maladyReport;
    rollMalady(s, rng, b, &maladyReport);
    if (!maladyReport.empty()) r.log.push_back(maladyReport);

    // ---- 5. random events ----
    if (traveling) {
        const HazardId hid = rollHazard(s, rng, b);
        if (hid != HazardId::None) {
            applyHazard(s, hid, rng, b, &r.log);
            // An event that consumed sols (repairs, impassable slope) means we
            // do not also bank the distance for this sol.
            if (s.progress.sol > r.sol) kmThisSol = 0.0;
        }

        std::string mercy;
        if (rollMercyValve(s, rng, b, &mercy)) r.log.push_back(mercy);
    }

    // ---- apply health, in the original's order ----
    applyDailyHealth(s, mods, b);
    r.healthAfter = s.health;

    // ---- 6. distance ----
    r.kmGained = kmThisSol;
    s.progress.kmTravelled += kmThisSol;
    r.kmEnd = s.progress.kmTravelled;
    r.stalled = kmThisSol <= 0.001;

    // ---- 7. calendar ----
    tickRecovery(s, b, &r.log);
    s.progress.sol += 1;

    // Zone follows the segment you are in.
    s.progress.zone = zoneAtKm(s.progress.kmTravelled);

    // ---- landmark arrival ----
    const auto& t = trail();
    const size_t idx = std::min<size_t>(static_cast<size_t>(s.progress.landmarkIndex),
                                        t.size() - 1);
    const Landmark& next = t[idx];

    if (s.progress.kmTravelled >= next.km - 0.001) {
        if (next.kind == LandmarkKind::Chasm) {
            // Stop at the lip. The crossing is a separate decision, so the
            // landmark is NOT consumed here and awaitingChasm is latched for the
            // caller to act on.
            r.atChasm = true;
            r.chasmIndex = next.index;
            if (!s.progress.awaitingChasm) {
                s.progress.awaitingChasm = true;
                s.progress.chasmIndex = next.index;
                r.log.push_back("You are at the lip of " + next.name + ".");
            }
        } else if (s.progress.landmarkIndex < static_cast<int>(t.size())) {
            r.reachedLandmark = true;
            r.landmarkIndex = s.progress.landmarkIndex;
            r.landmarkName = next.name.c_str();
            s.progress.landmarkIndex += 1;
            s.progress.landmarksReached += 1;
        }
    }

    checkLossConditions(s, b);
    return r;
}

std::vector<std::string> routeForecast(const RunState& s, uint64_t seed, const Balance& b,
                                       int sols) {
    // Simulates a shadow convoy forward on a copy, using an independent seed
    // stream so that looking does not disturb the actual run. This is a preview
    // of a fixed route, not a prophecy: your own choices still decide what
    // happens.
    std::vector<std::string> out;

    RunState ghost = s;
    ghost.crew.clear();
    CrewMember c;
    c.name = "forecast";
    ghost.crew.push_back(c);

    Rng rng;
    rng.seed(deriveSeed(seed, 0x5EEDull));

    for (int i = 0; i < sols; ++i) {
        const HazardId hid = rollHazard(ghost, rng, b);
        if (hid == HazardId::None) continue;
        for (const auto& h : hazardTable()) {
            if (h.id != hid) continue;
            char buf[64];
            std::snprintf(buf, sizeof(buf), "sol %d: %s", i + 1, h.name.c_str());
            out.emplace_back(buf);
            break;
        }
    }

    if (out.empty()) out.emplace_back("sol 1-8: nothing predicted");
    return out;
}

}  // namespace lt
