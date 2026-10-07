#include "sim/trail.h"

#include <algorithm>
#include <cmath>

namespace lt {

const std::vector<Landmark>& trail() {
    static const std::vector<Landmark> t = [] {
        std::vector<Landmark> v;

        auto add = [&v](int i, const char* name, LandmarkKind k, double km, Zone z,
                        int store, const char* blurb) {
            Landmark l;
            l.index = i;
            l.name = name;
            l.kind = k;
            l.km = km;
            l.zone = z;
            l.storeIndex = store;
            l.blurb = blurb;
            v.push_back(l);
            return static_cast<int>(v.size()) - 1;
        };

        // 0 -- the depot you land at. Base prices, exactly like Independence.
        add(0, "Shackleton Rim Depot", LandmarkKind::Depot, 0.0, Zone::Sunlit, 0,
            "Landing site. Docked, pressurized, and fully stocked -- for now. "
            "Prices here are what you will pay everywhere else, plus markup.");

        // 1 -- first chasma. No guided crossing available: always a gamble.
        {
            const int i = add(1, "Argo Chasma", LandmarkKind::Chasm, 7.0, Zone::Sunlit, -1,
                              "A 9-metre break in the shelf, 40 metres across. "
                              "No support service operates here.");
            v[static_cast<size_t>(i)].chasmaDepthM = 9.0;
            v[static_cast<size_t>(i)].chasmaSpanM = 40.0;
            v[static_cast<size_t>(i)].guidedAvailable = false;
        }

        add(2, "Ilus Field", LandmarkKind::Waypoint, 14.0, Zone::Sunlit, -1,
            "A flat basalt plain on the sunlit rim. Beautiful, and utterly dry. "
            "Nothing to mine here and nothing to hide from.");

        add(3, "Relay Post Krill", LandmarkKind::Depot, 22.0, Zone::Transition, 1,
            "A single pressurised module on stilts. One technician, one price list, "
            "and it is already a quarter dearer than the depot.");

        {
            const int i = add(4, "De Gerlache Rift", LandmarkKind::Chasm, 31.0, Zone::Transition, -1,
                              "Eleven metres deep, and the far wall is undercut. "
                              "A guided crossing can be arranged.");
            v[static_cast<size_t>(i)].chasmaDepthM = 11.0;
            v[static_cast<size_t>(i)].chasmaSpanM = 65.0;
            v[static_cast<size_t>(i)].guidedAvailable = true;
        }

        add(5, "Faustini Rim", LandmarkKind::Waypoint, 39.0, Zone::Transition, -1,
            "Ridge line where sunlight and shadow meet. Half the sky is lit, half "
            "is not. This is where sensible convoys refuel.");

        add(6, "Relay Post Shoemaker", LandmarkKind::Depot, 48.0, Zone::Transition, 2,
            "Two modules and a fuel bowser. Half again what the depot charged, "
            "and the technician has seen you coming.");

        {
            const int i = add(7, "Haworth Break", LandmarkKind::Chasm, 57.0, Zone::Transition, -1,
                              "The widest crossing on the route, right at the shadow "
                              "lip. Ninety metres, and the far side is already dark. "
                              "No guided crossing.");
            v[static_cast<size_t>(i)].chasmaDepthM = 14.0;
            v[static_cast<size_t>(i)].chasmaSpanM = 90.0;
            v[static_cast<size_t>(i)].guidedAvailable = false;
        }

        add(8, "Levi Massif", LandmarkKind::Waypoint, 66.0, Zone::Psr, -1,
            "The first ground that has not seen the sun in two billion years. Ice "
            "under your feet and no power overhead. You are on the battery now, and "
            "the next depot is ten kilometres away.");

        add(9, "Relay Post Amundsen", LandmarkKind::Depot, 76.0, Zone::Psr, 3,
            "The last depot with power of its own, and it sits at the near lip "
            "of the long dark. Three quarters above depot prices. Whatever you "
            "leave here, you carry into the dark.");

        // 10 -- branch point. The Upper Route is longer but skips a chasma,
        // mirroring South Pass -> Fort Bridger (long) or Green River (short).
        {
            const int i = add(10, "Nobile Sill", LandmarkKind::Waypoint, 86.0, Zone::Transition, -1,
                              "The shelf road climbs out of the shadow onto the "
                              "terminator here. Some sun, some ice, and the last "
                              "easy charging on the route.");
            v[static_cast<size_t>(i)].branch = true;
            v[static_cast<size_t>(i)].altNext = 11;
        }
        {
            const int i = add(11, "Amundsen Chasma", LandmarkKind::Chasm, 90.0, Zone::Transition, -1,
                              "Direct route, at the shadow edge. Fourteen metres "
                              "down and no bottom you can see. A guided crossing "
                              "can be arranged.");
            v[static_cast<size_t>(i)].chasmaDepthM = 14.0;
            v[static_cast<size_t>(i)].chasmaSpanM = 55.0;
            v[static_cast<size_t>(i)].guidedAvailable = true;
        }
        add(12, "Upper Route", LandmarkKind::Waypoint, 96.0, Zone::Transition, -1,
            "The long way round over the sill, entirely in twilight. Slower, and "
            "you keep all of your hardware.");

        add(13, "Relay Post Gerlache", LandmarkKind::Depot, 104.0, Zone::Transition, 4,
            "The final depot, back in the light. Double depot pricing. Whatever "
            "you leave here, you carry the last sixteen kilometres.");

        {
            const int i = add(14, "Columbia Chasma", LandmarkKind::Chasm, 112.0, Zone::Transition, -1,
                              "The last crossing. Beyond it the ground falls away "
                              "into the colony basin. No guided crossing.");
            v[static_cast<size_t>(i)].chasmaDepthM = 12.0;
            v[static_cast<size_t>(i)].chasmaSpanM = 75.0;
            v[static_cast<size_t>(i)].guidedAvailable = false;
        }

        add(15, "Horizons Colony Site", LandmarkKind::Colony, 120.0, Zone::Sunlit, -1,
            "The site. Two foundations and a comms mast, and two hundred people "
            "waiting to find out whether you made it.");

        return v;
    }();
    return t;
}

const Landmark& landmarkAtOrBefore(double km) {
    const auto& t = trail();
    const Landmark* best = &t.front();
    for (const auto& l : t) {
        if (l.km <= km) best = &l; else break;
    }
    return *best;
}

double kmToNextLandmark(double km) {
    const auto& t = trail();
    for (const auto& l : t) {
        if (l.km > km) return l.km - km;
    }
    return 0.0;
}

Zone zoneAtKm(double km) {
    // Landmark zones define the segments between them. The last landmark's zone
    // wins the final stretch.
    const Landmark& cur = landmarkAtOrBefore(km);
    return cur.zone;
}

// ---------------------------------------------------------------- depots

int depotRechargeSolsNeeded(const RunState& s, const Balance& b) {
    const double missing = b.batteryCapacityKwh - s.energyKwh;
    if (missing <= 1.0) return 0;
    return static_cast<int>(std::ceil(missing / std::max(1.0, b.depotRechargeKwhPerSol)));
}

bool rechargeAtDepot(RunState& s, const Landmark& lm, const Balance& b,
                     std::vector<std::string>* log) {
    if (lm.storeIndex < 0) return false;
    const double missing = b.batteryCapacityKwh - s.energyKwh;
    if (missing <= 1.0) {
        if (log) log->push_back("Batteries already full. Nothing to charge.");
        return false;
    }
    const int sols = static_cast<int>(std::ceil(missing / std::max(1.0, b.depotRechargeKwhPerSol)));
    s.energyKwh = b.batteryCapacityKwh;
    s.progress.sol += sols;
    if (log) {
        char buf[160];
        std::snprintf(buf, sizeof(buf),
                      "Charged at %s. %d sol(s) parked on the pad; batteries full.",
                      lm.name.c_str(), sols);
        log->push_back(buf);
    }
    return true;
}

// ---------------------------------------------------------------- chasms

namespace {

// The original's breakpoints, reinterpreted as metres of chasma depth.
constexpr double kSafeDepthM   = 2.5;
constexpr double kSwampDepthM  = 3.0;

// The guide does the work and the guide does not negotiate -- preserved from
// the original, where the hired guide always picks for you, always crosses
// immediately, and never warns you about the current.
double guideAdjustedRisk(double risk) { return risk * 0.20; }

}  // namespace

std::vector<ChasmPlan> chasmOptions(const RunState& s, const Landmark& lm, const Balance& b) {
    std::vector<ChasmPlan> out;
    if (lm.kind != LandmarkKind::Chasm) return out;

    const double depth = lm.chasmaDepthM;
    const double span  = lm.chasmaSpanM;

    // --- 1. Ramp Traverse: the ford. Free, instant, risk by depth. ---
    {
        ChasmPlan p;
        p.method = ChasmMethod::Ramp;
        p.available = true;
        p.solsCost = 0;
        if (depth < kSafeDepthM) {
            p.risk = span > 70.0 ? 0.06 : 0.03;
            p.description = "Free and immediate. Low risk at this depth.";
        } else if (depth <= kSwampDepthM) {
            p.risk = 0.10;
            p.solsCost = 1;
            p.description = "Marginal. You will bog down and lose a sol jacking "
                            "the rig free, but lose nothing else.";
        } else {
            // Linear in depth beyond the swamp band, exactly as documented, but
            // on a scale where the deepest crossing on the route is a serious
            // gamble rather than a foregone conclusion.
            p.risk = std::min(0.55, 0.08 + (depth - kSwampDepthM) * 0.030);
            p.description = "Past the safe limit. Risk climbs with every metre "
                            "of depth, and a failure is expensive.";
        }
        out.push_back(p);
    }

    // --- 2. Grapple And Winch: the caulk/float. One sol, needs power. ---
    {
        ChasmPlan p;
        p.method = ChasmMethod::Winch;
        // ChasmPlan default-initialises `available` to false, so every branch that
        // can succeed must set it. This one did not, which left the winch
        // permanently unavailable: the manual documents it as a real option and
        // no run could ever take it.
        p.available = true;
        p.solsCost = 1;
        p.energyKwh = 40.0;
        p.cuttingCharges = 2;
        p.risk = std::min(0.45, 0.06 + span * 0.0030);
        p.description = "Anchor on the far side, winch the rig across. Costs a sol "
                        "and a lot of battery.";
        if (s.energyKwh < p.energyKwh + 20.0) {
            p.available = false;
            p.unavailableReason = "Not enough battery to run the winch.";
        } else if (s.stock.cuttingCharges < p.cuttingCharges) {
            p.available = false;
            p.unavailableReason = "No cutting charges to seat the anchors.";
        }
        out.push_back(p);
    }

    // --- 3. Hop Assist: not in the original. Costs propellant, risks the rig. ---
    {
        ChasmPlan p;
        p.method = ChasmMethod::Hop;
        p.available = true;
        p.solsCost = 0;
        // Delta-v scales with span; propellant cost follows from the burn table.
        const double dvKmps = 0.05 + span * 0.0016;
        p.costPropellantKg = dvKmps * 30.0;
        p.risk = std::min(0.38, 0.05 + span * 0.0025);
        p.description = "Burn across on the descent engine. Fast, and it does not "
                        "care about your wheels.";
        if (!s.hasProportions()) {
            p.available = false;
            p.unavailableReason = "No usable propellant. You need both fuel and oxidiser.";
        } else if (s.stock.propellantKg() < p.costPropellantKg) {
            p.available = false;
            p.unavailableReason = "Not enough propellant for the hop.";
        }
        out.push_back(p);
    }

    // --- 4. Guided Crossing: the ferry. Costs credits, may wait, lowest risk. ---
    {
        ChasmPlan p;
        p.method = ChasmMethod::Guided;
        p.available = lm.guidedAvailable;
        p.costCredits = 240.0;
        p.risk = 0.06;
        p.solsCost = 0;  // may wait up to 6 sols for a support window
        p.description = "A support crew flies a crossing rig over and back. "
                        "They may not be available for several sols.";
        if (!lm.guidedAvailable) {
            p.unavailableReason = "No support service operates at this crossing.";
        }
        out.push_back(p);
    }

    // --- 5. Local Guide: the Indian guide. Three suit sets, -80% risk. ---
    {
        ChasmPlan p;
        p.method = ChasmMethod::Guide;
        p.available = true;
        p.costSuitSets = 3;
        p.solsCost = 0;
        p.risk = guideAdjustedRisk(std::min(0.55, 0.15 + depth * 0.020));
        p.description = "A local guide picks the line and the timing. Cuts your "
                        "risk by 80%. Does not wait, and does not warn you.";
        if (s.stock.suitSets < p.costSuitSets) {
            p.available = false;
            p.unavailableReason = "You need 3 suit sets to pay the guide.";
        }
        out.push_back(p);
    }

    (void)b;
    return out;
}

ChasmResult resolveChasm(RunState& s, const Landmark& lm, const ChasmMethod method,
                         Rng& rng, const Balance& b) {
    ChasmResult r;
    const std::vector<ChasmPlan> opts = chasmOptions(s, lm, b);

    const ChasmPlan* plan = nullptr;
    for (const auto& p : opts) {
        if (p.method == method) { plan = &p; break; }
    }
    if (plan == nullptr || !plan->available) {
        r.headline = "Cannot cross";
        r.lines.push_back(plan != nullptr ? plan->unavailableReason
                                          : std::string("No such method."));
        return r;
    }

    // --- charge the costs ---
    s.credits -= plan->costCredits;
    s.stock.suitSets -= plan->costSuitSets;
    s.stock.cuttingCharges -= plan->cuttingCharges;
    s.energyKwh = std::max(0.0, s.energyKwh - plan->energyKwh);

    if (plan->costPropellantKg > 0.0) {
        // Burn proportionally so the mixture is preserved.
        const double total = s.stock.propellantKg();
        const double burn = std::min(plan->costPropellantKg, total);
        s.stock.fuelKg     = std::max(0.0, s.stock.fuelKg     - burn * s.stock.fuelKg     / total);
        s.stock.oxidiserKg = std::max(0.0, s.stock.oxidiserKg - burn * s.stock.oxidiserKg / total);
        s.progress.propellantBurnedKg += burn;
    }

    // --- rolling ---
    const bool success = !rng.chance(plan->risk);

    if (method == ChasmMethod::Ramp && lm.chasmaDepthM > kSafeDepthM &&
        lm.chasmaDepthM <= kSwampDepthM && success) {
        // The swamp band: no losses at all, just a day lost.
        r.headline = "Bogged down";
        r.lines.push_back("The far wall is undercut. You spend a sol jacking the "
                          "rig up and pinning timbers under the bogies.");
        r.lines.push_back("Nothing lost. Nothing gained.");
        r.solsLost = 1;
        r.succeeded = true;
        r.kmGained = lm.km - s.progress.kmTravelled;
        return r;
    }

    if (success) {
        r.succeeded = true;
        r.kmGained = lm.km - s.progress.kmTravelled;

        switch (method) {
            case ChasmMethod::Ramp:
                r.headline = "Across";
                r.lines.push_back("The rig walks down the graded face and out the "
                                  "other side. Uneventful.");
                break;
            case ChasmMethod::Winch:
                r.headline = "Across";
                r.lines.push_back("Anchors seat on the first attempt. The winch hauls "
                                  "you over in one long pull and the batteries end "
                                  "the day well down.");
                break;
            case ChasmMethod::Hop:
                r.headline = "Across";
                r.lines.push_back("You light the engine, hold attitude, and set down "
                                  "on the far shelf. Somewhere behind you a "
                                  "descent stage is falling.");
                break;
            case ChasmMethod::Guided:
                r.headline = "Across";
                r.lines.push_back("The support rig flies out, sets a cable, and takes "
                                  "your mass across. You pay what you owe and they "
                                  "are gone.");
                break;
            case ChasmMethod::Guide:
                r.headline = "Across";
                r.lines.push_back("The guide walks the rim for forty minutes without "
                                  "speaking, then points. You take the line they "
                                  "pointed at.");
                break;
            default:
                break;
        }
        return r;
    }

    // --- failure ---
    r.succeeded = false;

    // Severity scales with depth. Shallow failures are annoying; deep ones cost
    // you the run's worth of cargo. The guide's 80% reduction applies to losses
    // as well as risk, exactly as documented in the original's tuning notes.
    double severity = 0.22 + (lm.chasmaDepthM - 2.5) * 0.030;
    if (method == ChasmMethod::Guide) severity *= 0.20;
    severity = std::min(0.85, severity);

    r.cargoLostKg   = s.stock.cargoKg    * severity * rng.range(0.4, 1.0);
    r.waterLostL    = s.stock.waterL    * severity * rng.range(0.4, 1.0);
    r.o2LostKg      = s.stock.o2Kg      * severity * rng.range(0.4, 1.0);
    r.fuelLostKg    = s.stock.fuelKg    * severity * rng.range(0.3, 0.9);
    r.oxidiserLostKg= s.stock.oxidiserKg* severity * rng.range(0.3, 0.9);
    r.solsLost = 2;

    // Catastrophe: lose the rig entirely. Rare, and reserved for the deepest
    // crossings taken without preparation, so that when it happens it is
    // memorable rather than routine.
    r.catastrophic = severity > 0.55 && rng.chance(0.12);

    if (r.catastrophic) {
        r.headline = "Lost in the chasma";
        r.lines.push_back("The rim gives. There is a long time during which nothing "
                          "happens, and then nothing at all.");
        r.kmGained = 0.0;
        // Everyone not in the cab goes out the hatch. Some do not come back.
        const int living = s.livingCrew();
        r.crewLost = std::max(1, living - 1);
        for (int i = 0; i < r.crewLost; ++i) {
            for (auto& c : s.crew) {
                if (!c.dead) {
                    c.dead = true;
                    c.causeOfDeath = "the chasma crossing";
                    break;
                }
            }
        }
        s.hardware.tractionDead = s.hardware.tractionUnits;
        s.hardware.needsWheelRepair = true;
        s.credits = 0.0;
        s.stock.cargoKg = 0.0;
        s.stock.waterL *= 0.2;
        s.stock.o2Kg *= 0.2;
        s.stock.fuelKg *= 0.2;
        s.stock.oxidiserKg *= 0.2;
        return r;
    }

    switch (method) {
        case ChasmMethod::Ramp:
            r.headline = "Went in";
            r.lines.push_back("A traction unit goes through the regolith on the "
                              "downslope and the rig noses in.");
            break;
        case ChasmMethod::Winch:
            r.headline = "Anchor failure";
            r.lines.push_back("Two anchors seat and the third does not. The cable "
                              "goes somewhere you would rather it did not.");
            break;
        case ChasmMethod::Hop:
            r.headline = "Hard landing";
            r.lines.push_back("You come down short. The engine is fine. Everything "
                              "that was not bolted down is not fine.");
            break;
        case ChasmMethod::Guided:
            r.headline = "Cable parted";
            r.lines.push_back("The support rig loses the cable mid-span. They are "
                              "apologetic, and they are already gone.");
            break;
        case ChasmMethod::Guide:
            r.headline = "The guide was wrong";
            r.lines.push_back("The guide takes you in a direction that looked solid "
                              "and was not. Nobody is claiming to have said "
                              "otherwise.");
            break;
        default:
            r.headline = "Failed";
            break;
    }

    // A steep-enough failure in the Ramp method can still cost a crewmember.
    if (severity > 0.45 && rng.chance(0.30)) {
        for (auto& c : s.crew) {
            if (!c.dead) {
                c.dead = true;
                c.causeOfDeath = "the chasma crossing";
                r.crewLost = 1;
                break;
            }
        }
    }

    s.stock.cargoKg     = std::max(0.0, s.stock.cargoKg     - r.cargoLostKg);
    s.stock.waterL      = std::max(0.0, s.stock.waterL      - r.waterLostL);
    s.stock.o2Kg        = std::max(0.0, s.stock.o2Kg        - r.o2LostKg);
    s.stock.fuelKg      = std::max(0.0, s.stock.fuelKg      - r.fuelLostKg);
    s.stock.oxidiserKg  = std::max(0.0, s.stock.oxidiserKg  - r.oxidiserLostKg);
    s.progress.cargoLostKg += r.cargoLostKg;

    // A non-catastrophic failure still gets you across, slowly and expensively.
    r.kmGained = lm.km - s.progress.kmTravelled;
    s.hardware.needsBogieRepair = true;
    return r;
}

}  // namespace lt
