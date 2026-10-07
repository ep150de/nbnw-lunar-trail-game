// The outpost economy.
//
// Two properties are copied from the original without alteration, because they
// are what make the game hard:
//
//   1. Credits are the only non-renewable resource. There is no way to earn
//      them. Barter can never produce credits.
//   2. Every outpost applies `base * (1 + 0.25 * index)`. Exactly linear,
//      exactly 25%. Buying late is therefore a pure, predictable, compounding
//      cost.
//
// The original went further and balanced the whole economy so that one point
// equals five dollars whether you spent the money or held it. That property is
// preserved here: pointsPerCredit = 0.2, and every good's score contribution
// is set so that converting credits into goods is score-neutral at best.

#pragma once

#include <string>
#include <vector>

#include "core/rng.h"
#include "sim/balance.h"
#include "sim/convoy.h"

namespace lt {

enum class Good {
    Food, Water, O2, Fuel, Oxidiser,
    SuitSets, CuttingCharges,
    SpareWheel, SpareBogie, SpareSeal,
    ColonyCargo,
    Count
};

const char* goodName(Good g);
const char* goodUnit(Good g);
// Price at the depot (index 0), in credits. Index 1..n multiply by markup.
double goodBasePrice(Good g, const Balance& b);
double goodPrice(Good g, int storeIndex, const Balance& b);
// Score contributed by one unit, mirroring the original's per-item scoring.
double goodPoints(Good g, const Balance& b);
// The hard cap on this good, or -1 for uncapped.
int goodCap(Good g, const Balance& b);

double stockOf(const RunState& s, Good g);
void   addStock(RunState& s, Good g, double amount);
// Fits a spare part to hardware that had failed in the field.
void   installSpare(RunState& s, Good g);
// How much of `g` fits, given the payload cap. Returns what can actually be
// added; this is the original's "not enough room in your wagon" check.
double addableAmount(const RunState& s, Good g, const Balance& b);

// Convenience wrappers used all over the shop UI.
double buyCost(RunState& s, Good g, double qty, int storeIndex, const Balance& b);
void   buy(RunState& s, Good g, double qty, int storeIndex, const Balance& b);
double sellValue(RunState& s, Good g, double qty, int storeIndex, const Balance& b);
void   sell(RunState& s, Good g, double qty, int storeIndex, const Balance& b);

// ---- jettison ----
//
// The original enforced a wagon capacity and simply refused to let you buy past
// it, which meant the player had to decide what to leave behind at the depot.
// Once the route is underway and the bay is full of ice you have no credits for,
// there has to be a way to dump mass in the field. This is that, and it is
// deliberately blunt: you choose a good and a quantity, and it goes.

// Mass carried as loose cargo, in kilograms.
double carriedMass(const RunState& s, const Balance& b);
// How much room is left, in kilograms. Negative when over the cap.
double payloadRoom(const RunState& s, const Balance& b);

// Discards up to `kg` of `g`. Returns how much was actually discarded.
double jettison(RunState& s, Good g, double kg);

// Discards cargo automatically, keeping life-support consumables, until the
// convoy is under the cap. Returns how much went over the side.
double jettisonToFit(RunState& s, const Balance& b);

// ---- barter ----
//
// Available between landmarks at a cost of one sol, as in the original. The
// counterparty proposes a swap; there is no negotiation; credits are never on
// the table. Offers are value-neutral at best and usually favour the NPC, which
// is exactly how the original's trade module felt.

struct BarterOffer {
    Good give = Good::Food;
    double giveQty = 0.0;
    Good receive = Good::Water;
    double receiveQty = 0.0;
    bool valid = false;
    std::string text;
};

// Returns false when nobody has anything to trade today ("Sorry, but nobody
// here's got <item> to spare.")
bool generateBarter(RunState& s, Rng& rng, const Balance& b, BarterOffer* out);

// Scoring.
struct ScoreBreakdown {
    double survivors = 0.0;
    double cargo = 0.0;
    double spares = 0.0;
    double suitSets = 0.0;
    double credits = 0.0;
    double subtotal = 0.0;
    double multiplier = 1.0;
    double total = 0.0;
    const char* rank = "";
};

ScoreBreakdown scoreRun(const RunState& s, const Balance& b);

}  // namespace lt
