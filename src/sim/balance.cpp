#include "sim/balance.h"

#include "core/json.h"

namespace lt {

namespace {

// Applies every numeric key present in `j` onto `b`. Kept flat and explicit so
// that a typo in balance.json is a compile-time-visible no-op rather than a
// silent behaviour change.
void overlay(Balance& b, const Json& j) {
    if (j.isNull()) return;

    b.dryMassKg            = j.num("dryMassKg", b.dryMassKg);
    b.payloadCapKg         = j.num("payloadCapKg", b.payloadCapKg);
    b.tractionUnits        = j.integer("tractionUnits", b.tractionUnits);
    b.bogies               = j.integer("bogies", b.bogies);

    b.foodPerCrewKg        = j.num("foodPerCrewKg", b.foodPerCrewKg);
    b.waterPerCrewKg       = j.num("waterPerCrewKg", b.waterPerCrewKg);
    b.o2PerCrewKg          = j.num("o2PerCrewKg", b.o2PerCrewKg);
    b.energyPerCrewKwh     = j.num("energyPerCrewKwh", b.energyPerCrewKwh);
    b.suitSetsPerCrewPerSol= j.num("suitSetsPerCrewPerSol", b.suitSetsPerCrewPerSol);

    b.baseKmPerSol         = j.num("baseKmPerSol", b.baseKmPerSol);
    b.paceCruise           = j.num("paceCruise", b.paceCruise);
    b.paceSprint           = j.num("paceSprint", b.paceSprint);
    b.paceFullBurn         = j.num("paceFullBurn", b.paceFullBurn);

    b.batteryCapacityKwh   = j.num("batteryCapacityKwh", b.batteryCapacityKwh);
    b.driveEnergyCruiseKwh = j.num("driveEnergyCruiseKwh", b.driveEnergyCruiseKwh);
    b.driveEnergySprintKwh = j.num("driveEnergySprintKwh", b.driveEnergySprintKwh);
    b.driveEnergyBurnKwh   = j.num("driveEnergyBurnKwh", b.driveEnergyBurnKwh);

    b.kgPropellantPerKm    = j.num("kgPropellantPerKm", b.kgPropellantPerKm);
    b.oxidiserToFuelRatio  = j.num("oxidiserToFuelRatio", b.oxidiserToFuelRatio);
    b.kgIcePerKgPropellant = j.num("kgIcePerKgPropellant", b.kgIcePerKgPropellant);

    b.sortieHaulCapKg      = j.num("sortieHaulCapKg", b.sortieHaulCapKg);
    b.sortieYieldPerCrewKg = j.num("sortieYieldPerCrewKg", b.sortieYieldPerCrewKg);
    b.sortieSols           = j.integer("sortieSols", b.sortieSols);

    b.healthMax            = j.num("healthMax", b.healthMax);
    b.healthRecoveryFraction = j.num("healthRecoveryFraction", b.healthRecoveryFraction);
    b.illnessChanceMax     = j.num("illnessChanceMax", b.illnessChanceMax);
    b.ailmentRecoverySols  = j.integer("ailmentRecoverySols", b.ailmentRecoverySols);
    b.injuryRecoverySols   = j.integer("injuryRecoverySols", b.injuryRecoverySols);

    b.paceHealthSteady     = j.num("paceHealthSteady", b.paceHealthSteady);
    b.paceHealthStrenuous  = j.num("paceHealthStrenuous", b.paceHealthStrenuous);
    b.paceHealthGrueling   = j.num("paceHealthGrueling", b.paceHealthGrueling);
    b.rationHealthFull     = j.num("rationHealthFull", b.rationHealthFull);
    b.rationHealthReduced  = j.num("rationHealthReduced", b.rationHealthReduced);
    b.rationHealthEmergency= j.num("rationHealthEmergency", b.rationHealthEmergency);

    b.zoneTerrainSunlit      = j.num("zoneTerrainSunlit", b.zoneTerrainSunlit);
    b.zoneTerrainTransition = j.num("zoneTerrainTransition", b.zoneTerrainTransition);
    b.zoneTerrainPsr        = j.num("zoneTerrainPsr", b.zoneTerrainPsr);
    b.zoneSolarSunlitKwh    = j.num("zoneSolarSunlitKwh", b.zoneSolarSunlitKwh);
    b.zoneSolarTransitionKwh= j.num("zoneSolarTransitionKwh", b.zoneSolarTransitionKwh);
    b.zoneSolarPsrKwh       = j.num("zoneSolarPsrKwh", b.zoneSolarPsrKwh);
    b.zoneIceSunlit         = j.num("zoneIceSunlit", b.zoneIceSunlit);
    b.zoneIceTransition     = j.num("zoneIceTransition", b.zoneIceTransition);
    b.zoneIcePsr            = j.num("zoneIcePsr", b.zoneIcePsr);

    b.trailTotalKm          = j.num("trailTotalKm", b.trailTotalKm);
    b.hardSolLimit          = j.integer("hardSolLimit", b.hardSolLimit);
    b.typicalSols           = j.integer("typicalSols", b.typicalSols);
    b.minimumSols           = j.integer("minimumSols", b.minimumSols);

    b.outpostMarkupPerIndex = j.num("outpostMarkupPerIndex", b.outpostMarkupPerIndex);
    b.basePriceWaterL       = j.num("basePriceWaterL", b.basePriceWaterL);
    b.basePriceO2Kg         = j.num("basePriceO2Kg", b.basePriceO2Kg);
    b.basePriceFuelKg       = j.num("basePriceFuelKg", b.basePriceFuelKg);
    b.basePriceOxidiserKg   = j.num("basePriceOxidiserKg", b.basePriceOxidiserKg);
    b.basePriceFoodKg       = j.num("basePriceFoodKg", b.basePriceFoodKg);
    b.basePriceSuitSet      = j.num("basePriceSuitSet", b.basePriceSuitSet);
    b.basePriceCuttingCharge= j.num("basePriceCuttingCharge", b.basePriceCuttingCharge);
    b.basePriceWheel        = j.num("basePriceWheel", b.basePriceWheel);
    b.basePriceBogie        = j.num("basePriceBogie", b.basePriceBogie);
    b.basePriceSealKit      = j.num("basePriceSealKit", b.basePriceSealKit);
    b.basePriceCargoKg      = j.num("basePriceCargoKg", b.basePriceCargoKg);

    b.depotRechargeKwhPerSol = j.num("depotRechargeKwhPerSol", b.depotRechargeKwhPerSol);
    b.depotRechargeSols     = j.integer("depotRechargeSols", b.depotRechargeSols);
    b.maxSparesPerType      = j.integer("maxSparesPerType", b.maxSparesPerType);
    b.maxCuttingCharges     = j.integer("maxCuttingCharges", b.maxCuttingCharges);
    b.maxSuitSets           = j.integer("maxSuitSets", b.maxSuitSets);

    b.creditsFlightDirector      = j.num("creditsFlightDirector", b.creditsFlightDirector);
    b.creditsMissionSpecialist   = j.num("creditsMissionSpecialist", b.creditsMissionSpecialist);
    b.creditsPayloadTechnician   = j.num("creditsPayloadTechnician", b.creditsPayloadTechnician);
    b.scoreMultFlightDirector    = j.num("scoreMultFlightDirector", b.scoreMultFlightDirector);
    b.scoreMultMissionSpecialist = j.num("scoreMultMissionSpecialist", b.scoreMultMissionSpecialist);
    b.scoreMultPayloadTechnician = j.num("scoreMultPayloadTechnician", b.scoreMultPayloadTechnician);

    b.pointsPerCredit        = j.num("pointsPerCredit", b.pointsPerCredit);
    b.pointsPerCargoKg       = j.num("pointsPerCargoKg", b.pointsPerCargoKg);
    b.pointsPerSurvivorGood  = j.num("pointsPerSurvivorGood", b.pointsPerSurvivorGood);
    b.pointsPerSurvivorFair  = j.num("pointsPerSurvivorFair", b.pointsPerSurvivorFair);
    b.pointsPerSurvivorPoor  = j.num("pointsPerSurvivorPoor", b.pointsPerSurvivorPoor);
    b.pointsPerSurvivorVeryPoor = j.num("pointsPerSurvivorVeryPoor", b.pointsPerSurvivorVeryPoor);

    b.leoWetMassKg           = j.num("leoWetMassKg", b.leoWetMassKg);
    b.tliWindowOpenSol       = j.integer("tliWindowOpenSol", b.tliWindowOpenSol);
    b.tliWindowCloseSol      = j.integer("tliWindowCloseSol", b.tliWindowCloseSol);
    b.tliCostPerKg           = j.num("tliCostPerKg", b.tliCostPerKg);
    b.tliWindowEscalation    = j.num("tliWindowEscalation", b.tliWindowEscalation);
    b.depotPricePerKgFuel    = j.num("depotPricePerKgFuel", b.depotPricePerKgFuel);
    b.depotPricePerKgOxidiser= j.num("depotPricePerKgOxidiser", b.depotPricePerKgOxidiser);
    b.lloiCostPerKg          = j.num("lloiCostPerKg", b.lloiCostPerKg);
    b.descentDryMassKg       = j.num("descentDryMassKg", b.descentDryMassKg);
    b.descentPropellantKg    = j.num("descentPropellantKg", b.descentPropellantKg);
    b.descentThrustN         = j.num("descentThrustN", b.descentThrustN);
    b.descentIspS            = j.num("descentIspS", b.descentIspS);
    b.descentStartAltitudeM  = j.num("descentStartAltitudeM", b.descentStartAltitudeM);
    b.descentStartVsMps      = j.num("descentStartVsMps", b.descentStartVsMps);
    b.descentStartHsMps      = j.num("descentStartHsMps", b.descentStartHsMps);
    b.descentMaxTiltDeg      = j.num("descentMaxTiltDeg", b.descentMaxTiltDeg);
    b.descentSafeVsMps       = j.num("descentSafeVsMps", b.descentSafeVsMps);
    b.descentSafeHsMps       = j.num("descentSafeHsMps", b.descentSafeHsMps);
    b.descentMaxSlopeDeg     = j.num("descentMaxSlopeDeg", b.descentMaxSlopeDeg);
    b.lunarGM3               = j.num("lunarGM3", b.lunarGM3);
    b.earthGM                = j.num("earthGM", b.earthGM);

    b.baseHazardChance       = j.num("baseHazardChance", b.baseHazardChance);
}

}  // namespace

bool Balance::load(const std::string& path) {
    std::string err;
    const Json j = Json::parseFile(path, &err);
    if (j.isNull()) return false;
    overlay(*this, j);
    return true;
}

}  // namespace lt
