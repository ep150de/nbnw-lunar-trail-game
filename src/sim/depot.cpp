#include "sim/depot.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace lt {

const char* goodName(Good g) {
    switch (g) {
        case Good::Food:            return "Food";
        case Good::Water:           return "Water";
        case Good::O2:              return "Oxygen (O2)";
        case Good::Fuel:            return "Fuel (LCH4)";
        case Good::Oxidiser:        return "Oxidiser (LOX)";
        case Good::SuitSets:        return "MMOD Suit Sets";
        case Good::CuttingCharges:  return "Cutting Charges";
        case Good::SpareWheel:      return "Spare Traction Unit";
        case Good::SpareBogie:      return "Spare Drive Bogie";
        case Good::SpareSeal:       return "Spare Seal Kit";
        case Good::ColonyCargo:     return "Colony Cargo";
        default:                    return "?";
    }
}

const char* goodUnit(Good g) {
    switch (g) {
        case Good::Food:            return "kg";
        case Good::Water:           return "L";
        case Good::O2:              return "kg";
        case Good::Fuel:            return "kg";
        case Good::Oxidiser:        return "kg";
        case Good::SuitSets:        return "sets";
        case Good::CuttingCharges:  return "ea";
        case Good::SpareWheel:      return "ea";
        case Good::SpareBogie:      return "ea";
        case Good::SpareSeal:       return "ea";
        case Good::ColonyCargo:     return "kg";
        default:                    return "";
    }
}

double goodBasePrice(Good g, const Balance& b) {
    switch (g) {
        case Good::Food:            return b.basePriceFoodKg;
        case Good::Water:           return b.basePriceWaterL;
        case Good::O2:              return b.basePriceO2Kg;
        case Good::Fuel:            return b.basePriceFuelKg;
        case Good::Oxidiser:        return b.basePriceOxidiserKg;
        case Good::SuitSets:        return b.basePriceSuitSet;
        case Good::CuttingCharges:  return b.basePriceCuttingCharge;
        case Good::SpareWheel:      return b.basePriceWheel;
        case Good::SpareBogie:      return b.basePriceBogie;
        case Good::SpareSeal:       return b.basePriceSealKit;
        case Good::ColonyCargo:     return b.basePriceCargoKg;
        default:                    return 1.0;
    }
}

double goodPrice(Good g, int storeIndex, const Balance& b) {
    // Exactly the original's linear inflation: +25% per outpost, compounding
    // nowhere, flattening nowhere.
    return goodBasePrice(g, b) * (1.0 + b.outpostMarkupPerIndex * static_cast<double>(storeIndex));
}

double goodPoints(Good g, const Balance& b) {
    // Uniform by construction: points are ALWAYS price * pointsPerCredit, for
    // every good, with no special cases. This is the original's deliberate
    // economy-wide balance -- five credits converts to one point whether you
    // spend it, hold it, or buy anything at all -- and it is what makes
    // "restock early" a genuinely correct decision rather than an obvious one.
    //
    // Deriving it rather than hard-coding per-good values means the invariant
    // cannot be broken by adding a good or changing a price.
    return goodBasePrice(g, b) * b.pointsPerCredit;
}

int goodCap(Good g, const Balance& b) {
    switch (g) {
        case Good::SpareWheel:
        case Good::SpareBogie:
        case Good::SpareSeal:
            return b.maxSparesPerType;   // the wheel/axle/tongue limit of 3
        case Good::SuitSets:
            return b.maxSuitSets;
        case Good::CuttingCharges:
            return b.maxCuttingCharges;
        default:
            return -1;  // uncapped, bounded only by credits and payload
    }
}

double stockOf(const RunState& s, Good g) {
    switch (g) {
        case Good::Food:           return s.stock.foodKg;
        case Good::Water:          return s.stock.waterL;
        case Good::O2:             return s.stock.o2Kg;
        case Good::Fuel:           return s.stock.fuelKg;
        case Good::Oxidiser:       return s.stock.oxidiserKg;
        case Good::SuitSets:       return static_cast<double>(s.stock.suitSets);
        case Good::CuttingCharges: return static_cast<double>(s.stock.cuttingCharges);
        case Good::SpareWheel:     return static_cast<double>(s.hardware.sparesWheel);
        case Good::SpareBogie:     return static_cast<double>(s.hardware.sparesBogie);
        case Good::SpareSeal:      return static_cast<double>(s.hardware.sparesSeal);
        case Good::ColonyCargo:    return s.stock.cargoKg;
        default:                   return 0.0;
    }
}

namespace {
// The as-built bogie complement. Hard-coded rather than read from Balance
// because a RunState does not carry its balance struct, and a value that
// silently drifted would let a depot conjure bogies out of nothing.
constexpr int kBogieComplement = 3;
}  // namespace

// Fitting a spare part to hardware that had failed. Called by the shop, since
// that is the only place the player can both obtain and fit a part.
void installSpare(RunState& s, Good g) {
    switch (g) {
        case Good::SpareWheel:
            if (s.hardware.needsWheelRepair && s.hardware.tractionDead > 0) {
                --s.hardware.tractionDead;
                if (s.hardware.tractionDead == 0) s.hardware.needsWheelRepair = false;
            }
            break;
        case Good::SpareBogie:
            if (s.hardware.needsBogieRepair && s.hardware.bogies < kBogieComplement) {
                s.hardware.bogies += 1;
                if (s.hardware.bogies >= kBogieComplement) s.hardware.needsBogieRepair = false;
            }
            break;
        default:
            break;
    }
}

void addStock(RunState& s, Good g, double amount) {
    switch (g) {
        case Good::Food:           s.stock.foodKg += amount; break;
        case Good::Water:          s.stock.waterL += amount; break;
        case Good::O2:             s.stock.o2Kg += amount; break;
        case Good::Fuel:           s.stock.fuelKg += amount; break;
        case Good::Oxidiser:       s.stock.oxidiserKg += amount; break;
        case Good::SuitSets:       s.stock.suitSets += static_cast<int>(std::lround(amount)); break;
        case Good::CuttingCharges: s.stock.cuttingCharges += static_cast<int>(std::lround(amount)); break;
        case Good::SpareWheel:     s.hardware.sparesWheel += static_cast<int>(std::lround(amount)); break;
        case Good::SpareBogie:     s.hardware.sparesBogie += static_cast<int>(std::lround(amount)); break;
        case Good::SpareSeal:      s.hardware.sparesSeal += static_cast<int>(std::lround(amount)); break;
        case Good::ColonyCargo:    s.stock.cargoKg += amount; break;
        default: break;
    }
}

double addableAmount(const RunState& s, Good g, const Balance& b) {
    const int cap = goodCap(g, b);
    if (cap >= 0) {
        return std::max(0.0, static_cast<double>(cap) - stockOf(s, g));
    }
    // Payload-limited. Mass per unit, excluding spares which are counted in
    // totalPayloadKg already.
    double kgPerUnit = 1.0;
    switch (g) {
        case Good::Food:        kgPerUnit = 1.0; break;
        case Good::Water:       kgPerUnit = 1.0; break;
        case Good::O2:          kgPerUnit = 1.0; break;
        case Good::Fuel:        kgPerUnit = 1.0; break;
        case Good::Oxidiser:    kgPerUnit = 1.0; break;
        case Good::SuitSets:    kgPerUnit = 18.0; break;
        case Good::CuttingCharges: kgPerUnit = 1.2; break;
        case Good::ColonyCargo: kgPerUnit = 1.0; break;
        default: kgPerUnit = 0.0; break;
    }
    if (kgPerUnit <= 0.0) return 0.0;

    const double used = s.totalPayloadKg(b);
    const double room = b.payloadCapKg - used;
    if (room <= 0.0) return 0.0;
    return std::max(0.0, room / kgPerUnit);
}

double buyCost(RunState& s, Good g, double qty, int storeIndex, const Balance& b) {
    (void)s;
    if (qty <= 0.0) return 0.0;
    return qty * goodPrice(g, storeIndex, b);
}

void buy(RunState& s, Good g, double qty, int storeIndex, const Balance& b) {
    if (qty <= 0.0) return;
    const double cost = buyCost(s, g, qty, storeIndex, b);
    if (cost > s.credits) return;
    s.credits -= cost;
    addStock(s, g, qty);
    // Fitting a spare to failed hardware is the point of carrying one.
    if (g == Good::SpareWheel || g == Good::SpareBogie) installSpare(s, g);
}

double sellValue(RunState& s, Good g, double qty, int storeIndex, const Balance& b) {
    (void)s;
    (void)storeIndex;
    if (qty <= 0.0) return 0.0;
    // Outposts buy back at depot price less ten percent, i.e. you lose the
    // markup. Selling is never a good idea, exactly as in the original.
    return qty * goodBasePrice(g, b) * 0.9;
}

void sell(RunState& s, Good g, double qty, int storeIndex, const Balance& b) {
    const double have = stockOf(s, g);
    const double give = std::min(qty, have);
    if (give <= 0.0) return;
    addStock(s, g, -give);
    s.credits += sellValue(s, g, give, storeIndex, b);
}

// ---------------------------------------------------------------- jettison

double carriedMass(const RunState& s, const Balance& b) { return s.totalPayloadKg(b); }

double payloadRoom(const RunState& s, const Balance& b) {
    return b.payloadCapKg - s.totalPayloadKg(b);
}

double jettison(RunState& s, Good g, double kg) {
    if (kg <= 0.0) return 0.0;
    const double have = stockOf(s, g);
    const double drop = std::min(kg, have);
    if (drop <= 0.0) return 0.0;
    addStock(s, g, -drop);
    return drop;
}

double jettisonToFit(RunState& s, const Balance& b) {
    // Thrown away in this order. Colony cargo first because it is the only thing
    // that is not keeping anybody alive; then propellant, which is heavy and
    // only matters if you intend to burn it. Water, oxygen, food and the spares
    // are never dumped automatically -- a player who wants that does it by hand.
    static const Good kDroppable[] = {
        Good::ColonyCargo, Good::Oxidiser, Good::Fuel, Good::CuttingCharges,
    };
    double dropped = 0.0;
    for (const Good g : kDroppable) {
        double over = -payloadRoom(s, b);
        if (over <= 0.0) break;
        dropped += jettison(s, g, over);
    }
    return dropped;
}

// ---------------------------------------------------------------- barter

bool generateBarter(RunState& s, Rng& rng, const Balance& b, BarterOffer* out) {
    if (out == nullptr) return false;
    *out = BarterOffer{};

    // The player must be overflowing with something and short of something else.
    // Anything they hold near zero they will need anyway; anything they hold
    // above a comfortable reserve is fair game to trade away.
    struct Pool { Good g; double surplus; double value; };
    std::vector<Pool> surplus;
    std::vector<Pool> shortlist;

    for (int i = 0; i < static_cast<int>(Good::Count); ++i) {
        const Good g = static_cast<Good>(i);
        const double have = stockOf(s, g);
        const double value = have * goodBasePrice(g, b);

        const double want = [&] {
            switch (g) {
                case Good::Food:           return b.foodPerCrewKg * s.livingCrew() * 8.0;
                case Good::Water:          return b.waterPerCrewKg * s.livingCrew() * 8.0;
                case Good::O2:             return b.o2PerCrewKg * s.livingCrew() * 8.0;
                case Good::Fuel:           return 60.0;
                case Good::Oxidiser:       return 360.0;
                case Good::SuitSets:       return 6.0;
                case Good::CuttingCharges: return 12.0;
                case Good::SpareWheel:     return 1.0;
                case Good::SpareBogie:     return 1.0;
                case Good::SpareSeal:      return 1.0;
                case Good::ColonyCargo:    return 20.0;
                default:                   return 0.0;
            }
        }();

        if (have > want * 1.6 && value > 30.0) {
            surplus.push_back({g, have - want, value});
        } else if (have < want * 0.75) {
            shortlist.push_back({g, want - have, value});
        }
    }

    if (surplus.empty() || shortlist.empty()) {
        // "Sorry, but nobody here's got <item> to spare."
        return false;
    }

    const Pool& give = surplus[rng.below(static_cast<uint32_t>(surplus.size()))];
    const Pool& get  = shortlist[rng.below(static_cast<uint32_t>(shortlist.size()))];

    // Match on value, then round in the NPC's favour -- which is why barter is
    // value-neutral at best.
    const double giveQty = std::min(give.surplus, get.value / goodBasePrice(give.g, b));
    if (giveQty <= 0.01) return false;

    const double getQty = std::floor(giveQty * goodBasePrice(give.g, b) /
                                     goodBasePrice(get.g, b));
    if (getQty <= 0.01) return false;

    addStock(s, give.g, -giveQty);
    addStock(s, get.g, getQty);

    out->give = give.g;
    out->giveQty = giveQty;
    out->receive = get.g;
    out->receiveQty = getQty;
    out->valid = true;

    char buf[192];
    std::snprintf(buf, sizeof(buf), "Traded %.0f %s for %.0f %s.", giveQty,
                  goodUnit(give.g), getQty, goodUnit(get.g));
    out->text = buf;
    return true;
}

// ---------------------------------------------------------------- scoring

namespace {

double healthPoints(const RunState& s, const Balance& b) {
    const HealthBand band = s.healthBand();
    switch (band) {
        case HealthBand::Good:     return b.pointsPerSurvivorGood;
        case HealthBand::Fair:     return b.pointsPerSurvivorFair;
        case HealthBand::Poor:     return b.pointsPerSurvivorPoor;
        case HealthBand::VeryPoor:
        case HealthBand::Dying:    return b.pointsPerSurvivorVeryPoor;
    }
    return 0.0;
}

const char* rankFor(double score) {
    if (score >= 3000.0) return "Colony Founder";
    if (score >= 1500.0) return "Trailblazer";
    if (score >= 500.0)  return "Expeditionary";
    return "Trainee";
}

}  // namespace

ScoreBreakdown scoreRun(const RunState& s, const Balance& b) {
    ScoreBreakdown sc;
    sc.survivors = healthPoints(s, b) * static_cast<double>(s.livingCrew());
    sc.cargo = s.stock.cargoKg * b.pointsPerCargoKg;
    // Priced per good, not per "spare": a bogie is twice a traction unit and a
    // seal kit is a little under one, so a flat per-spare figure quietly made
    // carrying the wrong spare worth more than carrying the right one.
    sc.spares = static_cast<double>(s.hardware.sparesWheel) * goodPoints(Good::SpareWheel, b) +
                static_cast<double>(s.hardware.sparesBogie) * goodPoints(Good::SpareBogie, b) +
                static_cast<double>(s.hardware.sparesSeal) * goodPoints(Good::SpareSeal, b);
    sc.suitSets = static_cast<double>(s.stock.suitSets) * goodPoints(Good::SuitSets, b);
    sc.credits = s.credits * b.pointsPerCredit;
    sc.subtotal = sc.survivors + sc.cargo + sc.spares + sc.suitSets + sc.credits;
    sc.multiplier = professionScoreMult(s.profession, b);
    sc.total = sc.subtotal * sc.multiplier;
    sc.rank = rankFor(sc.total);
    return sc;
}

}  // namespace lt
