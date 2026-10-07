// Powered descent: the interactive minigame that closes Chapter One.
//
// This is the only part of the game that is not a menu-driven day loop, and it
// is deliberately the hardest ninety seconds in Lunar Trail.
//
// The physics are real and they are thin, because at lunar gravity in vacuum
// there is very little to work with: no drag, no atmosphere, and therefore no
// visual cue for how fast you are actually going. Apollo pilots flew these on
// landmarks and callouts; there is no cue here except the numbers, which is
// exactly the problem the player has to solve.
//
//   a = T/m - g_lunar
//   T  = throttle * maxThrust
//   vh -= (T/m) * sin(tilt) * dt      tilt is measured from vertical
//   vh -= g * sin(tilt) * dt
//   vy -= g * dt
//   x  += vh * dt      y += vy * dt      z += v * dt
//
// All lunar-relevant values, no Earth gravity, no drag term.

#pragma once

#include <string>
#include <vector>

#include "sim/balance.h"

namespace lt {

enum class DescentPhase {
    Ready,
    Flying,
    Touchdown,
    Crashed,
    Success,
    OutOfPropellant,
};

struct DescentTelemetry {
    double altitudeM = 0.0;
    double vsMps = 0.0;
    double hsMps = 0.0;
    double propellantKg = 0.0;
    double massKg = 0.0;
    double throttle = 0.0;
    double tiltDeg = 0.0;
    double vAccelMps2 = 0.0;
    double groundSpeedMps = 0.0;
    // Predicted touchdown altitude-rate if the throttle is held now. The
    // player's primary instrument; without it the descent is unfair rather than
    // hard.
    double predictedVsMps = 0.0;
    double predictedRangeM = 0.0;
};

struct DescentSite {
    std::string name;
    double slopeDeg = 0.0;
    double groundOffsetM = 0.0;   // lateral distance to the target
    std::string blurb;
    bool hasLight = true;
};

struct DescentSim {
public:
    // Prepares a descent from the current balance numbers.
    void reset(const Balance& b, double descentCargoKg);

    // ---- controls, all 0..1 or signed degrees ----
    void setThrottle(double t);
    void throttleUp(double dt);
    void throttleDown(double dt);
    void tiltLeft(double dt);
    void tiltRight(double dt);
    void levelTilt();

    // Chooses the landing site from the list.
    void selectSite(int index);
    int siteCount() const;
    const DescentSite& site(int i) const;
    int selectedSite() const { return siteIndex_; }

    // One physics step. `dt` seconds.
    void step(double dt);

    DescentPhase phase() const { return phase_; }
    const DescentTelemetry& telemetry() const { return t_; }
    double remainingAltitudeM() const { return t_.altitudeM; }

    // What went wrong, or "" on success.
    const std::string& failureReason() const { return failure_; }
    // Cargo damage fraction, 0 (untouched) to 1 (all lost).
    double cargoDamage() const { return cargoDamage_; }
    // Delta-v actually expended, for the scoreboard.
    double deltaVUsedMps() const { return deltaVUsed_; }
    double propellantUsedKg() const { return propellantUsed_; }

    void abort();

private:
    void evaluateOutcome();

    Balance bal_;
    DescentTelemetry t_;
    DescentPhase phase_ = DescentPhase::Ready;
    std::vector<DescentSite> sites_;
    int siteIndex_ = 0;

    double dryMass_ = 1000.0;
    double payloadMass_ = 0.0;
    double maxThrust_ = 45000.0;
    double isp_ = 311.0;
    double exhaustVel_ = 0.0;
    double gMoon_ = 1.62;
    double maxTiltDeg_ = 20.0;

    double x_ = 0.0;
    double y_ = 0.0;
    double tiltRad_ = 0.0;
    double cargoMass_ = 0.0;

    double deltaVUsed_ = 0.0;
    double propellantUsed_ = 0.0;
    double cargoDamage_ = 0.0;
    double impactVs_ = 0.0;
    double impactHs_ = 0.0;
    std::string failure_;
};

}  // namespace lt
