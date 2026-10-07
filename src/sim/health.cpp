#include "sim/health.h"

#include <algorithm>
#include <cmath>

namespace lt {

namespace {

// Ailments and their relative likelihood, weighted toward the ones that make
// narrative sense at a given moment. Injuries are rarer and slower to recover.
Malady pickMalady(Rng& rng, bool injury) {
    if (injury) {
        switch (rng.rangeInt(0, 4)) {
            case 0: return Malady::Fracture;
            case 1: return Malady::Sprain;
            case 2: return Malady::Crush;
            default: return Malady::Puncture;
        }
    }
    switch (rng.rangeInt(0, 5)) {
        case 0: return Malady::Exhaustion;
        case 1: return Malady::Exhaustion;
        case 2: return Malady::Hypoxia;
        case 3: return Malady::Toxicity;
        default: return Malady::DustPneumoconiosis;
    }
}

double paceMod(Pace p, const Balance& b) {
    switch (p) {
        case Pace::Cruise:   return b.paceHealthSteady;
        case Pace::Sprint:   return b.paceHealthStrenuous;
        case Pace::FullBurn: return b.paceHealthGrueling;
    }
    return 0.0;
}

double rationMod(Rations r, const Balance& b) {
    switch (r) {
        case Rations::Full:      return b.rationHealthFull;
        case Rations::Reduced:   return b.rationHealthReduced;
        case Rations::Emergency: return b.rationHealthEmergency;
    }
    return 0.0;
}

}  // namespace

HealthBand applyDailyHealth(RunState& s, const HealthMods& mods, const Balance& b) {
    // ORDER MATTERS, and it is the original's order:
    //   1. natural recovery, as a 10% decrement
    //   2. then add every modifier for the day
    s.health = s.health * b.healthRecoveryFraction;
    s.health += mods.total();

    if (s.health < 0.0) s.health = 0.0;
    if (s.health > b.healthMax) s.health = b.healthMax;
    return s.healthBand();
}

HealthMods baseDailyMods(const RunState& s, const Balance& b) {
    HealthMods m;
    m.pace   = paceMod(s.pace, b);
    m.rations= rationMod(s.rations, b);

    const int crew = s.livingCrew();

    // Thermal. In a shadowed region with the sun down, the cab loses heat and
    // there is no ambient warmth to draw on. In sunlight it is a smaller drain.
    if (s.progress.zone == Zone::Psr)      m.thermal += 4.0;
    else if (s.progress.zone == Zone::Transition) m.thermal += 1.5;
    else                                    m.thermal += 0.5;

    // Maladies drag on the whole party: someone in pain slows everyone down
    // and needs the cabin heater turned up for them.
    m.eventDamage += 1.5 * static_cast<double>(s.maladiedCrew());

    // Shortages. These are the ones that actually kill runs, and they are meant
    // to. A zero on any of the big three is a crisis, not a warning.
    if (crew > 0) {
        if (s.stock.o2Kg <= 0.0)    m.o2Shortage += 30.0;
        if (s.stock.waterL <= 0.0)  m.waterShortage += 14.0;
        if (s.stock.foodKg <= 0.0)  m.foodShortage += 9.0;
    }

    // Power. On reserve the cabin is cold and the crew is not sleeping, which
    // is serious but survivable -- the death comes from what happens NEXT, not
    // from the brownout itself. Weighted to be a slow squeeze rather than a
    // cliff, because the interesting failure is running the batteries flat in
    // the dark with a hundred kilometres still to cover.
    const double daily = b.energyPerCrewKwh * static_cast<double>(crew) +
                         b.driveEnergyCruiseKwh;
    if (s.energyKwh <= 0.0) {
        m.powerFailure += 9.0;
    } else if (s.energyKwh < daily * 0.75) {
        m.powerFailure += 3.0;
    }

    // Regolith dust. Every sol outdoors is a small insult to the lungs.
    if (s.progress.zone == Zone::Sunlit) m.eventDamage += 1.0;

    return m;
}

bool rollMalady(RunState& s, Rng& rng, const Balance& b, std::string* report) {
    if (s.livingCrew() == 0) return false;

    // Documented band: odds run from 0% at perfect health to 40% at worst.
    const double frac = std::min(1.0, std::max(0.0, s.health / b.healthMax));
    const double chance = b.illnessChanceMax * frac;
    if (!rng.chance(chance)) return false;

    // The person and the condition are both chosen at random, as documented.
    std::vector<size_t> candidates;
    for (size_t i = 0; i < s.crew.size(); ++i) {
        if (!s.crew[i].dead) candidates.push_back(i);
    }
    if (candidates.empty()) return false;

    const size_t pick = candidates[rng.below(static_cast<uint32_t>(candidates.size()))];
    CrewMember& m = s.crew[pick];

    // THE RULE: already afflicted plus a new malady is fatal.
    if (m.maladied()) {
        const bool saved = s.hasPhysician();
        if (saved) {
            // The Physician's one intervention: the crew survives, but the
            // second malady still replaces the first and the clock restarts.
            m.malady = pickMalady(rng, false);
            m.recoverSol = maladyRecoverySols(m.malady, b);
            if (report) {
                *report = m.name + " took a second malady. " + [&] {
                    const CrewMember& doc = *std::min_element(
                        s.crew.begin(), s.crew.end(),
                        [](const CrewMember& x, const CrewMember& y) {
                            return x.perk == Perk::Physician && y.perk != Perk::Physician;
                        });
                    return std::string(doc.name) + " pulled them through. Barely.";
                }();
            }
        } else {
            m.dead = true;
            m.causeOfDeath = std::string("a second ") + maladyName(m.malady);
            m.malady = Malady::None;
            if (report) {
                *report = m.name + " was already suffering and could not take another. "
                                  "Nobody aboard knew what to do.";
            }
        }
        return true;
    }

    // Injuries are rarer than ailments.
    const bool injury = rng.chance(0.28);
    m.malady = pickMalady(rng, injury);
    m.recoverSol = maladyRecoverySols(m.malady, b);
    if (report) {
        *report = m.name + " is suffering from " + maladyName(m.malady) + ". (" +
                  std::to_string(m.recoverSol) + " sols to recover.)";
    }
    return true;
}

void tickRecovery(RunState& s, const Balance& b, std::vector<std::string>* log) {
    for (auto& m : s.crew) {
        if (m.dead || m.malady == Malady::None) continue;
        m.recoverSol -= 1;
        if (m.recoverSol <= 0) {
            m.malady = Malady::None;
            m.recoverSol = 0;
            if (log != nullptr) log->push_back(m.name + " is well again.");
        }
    }
    (void)b;
}

void checkLossConditions(RunState& s, const Balance& b) {
    if (s.outcome != Outcome::Playing) return;

    if (s.livingCrew() == 0) {
        s.outcome = Outcome::Lost;
        s.lossReason = LossReason::PartyDead;
        return;
    }

    // Health at 140 is the death threshold, not merely "as bad as it gets". The
    // original's manual is explicit: "140 or more: remaining party members all
    // die within a few days". Without this the party can sit pinned at 140
    // indefinitely, which is both wrong and a way to survive that the design
    // never intended.
    if (s.health >= b.healthMax) {
        for (auto& c : s.crew) {
            if (!c.dead) {
                c.dead = true;
                c.causeOfDeath = "the state the convoy was in by the time it ended";
            }
        }
        s.outcome = Outcome::Lost;
        s.lossReason = LossReason::PartyDead;
        return;
    }

    // The shadow line. Past the hard limit the next crew rotation window closes
    // and the convoy is a permanent monument. This is the lunar answer to the
    // original's blizzard-in-the-mountains wall.
    if (s.progress.sol >= b.hardSolLimit) {
        s.outcome = Outcome::Lost;
        s.lossReason = LossReason::TooLate;
        return;
    }

    // Stranded in the dark with no batteries: a slow death, but a death.
    if (s.energyKwh <= 0.0 && s.progress.zone == Zone::Psr) {
        if (s.health >= b.healthMax) {
            s.outcome = Outcome::Lost;
            s.lossReason = LossReason::StrandedNoPower;
            return;
        }
    }

    // No drive at all. Matches the original's "You have no more oxen." A single
    // dead wheel is not fatal -- the convoy limps -- but losing all of them, or
    // all the drive bogies, is.
    if (s.hardware.immobile()) {
        s.outcome = Outcome::Lost;
        s.lossReason = LossReason::StrandedNoTraction;
        return;
    }
}

}  // namespace lt
