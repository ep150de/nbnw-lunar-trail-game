#include "game/descent.h"

#include <algorithm>
#include <cmath>

namespace lt {

namespace {
constexpr double kG0 = 9.80665;
}

void DescentSim::reset(const Balance& b, double descentCargoKg) {
    bal_ = b;
    phase_ = DescentPhase::Ready;
    failure_.clear();
    cargoDamage_ = 0.0;
    deltaVUsed_ = 0.0;
    propellantUsed_ = 0.0;
    impactVs_ = 0.0;
    impactHs_ = 0.0;

    dryMass_ = b.descentDryMassKg;
    cargoMass_ = descentCargoKg;
    payloadMass_ = cargoMass_;
    maxThrust_ = b.descentThrustN;
    isp_ = b.descentIspS;
    exhaustVel_ = isp_ * kG0;
    gMoon_ = b.lunarGM3 / (1737.4 * 1737.4);   // GM / R^2 for the Moon
    maxTiltDeg_ = b.descentMaxTiltDeg;

    t_ = DescentTelemetry{};
    t_.altitudeM = b.descentStartAltitudeM;
    t_.vsMps = b.descentStartVsMps;
    t_.hsMps = b.descentStartHsMps;
    t_.propellantKg = b.descentPropellantKg;
    t_.throttle = 0.0;
    t_.tiltDeg = 0.0;

    const double m0 = dryMass_ + cargoMass_ + t_.propellantKg;
    t_.massKg = m0;

    x_ = 0.0;
    y_ = 0.0;
    tiltRad_ = 0.0;

    // The south pole rim is a shelf of overlapping crater walls. These are four
    // plausible spots and they are genuinely different problems: the flat one is
    // easy if you can null your drift, the slope needs you to kill horizontal
    // velocity before you care about vertical.
    sites_ = {
        {"Shelf Flat",      3.0,   0.0,  "Level ground, good light. Nothing to excuse you."},
        {"Rim Crest",      11.0,   0.0,  "Steep but aligned with the approach. Tipover risk."},
        {"Terminal Drop",   6.0,  90.0,  "Flat but ninety metres long. You must null drift first."},
        {"Shadowed Apron",  5.0,  40.0,  "In shadow. Good ground, and no light to see it by."},
    };
    siteIndex_ = 0;
}

void DescentSim::setThrottle(double v) {
    if (phase_ != DescentPhase::Flying && phase_ != DescentPhase::Ready) return;
    t_.throttle = std::max(0.0, std::min(1.0, v));
}

void DescentSim::throttleUp(double dt) {
    // A real throttle does not snap; it ramps over about a second. Acceptable
    // while Ready too, so the player can spool up before committing.
    if (phase_ != DescentPhase::Flying && phase_ != DescentPhase::Ready) return;
    t_.throttle = std::min(1.0, t_.throttle + dt * 1.0);
}

void DescentSim::throttleDown(double dt) {
    if (phase_ != DescentPhase::Flying && phase_ != DescentPhase::Ready) return;
    t_.throttle = std::max(0.0, t_.throttle - dt * 1.4);
}

void DescentSim::tiltLeft(double dt) {
    if (phase_ != DescentPhase::Flying) return;
    const double maxRad = maxTiltDeg_ * M_PI / 180.0;
    tiltRad_ = std::max(-maxRad, tiltRad_ - dt * 0.22);
}

void DescentSim::tiltRight(double dt) {
    if (phase_ != DescentPhase::Flying) return;
    const double maxRad = maxTiltDeg_ * M_PI / 180.0;
    tiltRad_ = std::min(maxRad, tiltRad_ + dt * 0.22);
}

void DescentSim::levelTilt() {
    if (phase_ != DescentPhase::Flying) return;
    tiltRad_ = 0.0;
}

void DescentSim::selectSite(int index) {
    // Clamp rather than reject: an out-of-range index should land on a real
    // site, not silently leave the player on a bad one.
    if (sites_.empty()) return;
    if (index < 0) index = 0;
    if (index >= static_cast<int>(sites_.size())) index = static_cast<int>(sites_.size()) - 1;
    siteIndex_ = index;
    y_ = sites_[static_cast<size_t>(index)].groundOffsetM;
}

int DescentSim::siteCount() const { return static_cast<int>(sites_.size()); }

const DescentSite& DescentSim::site(int i) const {
    return sites_[static_cast<size_t>(std::max(0, std::min(i, siteCount() - 1)))];
}

void DescentSim::abort() {
    if (phase_ == DescentPhase::Flying || phase_ == DescentPhase::Ready) {
        phase_ = DescentPhase::Crashed;
        failure_ = "The mission was scrubbed before you committed to the descent.";
    }
}

void DescentSim::step(double dt) {
    if (phase_ == DescentPhase::Ready) {
        phase_ = DescentPhase::Flying;
    }
    if (phase_ != DescentPhase::Flying) return;

    // Substep for stability. Fixed 20 ms internal ticks regardless of frame
    // time, so the trajectory is frame-rate independent and cannot be tunnelled
    // through the surface on a slow frame.
    constexpr int kMaxSub = 8;
    int steps = static_cast<int>(dt / 0.02);
    if (steps < 1) steps = 1;
    if (steps > kMaxSub) steps = kMaxSub;
    const double h = dt / static_cast<double>(steps);

    for (int i = 0; i < steps; ++i) {
        const double m = t_.massKg;
        const double thrust = t_.throttle * maxThrust_;
        const double accel = thrust / std::max(1.0, m);

        // Mass flow. At constant throttle the vehicle loses mass linearly in
        // time, which is what makes the endgame throttle-down interesting.
        double burnRate = 0.0;
        if (t_.throttle > 0.0) {
            burnRate = thrust / (exhaustVel_ * std::max(1.0, t_.throttle));
            t_.propellantKg = std::max(0.0, t_.propellantKg - burnRate * h);
            propellantUsed_ += burnRate * h;
            t_.massKg = std::max(dryMass_ + cargoMass_, t_.massKg - burnRate * h);
        }

        // Propellant exhaustion. Classic lunar death, and entirely arithmetic.
        if (t_.propellantKg <= 0.0 && t_.throttle > 0.0) {
            t_.throttle = 0.0;
        }

        // Integrated vertical and horizontal dynamics.
        t_.vAccelMps2 = accel * std::cos(tiltRad_) - gMoon_;

        const double dvx = -(accel * std::sin(tiltRad_) + gMoon_ * std::sin(tiltRad_)) * h;
        t_.hsMps += dvx;
        t_.vsMps += t_.vAccelMps2 * h;
        deltaVUsed_ += std::abs(accel) * h;

        x_ += t_.hsMps * h;
        y_ += t_.vsMps * h;
        t_.altitudeM -= std::abs(t_.vsMps) * h;

        t_.tiltDeg = tiltRad_ * 180.0 / M_PI;
        t_.groundSpeedMps = std::hypot(t_.hsMps, t_.vsMps);

        // Predicted intercept with the current throttle held: the player's main
        // instrument. It integrates to the surface at constant throttle, which
        // is exactly the mental model a real descent is flown on.
        if (t_.altitudeM > 0.0) {
            const double alt = t_.altitudeM;
            // Solve y + v*T + 0.5*a*T^2 = 0 for T > 0, then v + a*T.
            const double a = t_.vAccelMps2;
            double predicted;
            if (std::abs(a) < 1e-4) {
                predicted = (std::abs(t_.vsMps) > 1e-4) ? -alt / t_.vsMps : 0.0;
            } else {
                const double disc = t_.vsMps * t_.vsMps + 2.0 * a * (-alt);
                predicted = (disc >= 0.0)
                    ? (-t_.vsMps + std::sqrt(disc)) / a
                    : (-t_.vsMps - std::sqrt(std::max(0.0, disc))) / a;
            }
            if (predicted > 0.0) {
                t_.predictedVsMps = t_.vsMps + a * predicted;
                t_.predictedRangeM = t_.hsMps * predicted;
            } else {
                // Going up, or unreachable: no prediction is better than a wrong one.
                t_.predictedVsMps = 0.0;
                t_.predictedRangeM = 0.0;
            }
        }

        if (t_.altitudeM <= 0.0) {
            t_.altitudeM = 0.0;
            evaluateOutcome();
            return;
        }
    }
}

void DescentSim::evaluateOutcome() {
    impactVs_ = std::abs(t_.vsMps);
    impactHs_ = std::abs(t_.hsMps);
    const DescentSite& s = site(siteIndex_);

    // 1. Out of propellant above the surface is the classic and the worst.
    if (t_.propellantKg <= 0.0) {
        phase_ = DescentPhase::OutOfPropellant;
        failure_ = "You ran out of propellant above the surface. There is no air to "
                   "slow you and nothing left to push with.";
        return;
    }

    // 2. Vertical rate.
    if (impactVs_ > bal_.descentSafeVsMps) {
        phase_ = DescentPhase::Crashed;
        failure_ = "Touchdown at " + std::to_string(static_cast<int>(impactVs_)) +
                   " m/s vertical. The limit is " +
                   std::to_string(static_cast<int>(bal_.descentSafeVsMps)) +
                   ". The struts did not have an opinion about that.";
        cargoDamage_ = std::min(1.0, (impactVs_ - bal_.descentSafeVsMps) / 12.0);
        return;
    }

    // 3. Horizontal rate.
    if (impactHs_ > bal_.descentSafeHsMps) {
        phase_ = DescentPhase::Crashed;
        failure_ = "Still moving at " + std::to_string(static_cast<int>(impactHs_)) +
                   " m/s horizontally at contact. You dug a trench and kept going.";
        cargoDamage_ = std::min(1.0, (impactHs_ - bal_.descentSafeHsMps) / 6.0);
        return;
    }

    // 4. Slope. At one sixth gravity a lander on a slope goes over slowly and
    //    there is a great deal of time to watch it happen.
    if (s.slopeDeg > bal_.descentMaxSlopeDeg) {
        phase_ = DescentPhase::Crashed;
        failure_ = "You set down on a " + std::to_string(static_cast<int>(s.slopeDeg)) +
                   " degree slope at " + s.name +
                   ". The limit is " + std::to_string(static_cast<int>(bal_.descentMaxSlopeDeg)) +
                   ". It tipped. At this gravity it took a while.";
        cargoDamage_ = 0.6;
        return;
    }

    // 5. Missed the target badly on the lateral one.
    if (std::abs(x_) > s.groundOffsetM + 120.0) {
        phase_ = DescentPhase::Crashed;
        failure_ = "You came down well outside the surveyed area, on ground nobody "
                   "has mapped. The tank farm is not where you landed.";
        cargoDamage_ = 0.4;
        return;
    }

    // Success. A hard-but-legal landing still costs cargo.
    phase_ = DescentPhase::Success;
    if (impactVs_ > bal_.descentSafeVsMps * 0.7) {
        cargoDamage_ = std::min(0.5, (impactVs_ - bal_.descentSafeVsMps * 0.7) / 8.0);
    }
}

}  // namespace lt
