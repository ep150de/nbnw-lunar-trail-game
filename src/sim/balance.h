// Every tunable number in the game, in one struct.
//
// This is the single source of truth for the difficulty curve. `defaults()`
// returns the shipped values; `Balance::load()` overlays content/balance.json,
// so tuning the game does not require touching code.
//
// Design rule, inherited from the original: money is the only non-renewable
// resource, prices inflate at every outpost, and daily consumption is set
// deliberately ABOVE what credits can buy. That is what forces the player into
// prospecting the way the original forced a Farmer into hunting. Do not
// "balance" consumption down toward purchasable levels -- that removes the
// game. See docs/DESIGN.md section 1.
#pragma once

#include <string>

namespace lt {

struct Balance {
    // ---- convoy physics ----
    double dryMassKg = 2650.0;      // crew cab + chassis + wheels + batteries + RCS
    double payloadCapKg = 2100.0;   // regolith bearing limit; a real physical cap
    int tractionUnits = 6;          // wheels; below this you lose speed
    int bogies = 3;                 // drive bogies; failure halves speed
    double kgPerBogieAxe = 3.0;     // spare capacity hint for the shop UI

    // ---- daily consumption, per crew member, per sol ----
    double foodPerCrewKg = 1.4;
    double waterPerCrewKg = 3.5;
    double o2PerCrewKg = 0.55;
    double energyPerCrewKwh = 19.0;  // thermal + avionics + comms + hygiene
    double suitSetsPerCrewPerSol = 0.03;

    // ---- pace ----
    // Distance is baseKmPerSol * paceFactor * terrain * traction * health * slope.
    double baseKmPerSol = 3.0;
    double paceCruise = 1.0;       // electric, sustainable
    double paceSprint = 2.0;       // electric, heavy draw, wears gear
    double paceFullBurn = 4.667;   // chemical; 14 km/sol, burns propellant fast

    // ---- energy ----
    double batteryCapacityKwh = 900.0;
    double driveEnergyCruiseKwh = 28.0;
    double driveEnergySprintKwh = 52.0;
    double driveEnergyBurnKwh = 85.0;   // plus propellant pumping

    // ---- propellant ----
    double kgPropellantPerKm = 3.0;  // at Full Burn
    double oxidiserToFuelRatio = 6.0; // LCH4 : LOX by mass
    double kgIcePerKgPropellant = 8.0;

    // ---- ice prospecting (the hunting minigame) ----
    double sortieHaulCapKg = 90.0;   // the 100 lb cap, preserved in spirit
    double sortieYieldPerCrewKg = 22.0;
    int sortieSols = 1;

    // ---- health (Oregon Trail's model, verbatim) ----
    double healthMax = 140.0;
    double healthRecoveryFraction = 0.90;  // applied BEFORE modifiers; order matters
    double illnessChanceMax = 0.40;        // at worst health
    int ailmentRecoverySols = 10;
    int injuryRecoverySols = 30;

    // ---- pace / ration multipliers ----
    double paceHealthSteady = 0.0;
    double paceHealthStrenuous = 2.5;
    double paceHealthGrueling = 7.0;
    double rationHealthFull = 0.0;
    double rationHealthReduced = 3.0;
    double rationHealthEmergency = 11.0;

    // ---- terrain zones ----
    double zoneTerrainSunlit = 1.00;
    double zoneTerrainTransition = 0.85;
    double zoneTerrainPsr = 0.75;
    double zoneSolarSunlitKwh = 220.0;
    double zoneSolarTransitionKwh = 130.0;
    double zoneSolarPsrKwh = 0.0;
    // Ice availability per zone, kg per sortie per crew.
    double zoneIceSunlit = 0.0;
    double zoneIceTransition = 16.0;
    double zoneIcePsr = 52.0;

    // ---- journey ----
    double trailTotalKm = 120.0;
    int hardSolLimit = 110;        // the shadow line; crew rotation closes
    int typicalSols = 72;
    int minimumSols = 45;

    // ---- economy ----
    double outpostMarkupPerIndex = 0.25;   // +25% per outpost, exactly like the forts
    // All prices are multiples of 5 credits, because 5 credits == 1 point and
    // the economy is only balanced if that holds exactly for every good.
    double basePriceWaterL = 1.2;          //  0.24 points / L
    double basePriceO2Kg = 12.0;           //  2.4 points / kg
    double basePriceFuelKg = 10.0;         //  2.0 points / kg
    double basePriceOxidiserKg = 5.0;      //  1.0 points / kg
    double basePriceFoodKg = 3.0;          //  0.6 points / kg
    double basePriceSuitSet = 60.0;        // 12.0 points / set
    double basePriceCuttingCharge = 25.0;  //  5.0 points / charge
    double basePriceWheel = 300.0;         // 60.0 points
    double basePriceBogie = 600.0;         // 120.0 points
    double basePriceSealKit = 240.0;       // 48.0 points
    double basePriceCargoKg = 0.5;         //  0.1 points / kg  (1 per 10 kg)

    // A manned relay depot has its own array. Recharging is free -- the cost is
    // the sols you spend parked while it happens, which is the only currency
    // that matters near the shadow line.
    double depotRechargeKwhPerSol = 450.0;
    int depotRechargeSols = 2;

    int maxSparesPerType = 3;   // the wheel/axle/tongue limit, preserved
    int maxCuttingCharges = 200;
    int maxSuitSets = 60;

    // ---- professions (3 difficulty tiers, exactly as the original) ----
    double creditsFlightDirector = 16000.0;
    double creditsMissionSpecialist = 8800.0;
    double creditsPayloadTechnician = 5800.0;
    double scoreMultFlightDirector = 1.0;
    double scoreMultMissionSpecialist = 2.0;
    double scoreMultPayloadTechnician = 3.0;

    // ---- scoring ----
    // The original balanced its whole economy so that 1 point == $5. Preserved:
    // 5 credits converts to 1 point whether spent or held.
    double pointsPerCredit = 0.2;
    double pointsPerCargoKg = 0.1;
    double pointsPerSurvivorGood = 500.0;
    double pointsPerSurvivorFair = 400.0;
    double pointsPerSurvivorPoor = 300.0;
    double pointsPerSurvivorVeryPoor = 200.0;
    // Spares and suit sets are scored from their prices like every other good,
    // in goodPoints(). They used to carry separate constants (50 and 8) that no
    // longer matched the prices they were supposed to describe, so a spare was
    // worth 50 points against the 60 its price implied.
    // Points for surviving crew, by arrival health band. The original used
    // 500/400/300/200 for good/fair/poor/very poor; the band names here are
    // Nominal/Degraded/Stressed/Critical.
    double pointsPerChasm = 0.0;

    // ---- Chapter 1: ascent ----
    double leoWetMassKg = 28000.0;
    int tliWindowOpenSol = 3;
    int tliWindowCloseSol = 5;
    double tliCostPerKg = 4.2;
    double tliWindowEscalation = 1.6;   // price multiplier on the final window
    double depotPricePerKgFuel = 3.0;
    double depotPricePerKgOxidiser = 2.4;
    double lloiCostPerKg = 2.1;
    // Powered descent, the interactive minigame.
    double descentDryMassKg = 1000.0;
    double descentPropellantKg = 1600.0;
    double descentThrustN = 45000.0;
    double descentIspS = 311.0;
    double descentStartAltitudeM = 15000.0;
    double descentStartVsMps = -1700.0;   // orbital velocity, minus for capture
    double descentStartHsMps = 40.0;
    double descentMaxTiltDeg = 20.0;
    double descentSafeVsMps = 3.0;
    double descentSafeHsMps = 2.0;
    double descentMaxSlopeDeg = 15.0;
    double lunarGM3 = 4902.8;            // km^3/s^2
    double earthGM = 398600.4;

    // ---- hazards ----
    double baseHazardChance = 0.13;      // per sol of travel while moving

    static Balance defaults() { return Balance{}; }

    // Overlays values from content/balance.json. Unknown keys are ignored and
    // missing keys keep their default, so the file can be partial.
    bool load(const std::string& path);
};

}  // namespace lt
