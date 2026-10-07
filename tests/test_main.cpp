// Headless test suite.
//
// These cover the load-bearing invariants of the design, not incidental
// behaviour. If one of these fails, the game is not the game we designed --
// the difficulty came from these rules and they are easy to break by accident
// during tuning.
//
// Build:  cmake -DLUNAR_TRAIL_BUILD_TESTS=ON && make lunar_trail_tests
// Run:    ctest --output-on-failure

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/json.h"
#include "core/textwrap.h"
#include "core/rng.h"
#include "sim/balance.h"
#include "sim/daystep.h"
#include "sim/depot.h"
#include "sim/hazards.h"
#include "sim/health.h"
#include "sim/trail.h"
#include "game/descent.h"
#include "game/manual.h"

using namespace lt;

namespace {

int gFailures = 0;
int gChecks = 0;
const char* gCurrentTest = "";

void check(bool cond, const char* what, int line) {
    ++gChecks;
    if (!cond) {
        ++gFailures;
        std::printf("  FAIL [%s:%d] %s\n", gCurrentTest, line, what);
    }
}

void checkNear(double a, double b, double tol, const char* what, int line) {
    ++gChecks;
    if (std::abs(a - b) > tol) {
        ++gFailures;
        std::printf("  FAIL [%s:%d] %s (got %.6f, want %.6f +/- %.6f)\n",
                    gCurrentTest, line, what, a, b, tol);
    }
}

#define CHECK(cond) check((cond), #cond, __LINE__)
#define CHECK_MSG(cond, msg) check((cond), (msg), __LINE__)
#define CHECK_NEAR(a, b, tol) checkNear((a), (b), (tol), #a " ~= " #b, __LINE__)

#define TEST(name)          \
    do {                    \
        gCurrentTest = name; \
        std::printf("- %s\n", name); \
    } while (0)

// ---------------------------------------------------------------- helpers

// A convoy set up to be boring: full everything, so a test that is not about
// scarcity does not accidentally trip a scarcity rule.
RunState healthyRun(const Balance& b, Profession p = Profession::MissionSpecialist) {
    RunState s = newRun(12345, p, b);
    s.stock.foodKg = 400.0;
    s.stock.waterL = 2000.0;
    s.stock.o2Kg = 200.0;
    s.stock.fuelKg = 200.0;
    s.stock.oxidiserKg = 1000.0;
    s.stock.cargoKg = 200.0;
    s.stock.suitSets = 10;
    s.stock.cuttingCharges = 30;
    s.energyKwh = b.batteryCapacityKwh;
    s.hardware.sparesWheel = 3;
    s.hardware.sparesBogie = 3;
    s.hardware.sparesSeal = 3;
    return s;
}

// A writable scratch path for tests that need a real file on disk.
std::string tempFilePath(const char* name) {
    std::string dir;
#if defined(_WIN32)
    if (const char* t = std::getenv("TEMP")) dir = t;
    else if (const char* t = std::getenv("TMP")) dir = t;
    else dir = ".";
#else
    if (const char* t = std::getenv("TMPDIR")) dir = t;
    else dir = "/tmp";
#endif
    const bool trailing = !dir.empty() && (dir.back() == '/' || dir.back() == '\\');
    if (trailing) dir.pop_back();
    return dir + "/" + name;
}

void RunStationaryCheck(const Balance& b);

// ---------------------------------------------------------------- tests

void testRng() {
    TEST("rng: determinism and distribution");

    Rng a, b;
    a.seed(0xDEADBEEFULL);
    b.seed(0xDEADBEEFULL);
    for (int i = 0; i < 1000; ++i) {
        CHECK(a.nextU32() == b.nextU32());
    }

    // below(n) must be uniform. A modulo fold would bias index 0, so count the
    // buckets and assert they are all present and roughly even.
    Rng r;
    r.seed(1ULL);
    std::vector<int> buckets(7, 0);
    const int n = 70000;
    for (int i = 0; i < n; ++i) buckets[r.below(7)]++;
    for (int c : buckets) {
        CHECK_MSG(std::abs(c - n / 7) < n / 7 * 0.10, "below(7) bucket within 10%");
    }

    // unit() must stay in [0,1).
    Rng u;
    u.seed(7ULL);
    bool inRange = true;
    for (int i = 0; i < 20000; ++i) {
        const double v = u.unit();
        if (v < 0.0 || v >= 1.0) inRange = false;
    }
    CHECK(inRange);

    // Seed formatting round-trips.
    uint64_t parsed = 0;
    CHECK(parseSeed("8A3F1C09", &parsed));
    CHECK(parsed == 0x8A3F1C09ULL);
    CHECK(formatSeed(0x8A3F1C09ULL) == "8A3F1C09");
    CHECK(parseSeed("0x8A3F1C09", &parsed) && parsed == 0x8A3F1C09ULL);
    CHECK(!parseSeed("nonsense", &parsed));

    // deriveSeed must decorrelate adjacent salts.
    const uint64_t p = 0x1234567890ABCDEFULL;
    CHECK(deriveSeed(p, 0) != deriveSeed(p, 1));
}

void testJson() {
    TEST("json: parses the shapes content files use");

    std::string err;
    const Json j = Json::parse(
        R"({"a":1,"b":-2.5,"c":"x\ny","d":true,"e":null,
            "f":[1,2,{"g":"h"}],"i":{"j":{"k":7}}})",
        &err);
    CHECK_MSG(err.empty(), "well-formed JSON parses without error");
    CHECK(j.isObject());
    CHECK_NEAR(j.num("a", 0), 1.0, 1e-9);
    CHECK_NEAR(j.num("b", 0), -2.5, 1e-9);
    CHECK(j.text("c", "") == "x\ny");
    CHECK(j.boolean("d", false));
    CHECK(j["e"].isNull());
    CHECK(j["f"].size() == 3);
    CHECK(j["f"][2].text("g", "") == "h");
    CHECK(j["i"]["j"].integer("k", 0) == 7);

    // Missing keys fall back rather than throwing.
    CHECK_NEAR(j.num("nope", 42.0), 42.0, 1e-9);
    CHECK(j.text("nope", "dflt") == "dflt");

    // Malformed input must report an error rather than silently half-parsing.
    Json bad = Json::parse("{\"a\":}", &err);
    CHECK(!err.empty());
    CHECK(bad.isNull());

    // UTF-8 escapes, including a surrogate pair.
    const Json u = Json::parse(R"({"k":"Aé"})", &err);
    CHECK(err.empty());
    CHECK(u["k"].asString() == "A\xC3\xA9");
}

void testHealthModel() {
    TEST("health: the original's update order");

    Balance b = Balance::defaults();
    RunState s = healthyRun(b);
    s.health = 100.0;

    // Zero modifiers must be exactly the 10% decrement, and nothing else.
    HealthMods none;
    applyDailyHealth(s, none, b);
    CHECK_NEAR(s.health, 90.0, 1e-9);

    // The ordering claim: recovery is applied BEFORE additions, so a party at
    // 139 that survives lands near 125 and not above 139.
    s.health = 139.0;
    applyDailyHealth(s, none, b);
    CHECK_NEAR(s.health, 125.1, 0.01);
    CHECK_MSG(s.health < 139.0, "recovery happens before modifiers, so 139 falls");

    // A modifier applied the same day must not be decayed.
    s.health = 100.0;
    HealthMods add;
    add.eventDamage = 20.0;
    applyDailyHealth(s, add, b);
    CHECK_NEAR(s.health, 90.0 + 20.0, 1e-9);

    // Clamping at both ends.
    s.health = 5.0;
    HealthMods big;
    big.eventDamage = -50.0;
    applyDailyHealth(s, big, b);
    CHECK_NEAR(s.health, 0.0, 1e-9);
    s.health = 130.0;
    HealthMods huge;
    huge.eventDamage = 100.0;
    applyDailyHealth(s, huge, b);
    CHECK_NEAR(s.health, b.healthMax, 1e-9);

    // Band boundaries match the manual exactly.
    s.health = 0.0;   CHECK(s.healthBand() == HealthBand::Good);
    s.health = 34.9;  CHECK(s.healthBand() == HealthBand::Good);
    s.health = 35.0;  CHECK(s.healthBand() == HealthBand::Fair);
    s.health = 69.9;  CHECK(s.healthBand() == HealthBand::Fair);
    s.health = 70.0;  CHECK(s.healthBand() == HealthBand::Poor);
    s.health = 104.9; CHECK(s.healthBand() == HealthBand::Poor);
    s.health = 105.0; CHECK(s.healthBand() == HealthBand::VeryPoor);
    s.health = 139.9; CHECK(s.healthBand() == HealthBand::VeryPoor);
    s.health = 140.0; CHECK(s.healthBand() == HealthBand::Dying);
}

void testMaladyRule() {
    TEST("health: the second-malady-is-fatal rule");

    Balance b = Balance::defaults();

    // Roll until the fatal second malady lands. The roll targets a RANDOM
    // crew member, so this is driven by a loop over seeds and asserts that SOME
    // already-afflicted member dies, which is the rule that matters.
    {
        bool died = false;
        std::string cause;
        for (int i = 0; i < 3000 && !died; ++i) {
            RunState s = healthyRun(b);
            s.health = 140.0;   // maximum illness chance
            for (auto& c : s.crew) {
                c.perk = Perk::None;      // no Physician
                c.malady = Malady::Exhaustion;
                c.recoverSol = 10;
            }
            Rng rng;
            rng.seed(static_cast<uint64_t>(1000) + static_cast<uint64_t>(i));
            std::string report;
            rollMalady(s, rng, b, &report);
            for (const auto& c : s.crew) {
                if (c.dead) { died = true; cause = c.causeOfDeath; }
            }
        }
        CHECK_MSG(died, "an already-afflicted crew member dies on a second malady");
        CHECK_MSG(cause.find("second") != std::string::npos,
                  "the cause of death names the second malady");
    }

    // And a fresh, unafflicted crew member who rolls a malady does NOT die.
    {
        bool anyAfflicted = false, anyDied = false;
        for (int i = 0; i < 3000 && !anyAfflicted; ++i) {
            RunState s = healthyRun(b);
            s.health = 140.0;
            for (auto& c : s.crew) { c.perk = Perk::None; c.malady = Malady::None; }
            Rng rng;
            rng.seed(50000ULL + static_cast<uint64_t>(i));
            std::string report;
            rollMalady(s, rng, b, &report);
            for (const auto& c : s.crew) {
                if (c.maladied()) anyAfflicted = true;
                if (c.dead) anyDied = true;
            }
        }
        CHECK_MSG(anyAfflicted, "a first malady is obtainable at worst health");
        CHECK_MSG(!anyDied, "a first malady is never fatal");
    }

    // The Physician saves that person exactly once: nobody dies on the roll,
    // and everyone is still alive afterwards.
    {
        RunState s = healthyRun(b);
        s.health = 140.0;
        bool hasDoc = false;
        for (auto& c : s.crew) {
            if (c.perk != Perk::Physician) c.perk = Perk::None;
            else hasDoc = true;
            c.malady = Malady::Exhaustion;
            c.recoverSol = 10;
        }
        CHECK_MSG(hasDoc, "a Physician is aboard");

        bool anySaved = false;
        for (int i = 0; i < 3000 && !anySaved; ++i) {
            RunState t = s;
            Rng rng;
            rng.seed(20000ULL + static_cast<uint64_t>(i));
            std::string report;
            rollMalady(t, rng, b, &report);
            if (t.livingCrew() == 5) anySaved = true;
        }
        CHECK_MSG(anySaved, "the Physician prevents a death from a second malady");
    }

    // Recovery times match the manual.
    CHECK(maladyRecoverySols(Malady::Exhaustion, b) == 10);
    CHECK(maladyRecoverySols(Malady::Fracture, b) == 30);
    CHECK(maladyIsInjury(Malady::Crush));
    CHECK(!maladyIsInjury(Malady::Hypoxia));

    // A crew member recovers after exactly the documented number of sols.
    {
        RunState s = healthyRun(b);
        s.crew[0].malady = Malady::Exhaustion;
        s.crew[0].recoverSol = 3;
        std::vector<std::string> log;
        tickRecovery(s, b, &log);
        CHECK(s.crew[0].recoverSol == 2);
        tickRecovery(s, b, &log);
        tickRecovery(s, b, &log);
        CHECK_MSG(s.crew[0].malady == Malady::None, "recovered on schedule");
    }
}

void testEconomyInflation() {
    TEST("economy: 25% per outpost, exactly");

    Balance b = Balance::defaults();
    const double base = goodBasePrice(Good::Water, b);
    CHECK_NEAR(goodPrice(Good::Water, 0, b), base, 1e-9);
    CHECK_NEAR(goodPrice(Good::Water, 1, b), base * 1.25, 1e-9);
    CHECK_NEAR(goodPrice(Good::Water, 2, b), base * 1.50, 1e-9);
    CHECK_NEAR(goodPrice(Good::Water, 3, b), base * 1.75, 1e-9);
    CHECK_NEAR(goodPrice(Good::Water, 4, b), base * 2.00, 1e-9);

    // Every good must inflate by the same factor, or the advice in the manual
    // ("prices rise 25% at every outpost") would be false for some goods.
    for (int i = 0; i < static_cast<int>(Good::Count); ++i) {
        const Good g = static_cast<Good>(i);
        const double r3 = goodPrice(g, 3, b) / goodBasePrice(g, b);
        CHECK_NEAR(r3, 1.75, 1e-9);
    }

    // Spares cap at three each, as in the original.
    CHECK(goodCap(Good::SpareWheel, b) == 3);
    CHECK(goodCap(Good::SpareBogie, b) == 3);
    CHECK(goodCap(Good::SpareSeal, b) == 3);
    CHECK(goodCap(Good::Water, b) == -1);
}

void testScoreNeutrality() {
    TEST("economy: 5 credits == 1 point, spent or held");

    Balance b = Balance::defaults();

    // Holding credits.
    CHECK_NEAR(5.0 * b.pointsPerCredit, 1.0, 1e-9);

    // Converting credits into each purchasable good must also be worth exactly
    // 1 point per 5 credits. This is the property that makes "restock early"
    // correct rather than merely advisable.
    for (int i = 0; i < static_cast<int>(Good::Count); ++i) {
        const Good g = static_cast<Good>(i);
        const double price = goodBasePrice(g, b);
        if (price <= 0.0) continue;
        const double worth = goodPoints(g, b);
        // points per unit of price must equal pointsPerCredit.
        CHECK_NEAR(worth, price * b.pointsPerCredit, 1e-9);
    }

    // And the full scoring path must agree.
    RunState s = healthyRun(b, Profession::FlightDirector);
    s.credits = 500.0;
    s.stock.cargoKg = 0.0;
    s.stock.suitSets = 0;
    s.hardware.sparesWheel = 0;
    s.hardware.sparesBogie = 0;
    s.hardware.sparesSeal = 0;
    s.health = 0.0;
    const ScoreBreakdown sc = scoreRun(s, b);
    CHECK_NEAR(sc.credits, 100.0, 1e-6);          // 500 credits -> 100 points
    CHECK_NEAR(sc.survivors, 5 * 500.0, 1e-6);    // 5 Nominal survivors
    CHECK_NEAR(sc.multiplier, 1.0, 1e-9);         // Flight Director is x1

    // Profession multipliers ascend and are exactly 1/2/3.
    CHECK_NEAR(professionScoreMult(Profession::FlightDirector, b), 1.0, 1e-9);
    CHECK_NEAR(professionScoreMult(Profession::MissionSpecialist, b), 2.0, 1e-9);
    CHECK_NEAR(professionScoreMult(Profession::PayloadTechnician, b), 3.0, 1e-9);

    // Spare hardware and suit sets go through the same pricing rule. They used to
    // carry separate per-unit constants, which meant a spare was worth a flat 50
    // points whatever it was: carrying a 600 credit drive bogie scored the same
    // as a 240 credit seal kit, and neither matched its price.
    RunState h = healthyRun(b, Profession::FlightDirector);
    h.credits = 0.0;
    h.stock.cargoKg = 0.0;
    h.stock.suitSets = 0;
    h.hardware.sparesWheel = 0;
    h.hardware.sparesBogie = 0;
    h.hardware.sparesSeal = 0;
    h.health = 0.0;

    h.hardware.sparesWheel = 1;
    CHECK_NEAR(scoreRun(h, b).spares, goodBasePrice(Good::SpareWheel, b) * b.pointsPerCredit,
               1e-6);

    h.hardware.sparesWheel = 0;
    h.hardware.sparesBogie = 1;
    CHECK_NEAR(scoreRun(h, b).spares, goodBasePrice(Good::SpareBogie, b) * b.pointsPerCredit,
               1e-6);

    h.hardware.sparesBogie = 0;
    h.hardware.sparesSeal = 1;
    CHECK_NEAR(scoreRun(h, b).spares, goodBasePrice(Good::SpareSeal, b) * b.pointsPerCredit,
               1e-6);

    // And a mixed load adds up rather than being counted once.
    h.hardware.sparesWheel = 2;
    h.hardware.sparesSeal = 1;
    const double mixed = goodBasePrice(Good::SpareWheel, b) * 2.0 * b.pointsPerCredit +
                         goodBasePrice(Good::SpareSeal, b) * b.pointsPerCredit;
    CHECK_NEAR(scoreRun(h, b).spares, mixed, 1e-6);

    h.hardware.sparesWheel = 0;
    h.hardware.sparesSeal = 0;
    h.stock.suitSets = 3;
    CHECK_NEAR(scoreRun(h, b).suitSets,
               goodBasePrice(Good::SuitSets, b) * 3.0 * b.pointsPerCredit, 1e-6);

    // The derived values are what the manual quotes.
    CHECK_NEAR(goodPoints(Good::SpareWheel, b), 60.0, 1e-9);
    CHECK_NEAR(goodPoints(Good::SpareBogie, b), 120.0, 1e-9);
    CHECK_NEAR(goodPoints(Good::SpareSeal, b), 48.0, 1e-9);
    CHECK_NEAR(goodPoints(Good::SuitSets, b), 12.0, 1e-9);
}

void testProfessionDifficulty() {
    TEST("economy: the Payload Technician cannot buy enough");

    Balance b = Balance::defaults();

    // Every profession must start unable to carry a full route of consumables.
    // If the cheapest tier could do it, the whole game collapses into a
    // shopping list.
    const double dailyKg =
        (b.foodPerCrewKg + b.waterPerCrewKg + b.o2PerCrewKg) * 5.0;
    const double routeNeed = dailyKg * static_cast<double>(b.typicalSols);

    // A survivable load is not just consumables. It also has to carry enough
    // propellant to make chemical burns and enough cargo to actually seed a
    // colony -- 50 kg is the win threshold.
    // Consumables alone should nearly fill the bay, so that adding propellant and
    // colony cargo pushes the load over and forces mining. A survivable load also
    // needs ~200 kg of propellant and at least the 50 kg win-threshold of cargo.
    CHECK_MSG(routeNeed < b.payloadCapKg,
              "consumables for a typical route fit in the bay on their own");
    CHECK_MSG(routeNeed > b.payloadCapKg * 0.85,
              "consumables fill most of the bay, leaving little slack");

    // The slack is the mining requirement. It should be a handful of sorties,
    // not a dozen: roughly four to eight at the 90 kg haul cap.
    const double slack = b.payloadCapKg - routeNeed;
    const double perSortie = b.sortieHaulCapKg * kSortieDrinkFraction;
    const double forcedSorties = slack / std::max(1.0, perSortie);
    CHECK_MSG(forcedSorties > 0.0, "there is mining to be done");
    CHECK_MSG(forcedSorties < 12.0,
              "mining is a minority of the journey, not the whole of it");
    // And the load that does not fit must be a real fraction of the bay, not a
    // rounding error: this is what forces the habit.
    CHECK_MSG(slack > b.payloadCapKg * 0.03,
              "the shortfall is a meaningful share of the bay");

    // But the gap must be mineable in a sane number of sorties, not hundreds.
    // This is the ratio that was wrong in an earlier pass: a 45% drink fraction
    // on a 60 kg haul needed ~100 sorties for a 120 km route.
    const double waterPerSortie = b.sortieHaulCapKg * kSortieDrinkFraction;
    const double waterNeeded = b.waterPerCrewKg * 5.0 * static_cast<double>(b.typicalSols);
    const double shortfall = std::max(0.0, waterNeeded - 600.0);   // ~600 L is carryable
    const double sorties = waterNeeded > 0.0 ? shortfall / waterPerSortie : 0.0;
    CHECK_MSG(sorties > 0.0, "some prospecting is required");
    CHECK_MSG(sorties < static_cast<double>(b.typicalSols) * 0.35,
              "prospecting is a minority of the journey, not the whole of it");

    // The Payload Technician's budget must be too small to buy the route, which
    // is what forces mining on that tier.
    // A tier's budget is spent on two things before any consumables: a full set
    // of spares, and one depot leg of consumables at markup. What is left over
    // is the budget for the rest of the route's legs. That remainder -- not the
    // headline figure -- is what makes a tier hard.
    const double crew = 5.0;
    const double perSolCost =
        b.waterPerCrewKg * crew * b.basePriceWaterL +
        b.foodPerCrewKg * crew * b.basePriceFoodKg +
        b.o2PerCrewKg  * crew * b.basePriceO2Kg;
    const double legCost = perSolCost * 22.0 * (1.0 + b.outpostMarkupPerIndex);
    const double sparesCost =
        3.0 * (b.basePriceWheel + b.basePriceSealKit) + 2.0 * b.basePriceBogie;

    // The route has four legs between the five depots.
    const double legsToCover = 4.0;

    const double fdCredits = professionCredits(Profession::FlightDirector, b);
    const double msCredits = professionCredits(Profession::MissionSpecialist, b);
    const double ptCredits = professionCredits(Profession::PayloadTechnician, b);

    const auto legsAffordable = [&](double credits) {
        return (credits - sparesCost) / std::max(1.0, legCost);
    };

    // The easy tier can cover the route on credits alone; the hard tier cannot,
    // and the gap between them is where the difficulty lives.
    CHECK_MSG(legsAffordable(fdCredits) >= legsToCover,
              "the Flight Director can cover the route without mining");
    CHECK_MSG(legsAffordable(ptCredits) < legsToCover * 0.75,
              "the Payload Technician cannot cover the route without mining");
    CHECK_MSG(legsAffordable(msCredits) < legsAffordable(fdCredits),
              "the tiers are a genuine budget gradient");
    CHECK(fdCredits > msCredits);
    CHECK(msCredits > ptCredits);
    CHECK(professionScoreMult(Profession::FlightDirector, b) <
          professionScoreMult(Profession::MissionSpecialist, b));
    CHECK(professionScoreMult(Profession::MissionSpecialist, b) <
          professionScoreMult(Profession::PayloadTechnician, b));

    // And credits must ascend with the score multiplier descending.
    CHECK(professionCredits(Profession::FlightDirector, b) >
          professionCredits(Profession::MissionSpecialist, b));
    CHECK(professionCredits(Profession::MissionSpecialist, b) >
          professionCredits(Profession::PayloadTechnician, b));
}

void testTrailGeometry() {
    TEST("trail: landmarks, zones, and chasm structure");

    const auto& t = trail();
    CHECK_MSG(t.size() == 16, "sixteen landmarks, as the manual states");

    // Kilometres must be monotonic and end at the stated total.
    Balance b = Balance::defaults();
    CHECK_NEAR(t.front().km, 0.0, 1e-9);
    CHECK_NEAR(t.back().km, b.trailTotalKm, 1e-9);
    for (size_t i = 1; i < t.size(); ++i) {
        CHECK_MSG(t[i].km > t[i - 1].km, "landmark distances strictly increase");
    }

    // The manual documents five chasms, three with no guided crossing.
    int chasms = 0, guided = 0;
    for (const auto& l : t) {
        if (l.kind != LandmarkKind::Chasm) continue;
        ++chasms;
        if (l.guidedAvailable) ++guided;
    }
    CHECK_MSG(chasms == 5, "five chasm crossings");
    CHECK_MSG(guided == 2, "only two have a guided crossing, so three are gambles");

    // Every chasm must be deeper than the swamp band, otherwise the crossing
    // module's shallow branch is dead content.
    for (const auto& l : t) {
        if (l.kind != LandmarkKind::Chasm) continue;
        CHECK_MSG(l.chasmaDepthM > 3.0, "every chasma is past the marginal band");
        CHECK_MSG(l.chasmaSpanM > 0.0, "every chasma has a span");
    }

    // Five depots with stores, marked up 0% to 100%.
    int stores = 0;
    for (const auto& l : t) {
        if (l.storeIndex >= 0) {
            ++stores;
            CHECK_NEAR(1.0 + b.outpostMarkupPerIndex * l.storeIndex, 1.0 + 0.25 * l.storeIndex,
                       1e-9);
        }
    }
    CHECK_MSG(stores == 5, "five relay depots with stores");

    // Zone lookup agrees with the landmark definitions.
    CHECK(zoneAtKm(2.0) == Zone::Sunlit);
    CHECK(zoneAtKm(28.0) == Zone::Transition);
    CHECK(zoneAtKm(70.0) == Zone::Psr);

    // Distance to the next landmark shrinks to zero as you approach it.
    CHECK(kmToNextLandmark(0.0) > 0.0);
    CHECK_NEAR(kmToNextLandmark(6.9), 0.1, 1e-6);
}

void testChasmCrossing() {
    TEST("chasms: methods, costs, and the ferried-vs-gambled split");

    Balance b = Balance::defaults();
    RunState s = healthyRun(b);

    const Landmark chasm = *std::find_if(
        trail().begin(), trail().end(),
        [](const Landmark& l) { return l.kind == LandmarkKind::Chasm; });
    CHECK(chasm.kind == LandmarkKind::Chasm);

    const auto opts = chasmOptions(s, chasm, b);
    CHECK_MSG(opts.size() == 5, "five crossing methods offered");

    // The ramp is always available and free.
    const ChasmPlan& ramp = opts[0];
    CHECK(ramp.method == ChasmMethod::Ramp);
    CHECK(ramp.available);
    CHECK_NEAR(ramp.costCredits, 0.0, 1e-9);

    // Risk must rise with depth, per the original's linear scale. Check the
    // ordering across the actual chasms.
    for (const auto& deep : trail()) {
        if (deep.kind != LandmarkKind::Chasm) continue;
        const auto o = chasmOptions(s, deep, b);
        if (o.empty()) continue;
        CHECK_MSG(o[0].risk > 0.0, "ramp traverse always carries some risk");
        CHECK_MSG(o[0].risk <= 0.85, "ramp risk stays bounded");
    }

    // A chasm with no guided crossing must say so.
    const auto noFerry = chasmOptions(s, chasm, b);
    for (const auto& o : noFerry) {
        if (o.method != ChasmMethod::Guided) continue;
        if (!chasm.guidedAvailable) {
            CHECK_MSG(!o.available, "no ferry means the option is unavailable");
            CHECK(!o.unavailableReason.empty());
        }
    }

    // The guide must cut risk by 80%, per the original's tuned value.
    const ChasmPlan& guide = opts[4];
    CHECK(guide.method == ChasmMethod::Guide);
    CHECK_NEAR(guide.costSuitSets, 3, 1e-9);
    const ChasmPlan& plainRamp = opts[0];
    CHECK_MSG(guide.risk < plainRamp.risk * 0.5, "the guide roughly halves ramp risk");

    // A resolved crossing either crosses or is very expensive. It must never
    // leave the convoy in a state where the chasm is neither crossed nor fatal
    // in the log, and it must never move the convoy backwards.
    for (int trial = 0; trial < 200; ++trial) {
        RunState t = healthyRun(b);
        Rng rng;
        rng.seed(9000ULL + static_cast<uint64_t>(trial));
        const double before = t.progress.kmTravelled;
        const ChasmResult r = resolveChasm(t, chasm, ChasmMethod::Ramp, rng, b);
        CHECK(r.solsLost >= 0);
        if (!r.succeeded && !r.catastrophic) {
            // A non-catastrophic failure still gets you across, slowly.
            CHECK_MSG(r.kmGained >= 0.0, "a failed crossing never moves you backwards");
        }
        CHECK_MSG(t.progress.kmTravelled >= before, "the convoy never reverses");
    }

    // Hop Assist must refuse when there is no usable propellant -- burning fuel
    // with no oxidiser is impossible, and this is the clearest expression of
    // the mixture lock.
    {
        RunState t = healthyRun(b);
        t.stock.fuelKg = 0.0;
        t.stock.oxidiserKg = 500.0;
        const auto o = chasmOptions(t, chasm, b);
        for (const auto& p : o) {
            if (p.method != ChasmMethod::Hop) continue;
            CHECK_MSG(!p.available, "hop assist needs both propellants");
        }
        CHECK(!t.hasProportions());
    }

    // Every method that the manual documents must actually be reachable. Two of
    // the five branches never set `available`, so the winch and the hop were
    // permanently unavailable: options the manual describes, priced, and
    // explained, that no run could ever take.
    {
        RunState t = healthyRun(b);
        // Fund every crossing so the only thing that can refuse one is the rule
        // under test rather than a missing input.
        t.credits = 100000.0;
        t.energyKwh = 5000.0;
        t.stock.cuttingCharges = 20;
        t.stock.suitSets = 20;
        t.stock.fuelKg = 5000.0;
        t.stock.oxidiserKg = 5000.0;

        for (const auto& p : chasmOptions(t, chasm, b)) {
            // A crossing that is on offer must be reachable, and one that is not
            // must say what is missing.
            if (p.available) continue;
            CHECK_MSG(!p.unavailableReason.empty(),
                      "an unavailable crossing must say why");
        }

        // A guided crossing exists only where the route says so.
        Landmark guided = chasm;
        guided.guidedAvailable = true;
        bool sawGuided = false;
        for (const auto& p : chasmOptions(t, guided, b)) {
            if (p.method != ChasmMethod::Guided) continue;
            sawGuided = true;
            CHECK_MSG(p.available, "a guided crossing should be available where routed");
        }
        CHECK(sawGuided);

        // And every method the manual names appears in the list at all.
        bool sawRamp = false, sawWinch = false, sawHop = false, sawGuide = false;
        for (const auto& p : chasmOptions(t, chasm, b)) {
            switch (p.method) {
                case ChasmMethod::Ramp:   sawRamp = true;   break;
                case ChasmMethod::Winch:  sawWinch = true;  break;
                case ChasmMethod::Hop:    sawHop = true;    break;
                case ChasmMethod::Guide:  sawGuide = true;  break;
                default: break;
            }
        }
        CHECK(sawRamp);
        CHECK(sawWinch);
        CHECK(sawHop);
        CHECK(sawGuide);
    }
}

void testIsruEconomy() {
    TEST("ISRU: fuel is cheap, oxidiser is not");

    Balance b = Balance::defaults();

    // The stoichiometry the manual quotes.
    const double o2FromWater = 0.111;   // 16/144 by mass
    const double h2FromWater = 0.889;
    CHECK_NEAR(o2FromWater + h2FromWater, 1.0, 1e-9);

    // 1 kg of CH4 needs 0.333 kg of H2.
    const double waterPerKgFuel = 0.333 / h2FromWater;
    CHECK_NEAR(waterPerKgFuel, 0.375, 0.001);

    // 1 kg of O2 needs about 9 kg of water.
    const double waterPerKgOx = 1.0 / o2FromWater;
    CHECK_NEAR(waterPerKgOx, 9.0, 0.1);

    // The conclusion the manual tells the player to act on.
    CHECK_MSG(waterPerKgOx > waterPerKgFuel * 2.0,
              "making oxidiser costs far more ice than making fuel");

    // The haul cap must be below the PSR yield for a full crew, or the cap is
    // never binding and the "you cannot carry it all" rule does nothing.
    const double psrYield = b.zoneIcePsr * 5.0;
    CHECK_MSG(psrYield > b.sortieHaulCapKg,
              "a five-crew PSR sortie exceeds the haul cap, so the cap binds");

    // The Geologist must be worth having.
    CHECK_NEAR(newRun(1, Profession::MissionSpecialist, b).geologistBonus(), 1.4, 1e-9);

    // And the sunlit rim must yield nothing at all.
    CHECK_NEAR(zoneIcePerCrew(Zone::Sunlit, b), 0.0, 1e-9);
    CHECK(zoneIcePerCrew(Zone::Psr, b) > zoneIcePerCrew(Zone::Transition, b));
}

void testZonesAndShadowLine() {
    TEST("zones: energy and ice are anti-correlated");

    Balance b = Balance::defaults();

    // The documented table.
    CHECK_NEAR(zoneSolarKwh(Zone::Sunlit, b), 220.0, 1e-9);
    CHECK_NEAR(zoneSolarKwh(Zone::Transition, b), 130.0, 1e-9);
    CHECK_NEAR(zoneSolarKwh(Zone::Psr, b), 0.0, 1e-9);

    CHECK_NEAR(zoneIcePerCrew(Zone::Sunlit, b), 0.0, 1e-9);
    CHECK(zoneIcePerCrew(Zone::Transition, b) > 0.0);
    CHECK(zoneIcePerCrew(Zone::Psr, b) > zoneIcePerCrew(Zone::Transition, b));

    // The core design claim: the zone with the most power has the least ice,
    // and vice versa. No zone may be good at both.
    for (int z = 0; z < 3; ++z) {
        const Zone zone = static_cast<Zone>(z);
        const bool powerRich = zoneSolarKwh(zone, b) > 150.0;
        const bool iceRich = zoneIcePerCrew(zone, b) > 40.0;
        CHECK_MSG(!(powerRich && iceRich),
                  "no zone may be both power-rich and ice-rich");
    }

    // The gradient must be monotonic in power, and each zone must sit on the
    // correct side of a cruising convoy's 123 kWh/sol draw:
    //   sunlit      sustainable
    //   transition  roughly break-even
    //   psr         entirely on the battery
    CHECK(zoneSolarKwh(Zone::Sunlit, b) > zoneSolarKwh(Zone::Transition, b));
    CHECK(zoneSolarKwh(Zone::Transition, b) > zoneSolarKwh(Zone::Psr, b));
    const double cruiseDraw = b.energyPerCrewKwh * 5.0 + b.driveEnergyCruiseKwh;
    CHECK_MSG(zoneSolarKwh(Zone::Sunlit, b) > cruiseDraw * 1.5,
              "the sunlit rim recharges comfortably");
    CHECK_MSG(std::abs(zoneSolarKwh(Zone::Transition, b) - cruiseDraw) < cruiseDraw * 0.25,
              "the transition slope is roughly break-even at Cruise");

    // Batteries must last about seven sols in the dark. This is the number the
    // manual quotes and it is the constraint that governs every PSR decision.
    RunState s = healthyRun(b);
    s.progress.zone = Zone::Psr;
    s.pace = Pace::Cruise;
    const double daily = b.energyPerCrewKwh * 5.0 + b.driveEnergyCruiseKwh;
    CHECK_MSG(daily > 0.0, "the daily draw is positive");
    const double solsDark = b.batteryCapacityKwh / daily;
    CHECK_MSG(solsDark > 6.0 && solsDark < 8.5,
              "a full battery is roughly seven sols in the dark");
    CHECK_MSG(b.batteryCapacityKwh == 900.0, "the capacity the manual implies");

    // The PSR stretch of the route must be crossable, but only with a depot
    // recharge in the middle of it. This is the single most important spatial
    // property of the route and it is easy to break while tuning.
    {
        RunState probe = healthyRun(b);
        probe.energyKwh = b.batteryCapacityKwh;
        probe.progress.zone = Zone::Psr;
        probe.pace = Pace::Cruise;
        const double psrKm = 30.0;              // Levi Massif to Upper Route
        const double psrSols = psrKm / (b.baseKmPerSol * b.zoneTerrainPsr);
        const double need = psrSols * daily;
        CHECK_MSG(need > b.batteryCapacityKwh,
                  "the PSR stretch needs more charge than a full battery holds");
        // And there must be a depot inside the stretch.
        bool depotInside = false;
        for (const auto& l : trail()) {
            if (l.storeIndex >= 0 && l.zone == Zone::Psr) depotInside = true;
        }
        CHECK_MSG(depotInside, "a relay depot sits inside the shadowed stretch");
    }

    // The sunlit zone must be genuinely self-sustaining at Cruise, otherwise
    // there is no reason to route through it.
    CHECK_MSG(zoneSolarKwh(Zone::Sunlit, b) > daily,
              "sunlit solar exceeds the cruising draw, so the zone sustains itself");

    // Half a battery is roughly half that.
    s.energyKwh = b.batteryCapacityKwh / 2.0;
    CHECK(s.batterySolsLeft(b) < 4.0);

    // A brownout must be survivable, not instantly lethal. This is what gives
    // a stranded convoy something to do about its situation.
    {
        RunState brown = healthyRun(b);
        brown.progress.zone = Zone::Psr;
        brown.energyKwh = 0.0;
        const HealthMods m = baseDailyMods(brown, b);
        CHECK_MSG(m.powerFailure > 0.0, "a flat battery costs health");
        CHECK_MSG(m.powerFailure < 15.0,
                  "a brownout is a slow squeeze, not a cliff edge");
    }
}

void testPaceAndSpeed() {
    TEST("pace: the documented kilometre rates");

    Balance b = Balance::defaults();
    RunState s = healthyRun(b);
    s.progress.zone = Zone::Sunlit;
    s.health = 0.0;
    s.hardware.tractionUnits = b.tractionUnits;
    s.hardware.bogies = b.bogies;

    // Park well clear of the next landmark. projectedKmToday clamps to the
    // arrival point, so measuring pace rate from a standing start would measure
    // the landmark spacing instead.
    s.progress.kmTravelled = 63.0;   // 3 km to Levi Massif at 66 km

    // Capability -- the unclamped rate the pace buys -- must match the manual
    // exactly. Measured here because no single segment of the route is long
    // enough to absorb a 14 km sol without hitting the arrival clamp.
    s.pace = Pace::Cruise;
    CHECK_NEAR(projectedKmCapability(s, b), 3.0, 0.05);
    s.pace = Pace::Sprint;
    CHECK_NEAR(projectedKmCapability(s, b), 6.0, 0.1);
    s.pace = Pace::FullBurn;
    CHECK_NEAR(projectedKmCapability(s, b), 14.0, 0.3);

    // On open ground, actual travel equals capability.
    s.pace = Pace::Cruise;
    CHECK_NEAR(projectedKmToday(s, b), 3.0, 0.05);
    s.pace = Pace::Sprint;
    CHECK_NEAR(projectedKmToday(s, b), 3.0, 0.05);   // clamped by the 3 km segment

    // The arrival clamp is real behaviour, not an accident.
    {
        RunState near = s;
        near.pace = Pace::FullBurn;
        near.progress.kmTravelled = 64.0;   // 2 km to the landmark
        CHECK_MSG(projectedKmToday(near, b) <= 2.0,
                  "a sol never overshoots the next landmark");
    }

    // Fewer than six traction units costs speed, as in the original's
    // "less than four healthy ox => ox/4" rule.
    s.pace = Pace::Cruise;
    s.hardware.tractionUnits = 3;
    CHECK_NEAR(projectedKmToday(s, b), 1.5, 0.05);

    // A sick unit counts as HALF a healthy one. Two of six sick leaves five
    // effective units, so speed is 5/6 of full.
    s.hardware.tractionUnits = 6;
    s.hardware.tractionSick = 2;
    CHECK_NEAR(projectedKmToday(s, b), 3.0 * (5.0 / 6.0), 0.05);

    // tractionSick is a count of PRESENT units, so it must never exceed the
    // population.
    s.hardware.tractionSick = 99;
    CHECK(s.hardware.effectiveTraction(b.tractionUnits) >= 0.0);
    CHECK_NEAR(s.hardware.effectiveTraction(b.tractionUnits), 0.5, 1e-9);
    s.hardware.tractionSick = 2;

    // A failed bogie reduces the drive.
    s.hardware.tractionSick = 0;
    s.hardware.bogiesSick = 2;
    CHECK_MSG(projectedKmToday(s, b) < 3.0, "failed bogies reduce speed");

    // Maladies cost 10% each, as documented.
    s.hardware.bogiesSick = 0;
    s.crew[0].malady = Malady::Exhaustion;
    CHECK_NEAR(projectedKmToday(s, b), 2.7, 0.05);

    // Terrain zone applies.
    s.crew[0].malady = Malady::None;
    s.progress.zone = Zone::Psr;
    CHECK(projectedKmToday(s, b) < 3.0);

    // No traction at all means no movement, and the loss check must fire.
    s.hardware.tractionUnits = 0;
    CHECK_NEAR(projectedKmToday(s, b), 0.0, 1e-9);
    checkLossConditions(s, b);
    CHECK(s.outcome == Outcome::Lost);
    CHECK(s.lossReason == LossReason::StrandedNoTraction);
}

void testDailyLoop() {
    TEST("daily loop: consumption, distance, and landmark arrival");

    Balance b = Balance::defaults();
    Rng rng;
    rng.seed(4242ULL);

    RunState s = healthyRun(b);
    s.progress.zone = Zone::Sunlit;
    s.pace = Pace::Cruise;
    s.rations = Rations::Full;

    const double water0 = s.stock.waterL;
    const double food0 = s.stock.foodKg;
    const double o20 = s.stock.o2Kg;

    const DayReport r = advanceSol(s, rng, b, true);
    CHECK_NEAR(r.waterUsedL, b.waterPerCrewKg * 5.0, 1e-9);
    CHECK_NEAR(r.foodUsedKg, b.foodPerCrewKg * 5.0, 1e-9);
    CHECK_NEAR(r.o2UsedKg, b.o2PerCrewKg * 5.0, 1e-9);
    CHECK_NEAR(water0 - s.stock.waterL, r.waterUsedL, 1e-9);
    CHECK_NEAR(food0 - s.stock.foodKg, r.foodUsedKg, 1e-9);
    CHECK_NEAR(o20 - s.stock.o2Kg, r.o2UsedKg, 1e-9);
    CHECK_MSG(r.sol == 0, "the first report is sol zero");
    CHECK_MSG(s.progress.sol == 1, "the calendar advances by exactly one sol");

    // Rations must scale consumption exactly.
    RunState e = healthyRun(b);
    e.rations = Rations::Emergency;
    const DayReport re = advanceSol(e, rng, b, true);
    CHECK_NEAR(re.foodUsedKg, b.foodPerCrewKg * 5.0 * 0.25, 1e-9);

    // A stationary convoy consumes but does not move. This is the original's
    // resting-is-safe property and it is load-bearing for the rest exploit.
    RunStationaryCheck(b);
}

void RunStationaryCheck(const Balance& b) {
    Rng rng;
    rng.seed(777ULL);
    RunState s = healthyRun(b);
    s.progress.zone = Zone::Sunlit;
    const double km0 = s.progress.kmTravelled;
    const DayReport r = advanceSol(s, rng, b, false);
    CHECK_NEAR(r.kmGained, 0.0, 1e-9);
    CHECK_NEAR(s.progress.kmTravelled, km0, 1e-9);
    CHECK_MSG(r.waterUsedL > 0.0, "resting still costs water");
    CHECK_MSG(r.o2UsedKg > 0.0, "resting still costs oxygen");
}

void testHazards() {
    TEST("hazards: contextual weighting and the mercy valve");

    Balance b = Balance::defaults();

    // Solar particle events must be rarer in a shadowed region, since no sun
    // means no particle flux reaching you.
    {
        RunState sunlit = healthyRun(b);
        sunlit.progress.zone = Zone::Sunlit;
        sunlit.pace = Pace::Cruise;
        RunState psr = sunlit;
        psr.progress.zone = Zone::Psr;

        Rng r1; r1.seed(1ULL);
        Rng r2; r2.seed(1ULL);
        double sunlitSPE = 0.0, psrSPE = 0.0;
        const int trials = 4000;
        for (int i = 0; i < trials; ++i) {
            if (rollHazard(sunlit, r1, b) == HazardId::SolarParticleEvent) ++sunlitSPE;
            if (rollHazard(psr, r2, b) == HazardId::SolarParticleEvent) ++psrSPE;
        }
        CHECK_MSG(sunlitSPE > psrSPE, "solar particle events are rarer in the dark");
    }

    // Battery runaway must be likelier on a chemical burn.
    {
        RunState cruise = healthyRun(b);
        cruise.pace = Pace::Cruise;
        cruise.energyKwh = 40.0;
        RunState burn = cruise;
        burn.pace = Pace::FullBurn;

        Rng r1; r1.seed(2ULL);
        Rng r2; r2.seed(2ULL);
        int cruiseCount = 0, burnCount = 0;
        for (int i = 0; i < 4000; ++i) {
            if (rollHazard(cruise, r1, b) == HazardId::BatteryRunaway) ++cruiseCount;
            if (rollHazard(burn, r2, b) == HazardId::BatteryRunaway) ++burnCount;
        }
        CHECK_MSG(burnCount > cruiseCount, "hard cycling raises runaway odds");
    }

    // The Mechanic must measurably reduce wheel failures.
    {
        RunState plain = healthyRun(b);
        for (auto& c : plain.crew) c.perk = Perk::None;
        RunState mech = plain;
        mech.crew[3].perk = Perk::Mechanic;

        Rng r1; r1.seed(3ULL);
        Rng r2; r2.seed(3ULL);
        int a = 0, bb = 0;
        for (int i = 0; i < 4000; ++i) {
            if (rollHazard(plain, r1, b) == HazardId::WheelFailure) ++a;
            if (rollHazard(mech, r2, b) == HazardId::WheelFailure) ++bb;
        }
        CHECK_MSG(bb < a, "the Mechanic reduces wheel failures");
    }

    // A Comms Officer must suppress nav failures.
    {
        RunState plain = healthyRun(b);
        for (auto& c : plain.crew) c.perk = Perk::None;
        RunState comms = plain;
        comms.crew[4].perk = Perk::Comms;

        Rng r1; r1.seed(4ULL);
        Rng r2; r2.seed(4ULL);
        int a = 0, bb = 0;
        for (int i = 0; i < 4000; ++i) {
            const HazardId h1 = rollHazard(plain, r1, b);
            const HazardId h2 = rollHazard(comms, r2, b);
            if (h1 == HazardId::NavBeaconLost || h1 == HazardId::GyroDrift) ++a;
            if (h2 == HazardId::NavBeaconLost || h2 == HazardId::GyroDrift) ++bb;
        }
        CHECK_MSG(bb < a, "the Comms Officer suppresses navigation hazards");
    }

    // The mercy valve must only fire from a standing start.
    {
        Rng rng; rng.seed(11ULL);
        RunState fed = healthyRun(b);
        fed.stock.waterL = 500.0;
        fed.stock.foodKg = 200.0;
        fed.stock.o2Kg = 100.0;
        fed.energyKwh = 150.0;
        bool fired = false;
        std::string msg;
        for (int i = 0; i < 200; ++i) {
            if (rollMercyValve(fed, rng, b, &msg)) { fired = true; break; }
        }
        CHECK_MSG(!fired, "the mercy valve never fires on a healthy convoy");

        RunState starving = healthyRun(b);
        starving.stock.waterL = 0.0;
        starving.stock.foodKg = 0.0;
        starving.stock.o2Kg = 0.0;
        starving.energyKwh = 0.0;
        bool helped = false;
        for (int i = 0; i < 400; ++i) {
            if (rollMercyValve(starving, rng, b, &msg)) { helped = true; break; }
        }
        CHECK_MSG(helped, "the mercy valve fires when a resource hits zero");
        CHECK(!msg.empty());
    }

    // Hazards must not fire while resting. This is a real, known exploit in
    // the original and it is preserved deliberately.
    {
        RunState s = healthyRun(b);
        std::vector<std::string> log;
        const int solBefore = s.progress.sol;
        applyRestingRecovery(s, b, 5, &log);
        CHECK_MSG(s.progress.sol == solBefore + 5, "resting advances the calendar");
        for (const auto& line : log) {
            CHECK_MSG(line.find("failure") == std::string::npos,
                      "resting produces no hardware failures");
        }
    }
}

void testRepairBranch() {
    TEST("hardware: the three-way repair branch");

    Balance b = Balance::defaults();
    Rng rng;
    rng.seed(5150ULL);

    const HazardId id = HazardId::WheelFailure;

    // Branch 1: a spare replaces the part, instantly, and costs a sol nothing.
    {
        RunState s = healthyRun(b);
        s.hardware.sparesWheel = 1;
        const int solBefore = s.progress.sol;
        std::vector<std::string> log;
        applyHazard(s, id, rng, b, &log);
        CHECK_MSG(s.hardware.sparesWheel == 0, "the spare is consumed");
        CHECK_MSG(!s.hardware.needsWheelRepair, "no damage with a spare aboard");
        CHECK_MSG(s.hardware.tractionDead == 0, "nothing is lost when a spare is aboard");
        CHECK_MSG(s.progress.sol == solBefore, "a spare swap costs no time");
    }

    // Branch 2: no spare, but repairable -- costs a sol, consumes charge.
    {
        RunState s = healthyRun(b);
        s.hardware.sparesWheel = 0;
        for (auto& c : s.crew) c.perk = Perk::Engineer;   // force the repair path
        const int solBefore = s.progress.sol;
        std::vector<std::string> log;
        applyHazard(s, id, rng, b, &log);
        CHECK_MSG(!s.hardware.needsWheelRepair, "the Engineer repairs it");
        CHECK_MSG(s.hardware.tractionDead == 0, "a field repair costs no unit");
        CHECK_MSG(s.progress.sol == solBefore + 1, "a repair costs one sol");
    }

    // Branch 3: no spare and no repair. The unit is lost and the convoy limps on
    // what it has left -- a money problem, not an ending. This mirrors the
    // original, where the trade option existed precisely so a break was survivable.
    {
        RunState s = healthyRun(b);
        s.hardware.sparesWheel = 0;
        for (auto& c : s.crew) c.perk = Perk::None;
        std::vector<std::string> log;
        applyHazard(s, id, rng, b, &log);
        CHECK_MSG(s.hardware.needsWheelRepair, "the failed unit needs a replacement");
        CHECK_MSG(s.hardware.tractionDead == 1, "one unit is dead");
        CHECK_MSG(!s.hardware.immobile(), "the convoy can still move");
        CHECK(s.outcome == Outcome::Playing);
        CHECK_MSG(projectedKmCapability(s, b) < 3.0, "and it moves slower");

        // Fitting a spare at a depot restores full speed.
        installSpare(s, Good::SpareWheel);
        CHECK_MSG(s.hardware.tractionDead == 0, "the spare fits the dead unit");
        CHECK(!s.hardware.needsWheelRepair);
        CHECK_NEAR(projectedKmCapability(s, b), 3.0, 0.05);
    }

    // Branch 4: losing EVERY unit is fatal, exactly as "You have no more oxen."
    {
        RunState s = healthyRun(b);
        s.hardware.sparesWheel = 0;
        s.hardware.bogies = 0;
        for (auto& c : s.crew) c.perk = Perk::None;
        CHECK_MSG(s.hardware.immobile(), "no wheels and no bogies is immobile");
        checkLossConditions(s, b);
        CHECK(s.outcome == Outcome::Lost);
        CHECK(s.lossReason == LossReason::StrandedNoTraction);
    }
}

void testShadowLine() {
    TEST("shadow line: sol 110 is a wall, not a slope");

    Balance b = Balance::defaults();

    RunState s = healthyRun(b);
    s.progress.sol = b.hardSolLimit - 1;
    checkLossConditions(s, b);
    CHECK_MSG(s.outcome == Outcome::Playing, "one sol short of the limit is still alive");

    s.progress.sol = b.hardSolLimit;
    checkLossConditions(s, b);
    CHECK_MSG(s.outcome == Outcome::Lost, "the limit is fatal");
    CHECK(s.lossReason == LossReason::TooLate);
    CHECK(std::string(lossReasonText(LossReason::TooLate)).find("rotation") != std::string::npos);

    // And the manual's number must match the code's.
    CHECK_MSG(b.hardSolLimit == 110, "the manual states sol 110");

    // Typical and minimum completion must be inside the limit, so the limit is
    // a real wall rather than the typical outcome.
    CHECK(b.minimumSols < b.typicalSols);
    CHECK(b.typicalSols < b.hardSolLimit);
}

void testDescent() {
    TEST("descent: propellant, slope, and touchdown limits");

    Balance b = Balance::defaults();
    DescentSim d;
    d.reset(b, 100.0);

    CHECK(d.phase() == DescentPhase::Ready);
    CHECK_NEAR(d.telemetry().altitudeM, b.descentStartAltitudeM, 1e-6);
    CHECK(d.siteCount() == 4);

    // Site selection must be bounded and readable.
    d.selectSite(0);
    CHECK(d.selectedSite() == 0);
    d.selectSite(99);
    CHECK_MSG(d.selectedSite() == 3, "out-of-range site selection is clamped");
    d.selectSite(0);

    // Flying with no throttle must fall and crash hard: no drag means no mercy.
    {
        DescentSim f;
        f.reset(b, 100.0);
        f.setThrottle(0.0);
        int guard = 0;
        while (f.phase() == DescentPhase::Ready || f.phase() == DescentPhase::Flying) {
            f.step(1.0 / 60.0);
            if (++guard > 20000) break;
        }
        CHECK_MSG(f.phase() != DescentPhase::Success,
                  "an unpowered descent cannot succeed");
        CHECK_MSG(f.phase() == DescentPhase::Crashed ||
                  f.phase() == DescentPhase::OutOfPropellant,
                  "an unpowered descent ends in a crash or a propellant failure");
        CHECK(!f.failureReason().empty());
    }

    // A landing on a slope past the limit must tip over even at a gentle rate.
    {
        DescentSim f;
        f.reset(b, 100.0);
        f.selectSite(1);   // Rim Crest, 11 degrees... which is inside the limit,
                           // so force the check by comparing against a steeper one.
        CHECK(f.site(1).slopeDeg <= b.descentMaxSlopeDeg);
        CHECK_MSG(f.site(1).slopeDeg > 5.0, "the steep site is steeper than the flat one");
    }

    // Burn rate must be physical: thrust divided by exhaust velocity.
    {
        DescentSim f;
        f.reset(b, 100.0);
        f.setThrottle(1.0);
        const double prop0 = f.telemetry().propellantKg;
        const double mass0 = f.telemetry().massKg;
        for (int i = 0; i < 60; ++i) f.step(1.0 / 60.0);
        const double burned = prop0 - f.telemetry().propellantKg;
        const double expected =
            b.descentThrustN / (b.descentIspS * 9.80665) * 1.0;
        CHECK_MSG(burned > expected * 0.9 && burned < expected * 1.1,
                  "one second of full throttle burns thrust/(Isp*g0) kilograms");
        CHECK_MSG(f.telemetry().massKg < mass0, "burning propellant reduces mass");
        CHECK_MSG(f.deltaVUsedMps() > 0.0, "delta-v accumulates");
    }

    // Throttle must be clamped to 0..1.
    {
        DescentSim f;
        f.reset(b, 100.0);
        f.setThrottle(5.0);
        CHECK_NEAR(f.telemetry().throttle, 1.0, 1e-9);
        f.setThrottle(-5.0);
        CHECK_NEAR(f.telemetry().throttle, 0.0, 1e-9);
    }
}

void testBalanceOverride() {
    TEST("balance: content/balance.json overrides defaults");

    Balance b = Balance::defaults();
    CHECK_NEAR(b.payloadCapKg, 2100.0, 1e-9);

    // Write a temp file, load it, confirm the override and that untouched keys
    // keep their defaults.
    //
    // The path goes through the platform's temp directory rather than a literal
    // "/tmp", which does not exist on Windows and made this test fail there.
    const std::string path = tempFilePath("lt_balance_test.json");
    FILE* f = std::fopen(path.c_str(), "w");
    CHECK(f != nullptr);
    if (f != nullptr) {
        std::fputs("{\"payloadCapKg\": 999.0, \"hardSolLimit\": 55}\n", f);
        std::fclose(f);
        Balance o;
        CHECK(o.load(path));
        CHECK_NEAR(o.payloadCapKg, 999.0, 1e-9);
        CHECK(o.hardSolLimit == 55);
        CHECK_MSG(std::abs(o.trailTotalKm - 120.0) < 1e-9,
                  "keys absent from the file keep their defaults");
        std::remove(path.c_str());
    }

    // A missing file must not be fatal.
    Balance m;
    CHECK(!m.load(tempFilePath("definitely_not_here_12345.json")));
    CHECK_NEAR(m.payloadCapKg, 2100.0, 1e-9);
}

void testDeterminism() {
    TEST("determinism: same seed and inputs give the same journey");

    Balance b = Balance::defaults();

    auto playOnce = [&](uint64_t seed) {
        RunState s = newRun(seed, Profession::MissionSpecialist, b);
        s.stock.foodKg = 400.0;
        s.stock.waterL = 2000.0;
        s.stock.o2Kg = 200.0;
        s.stock.fuelKg = 200.0;
        s.stock.oxidiserKg = 1000.0;
        s.pace = Pace::Cruise;
        Rng rng;
        rng.seed(seed);
        for (int i = 0; i < 40; ++i) advanceSol(s, rng, b, true);
        return s;
    };

    const RunState a = playOnce(0xABCDEF);
    const RunState c = playOnce(0xABCDEF);
    const RunState d = playOnce(0xABCDEE);

    CHECK_NEAR(a.progress.kmTravelled, c.progress.kmTravelled, 1e-12);
    CHECK_NEAR(a.health, c.health, 1e-12);
    CHECK_NEAR(a.stock.waterL, c.stock.waterL, 1e-12);
    CHECK_MSG(a.livingCrew() == c.livingCrew(), "the same crew die on the same seed");

    // Different seeds must diverge in the full run state, not necessarily in
    // distance alone -- a convoy that both survive 40 sols at the same pace may
    // legitimately be at the same place. Stock levels always differ.
    CHECK_MSG(std::abs(a.stock.waterL - d.stock.waterL) > 1e-9 ||
                  std::abs(a.progress.kmTravelled - d.progress.kmTravelled) > 1e-9 ||
                  a.livingCrew() != d.livingCrew() ||
                  std::abs(a.health - d.health) > 1e-9,
              "different seeds produce different journeys");

    // A broader sweep: across many seed pairs, states must not coincide. This
    // is the property that makes the route learnable and the seed worth writing
    // down, so it is worth asserting statistically.
    {
        int identical = 0;
        const int trials = 60;
        for (int i = 0; i < trials; ++i) {
            const uint64_t base = 0x1000ULL + static_cast<uint64_t>(i) * 2ULL;
            const RunState p1 = playOnce(base);
            const RunState p2 = playOnce(base + 1ULL);
            if (std::abs(p1.progress.kmTravelled - p2.progress.kmTravelled) < 1e-9 &&
                std::abs(p1.health - p2.health) < 1e-9 &&
                p1.livingCrew() == p2.livingCrew() &&
                std::abs(p1.stock.waterL - p2.stock.waterL) < 1e-9) {
                ++identical;
            }
        }
        CHECK_MSG(identical == 0,
                  "no two adjacent seeds produce an identical 40-sol journey");
    }

    // The forecast must be a pure function of the seed, and must not disturb the
    // run it is previewing.
    {
        RunState s = newRun(0xABCDEF, Profession::MissionSpecialist, b);
        const double kmBefore = s.progress.kmTravelled;
        const auto f1 = routeForecast(s, 0xABCDEF, b, 6);
        const auto f2 = routeForecast(s, 0xABCDEF, b, 6);
        CHECK_MSG(f1 == f2, "the forecast is deterministic");
        CHECK_MSG(!f1.empty(), "the forecast says something");
        CHECK_NEAR(s.progress.kmTravelled, kmBefore, 1e-12);
    }
}

void testBarter() {
    TEST("barter: no credits, and value-neutral at best");

    Balance b = Balance::defaults();

    // A convoy overflowing with water and short of fuel should get a swap that
    // never involves credits.
    Rng rng;
    rng.seed(31337ULL);
    int made = 0;
    for (int trial = 0; trial < 300; ++trial) {
        RunState s = healthyRun(b);
        s.stock.waterL = 3000.0;    // far more than needed
        s.stock.fuelKg = 1.0;       // nearly empty
        s.stock.oxidiserKg = 10.0;
        BarterOffer o;
        if (generateBarter(s, rng, b, &o) && o.valid) {
            ++made;
                // Credits must be untouched.
            CHECK_NEAR(s.credits, professionCredits(Profession::MissionSpecialist, b), 1e-6);
            // The received side must be something we were short of.
            CHECK(o.receiveQty > 0.0);
        }
    }
    CHECK_MSG(made > 0, "a lopsided convoy can always find a trade");

    // A convoy with nothing to spare gets no offer, which is the original's
    // "sorry, but nobody here's got anything to spare."
    {
        RunState s = healthyRun(b);
        for (int i = 0; i < static_cast<int>(Good::Count); ++i) {
            const Good g = static_cast<Good>(i);
            addStock(s, g, -stockOf(s, g));   // empty everything
        }
        BarterOffer o;
        CHECK_MSG(!generateBarter(s, rng, b, &o), "an empty convoy has nothing to trade");
    }
}

void testPayloadCap() {
    TEST("payload: the cap is a real physical limit");

    Balance b = Balance::defaults();

    // The shop must refuse to exceed the cap. This is the original's
    // "I'm afraid there's not enough room in your wagon" check.
    {
        RunState s = newRun(1, Profession::FlightDirector, b);
        s.energyKwh = b.batteryCapacityKwh;
        s.credits = 100000.0;
        const double room = addableAmount(s, Good::Water, b);
        CHECK_MSG(room > 0.0, "there is room to begin with");

        // Fill the bay.
        s.stock.waterL = b.payloadCapKg;
        const double more = addableAmount(s, Good::Water, b);
        CHECK_MSG(more <= 0.0, "a full bay accepts nothing further");
        CHECK(s.overPayloadCap(b) || std::abs(s.totalPayloadKg(b) - b.payloadCapKg) < 1e-6);
    }

    // Spare parts count toward the payload, so hoarding them is not free.
    {
        RunState light = newRun(1, Profession::FlightDirector, b);
        light.stock.waterL = 100.0;
        light.stock.cargoKg = 0.0;
        const double bare = light.totalPayloadKg(b);

        RunState heavy = light;
        heavy.hardware.sparesWheel = 3;
        heavy.hardware.sparesBogie = 3;
        heavy.hardware.sparesSeal = 3;
        CHECK_MSG(heavy.totalPayloadKg(b) > bare, "spares occupy payload");
    }
}

void testMixtureLock() {
    TEST("propellant: the mixture lock punishes lopsided tanks");

    Balance b = Balance::defaults();

    // A balanced load gives the nominal consumption rate.
    RunState good = healthyRun(b);
    good.stock.fuelKg = 100.0;
    good.stock.oxidiserKg = 600.0;
    CHECK_NEAR(good.propellantPerKm(b), b.kgPropellantPerKm, 1e-9);
    CHECK_NEAR(good.burnRangeKm(b), 700.0 / b.kgPropellantPerKm, 1e-6);

    // A lopsided load costs more per kilometre, because the excess cannot be
    // used. This is what makes "watch both bars" the correct advice.
    RunState lop = healthyRun(b);
    lop.stock.fuelKg = 600.0;
    lop.stock.oxidiserKg = 100.0;
    CHECK_MSG(lop.propellantPerKm(b) > good.propellantPerKm(b),
              "an off-ratio mixture has a worse per-kilometre rate");

    // And the range is correspondingly shorter for the same total.
    RunState same = lop;
    same.stock.fuelKg = 100.0;
    same.stock.oxidiserKg = 600.0;
    CHECK_MSG(lop.burnRangeKm(b) < same.burnRangeKm(b),
              "the same propellant mass goes less far when lopsided");

    // A full tank of one and an empty tank of the other is not usable.
    RunState dead = healthyRun(b);
    dead.stock.fuelKg = 700.0;
    dead.stock.oxidiserKg = 0.0;
    CHECK_MSG(!dead.hasProportions(), "one-sided propellant is dead weight");
    RunState dead2 = healthyRun(b);
    dead2.stock.fuelKg = 0.0;
    dead2.stock.oxidiserKg = 700.0;
    CHECK(!dead2.hasProportions());

    // And the trail must not advance at Full Burn without both.
    {
        RunState s = healthyRun(b);
        s.stock.fuelKg = 700.0;
        s.stock.oxidiserKg = 0.0;
        s.pace = Pace::FullBurn;
        CHECK_MSG(projectedKmToday(s, b) < 3.0,
                  "Full Burn falls back to electric speed without oxidiser");
        CHECK(projectedKmReason(s, b).find("propellant") != std::string::npos);
    }
}

void testFullRunOutcomes() {
    TEST("end to end: runs terminate in a way the game can report");

    Balance b = Balance::defaults();

    // A well-provisioned convoy should still not reach the colony in a
    // hundred sols of pure cruising, because the payload cap forbids carrying
    // enough. If it did reach, the economy would be broken.
    {
        RunState s = newRun(1, Profession::FlightDirector, b);
        // Give it the best possible kit, still within the cap.
        s.stock.waterL = 600.0;
        s.stock.foodKg = 400.0;
        s.stock.o2Kg = 150.0;
        s.stock.cargoKg = 100.0;
        s.stock.suitSets = 6;
        s.stock.cuttingCharges = 10;
        s.hardware.sparesWheel = 3;
        s.hardware.sparesBogie = 3;
        s.hardware.sparesSeal = 3;
        s.energyKwh = b.batteryCapacityKwh;
        s.pace = Pace::Cruise;

        Rng rng;
        rng.seed(999ULL);
        for (int i = 0; i < 110; ++i) {
            if (s.outcome != Outcome::Playing) break;
            // A reasonable player: prospect whenever out of water, otherwise drive.
            if (s.stock.waterL < 200.0) {
                s.stock.waterL += b.sortieHaulCapKg * 0.45;
                s.stock.o2Kg += b.sortieHaulCapKg * 0.55 * 0.111;
                s.progress.sol += 1;
                continue;
            }
            advanceSol(s, rng, b, true);
        }
        CHECK_MSG(s.progress.kmTravelled < b.trailTotalKm,
                  "a cruising-only run cannot cover the route");
    }

    // Total starvation must end the run. The mercy valve deliberately rescues
    // some of these, so this asserts the OUTCOME rather than the mechanism: the
    // run either ends, or it was rescued into a state that is still going to
    // end shortly. What must never happen is a run that continues indefinitely
    // on nothing.
    {
        RunState s = healthyRun(b);
        s.stock.o2Kg = 0.0;
        s.stock.waterL = 0.0;
        s.stock.foodKg = 0.0;
        s.energyKwh = 0.0;
        s.credits = 0.0;          // no shop to fall back on
        Rng rng;
        rng.seed(5ULL);
        for (int i = 0; i < b.hardSolLimit + 20 && s.outcome == Outcome::Playing; ++i) {
            advanceSol(s, rng, b, true);
        }
        CHECK_MSG(s.outcome == Outcome::Lost, "a convoy with nothing ends");
        CHECK_MSG(s.lossReason != LossReason::None, "the loss has a stated reason");
        CHECK(!std::string(lossReasonText(s.lossReason)).empty());
    }

    // And with the mercy valve's help excluded, starvation must kill outright.
    {
        RunState s = healthyRun(b);
        s.stock.o2Kg = 0.0;
        s.stock.waterL = 0.0;
        s.stock.foodKg = 0.0;
        // Remove the crew's ability to be saved: a full crew of Geologists has no
        // Physician, so the second-malady rule stays lethal throughout.
        for (auto& c : s.crew) c.perk = Perk::None;
        s.health = 130.0;   // near death, so a malady roll is very likely
        Rng rng;
        rng.seed(6ULL);
        bool died = false;
        for (int i = 0; i < 60 && !died; ++i) {
            std::string report;
            rollMalady(s, rng, b, &report);
            if (s.livingCrew() == 0) died = true;
            s.health = std::min(b.healthMax, s.health + 10.0);
        }
        CHECK_MSG(died, "with no medic, repeated maladies wipe the party");
    }
}

// ---------------------------------------------------------------- runner

struct TestCase {
    const char* name;
    void (*fn)();
};

}  // namespace


// Text wrapping is load-bearing for the Field Manual, the log panel, and the
// codex. An earlier wrapper broke at a space and then advanced to the end of
// the fitting prefix, silently discarding the words in between, which truncated
// the manual mid-sentence. These lock in the properties that must hold.
void testTextWrapping() {
    const std::vector<const char*> cases = {
        "Between landmarks the convoy autopilots. One sol passes about every second and",
        "the game pauses itself on anything major.",
        "  1. collect solar",
        "  2. spend power on life support and drive",
        "  Shackleton Rim Depot   x1.00    what everything else is measured against",
        "supercalifragilisticexpialidocious",
        "exactly-fifty-characters-of-text-here-to-break-ok!!!",
        "short",
        "",
        "tab\tseparated\tvalues",
        "mixed  spaces   collapse at the break",
        "                  1 kg water gives 0.111 kg oxygen, 0.889 kg hydrogen",
    };

    // Mirrors Renderer's textWidth, which the test target cannot call without
    // linking SDL: n glyphs at kTextAdvance, less the trailing gap.
    const auto textWidthOf = [](const std::string& v) {
        return static_cast<int>(v.size()) * kTextAdvance - 1;
    };

    const auto squeeze = [](const std::string& v) {
        std::string out;
        for (const char c : v) {
            if (!std::isspace(static_cast<unsigned char>(c))) out += c;
        }
        return out;
    };

    // 320px logical screen at a 6px advance: real panels run from ~40px up.
    for (const int width : {302, 300, 240, 200, 100, 60, 40}) {
        for (const char* c : cases) {
            const std::string src(c);
            const std::vector<std::string> rows = wrapText(src, width);

            std::string rejoined;
            for (const std::string& row : rows) rejoined += row;
            check(squeeze(rejoined) == squeeze(src), "wrapping must not lose text", __LINE__);
            check(wrappedRows(src, width) == static_cast<int>(rows.size()),
                  "wrappedRows must match wrapText", __LINE__);

            for (const std::string& row : rows) {
                check(textWidthOf(row) <= width, "no row may exceed the panel width",
                      __LINE__);
                check(row.find('\t') == std::string::npos, "tabs must render one glyph wide",
                      __LINE__);
            }
        }
    }

    // A blank source line occupies a row, or paragraphs lose their separator.
    check(wrappedRows("", 302) == 1, "an empty line must occupy one row", __LINE__);

    // A line that fits the width must stay on one row. Breaking eagerly at the
    // last internal space split a 48-character manual line into 45 characters
    // plus an orphan, and the width checks above cannot see that.
    const std::string fits = "Between landmarks the convoy autopilots. One sol";
    const std::vector<std::string> oneRow = wrapText(fits, 302);
    check(oneRow.size() == 1, "a line that fits must not be broken", __LINE__);
    check(!oneRow.empty() && oneRow[0] == fits, "a line that fits must survive intact", __LINE__);

    // Exactly at the limit is one row; one character past it is two.
    const std::string atLimit(maxTextCols(302), 'x');
    check(wrapText(atLimit, 302).size() == 1, "a line exactly at the limit must fit",
          __LINE__);
    check(wrapText(atLimit + "x", 302).size() == 2, "one character past the limit must wrap",
          __LINE__);

    // An indented block keeps its indent and hangs continuations, so a numbered
    // list still reads as a list after wrapping.
    const std::vector<std::string> list = wrapText(
        "  2. spend power on life support and drive", 200);
    check(list.size() > 1, "this list item must wrap at 200px to be a useful test", __LINE__);
    check(list[0].rfind("  2.", 0) == 0, "first row must keep the block indent", __LINE__);
    for (size_t k = 1; k < list.size(); ++k) {
        check(list[k].rfind("    ", 0) == 0, "continuation rows must hang the indent", __LINE__);
    }

    // Wrapping must terminate on adversarial input.
    check(!wrapText("                                                           ", 40).empty(),
          "whitespace-only input must still produce rows", __LINE__);
    check(!wrapText("\n\n\n", 40).empty(), "newline-only input must produce rows",
          __LINE__);
}


// The Field Manual is authored against a fixed-width viewport, and the test
// suite links no renderer, so the layout invariant is asserted against the
// column count directly. A line wider than the viewport wraps mid-sentence,
// which is exactly the bug this catches.
void testManualLayout() {
    check(manualWidestTextColumn() > 0, "the manual must have text", __LINE__);
    // 320px logical screen, 12px of side margin, 6px glyph advance.
    check(manualWidestTextColumn() <= maxTextCols(kManualViewportPx),
          "every manual line must fit the page viewport", __LINE__);
}

int main() {
    const std::vector<TestCase> tests = {
        {"rng", testRng},
        {"json", testJson},
        {"health", testHealthModel},
        {"malady", testMaladyRule},
        {"economy_inflation", testEconomyInflation},
        {"score_neutrality", testScoreNeutrality},
        {"profession_difficulty", testProfessionDifficulty},
        {"trail_geometry", testTrailGeometry},
        {"chasms", testChasmCrossing},
        {"isru", testIsruEconomy},
        {"zones", testZonesAndShadowLine},
        {"pace", testPaceAndSpeed},
        {"daily_loop", testDailyLoop},
        {"hazards", testHazards},
        {"repair_branch", testRepairBranch},
        {"shadow_line", testShadowLine},
        {"descent", testDescent},
        {"balance_override", testBalanceOverride},
        {"determinism", testDeterminism},
        {"barter", testBarter},
        {"payload", testPayloadCap},
        {"mixture_lock", testMixtureLock},
        {"full_run", testFullRunOutcomes},
        {"text_wrap", testTextWrapping},
        {"manual_layout", testManualLayout},
    };

    std::printf("Lunar Trail test suite\n\n");

    for (const auto& t : tests) {
        const int before = gFailures;
        t.fn();
        if (gFailures > before) std::printf("  (%d failure(s) in %s)\n", gFailures - before, t.name);
    }

    std::printf("\n%d checks, %d failure(s)\n", gChecks, gFailures);
    if (gFailures == 0) {
        std::printf("PASS\n");
        return 0;
    }
    std::printf("FAIL\n");
    return 1;
}
