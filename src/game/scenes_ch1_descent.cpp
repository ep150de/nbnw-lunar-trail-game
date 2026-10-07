// The powered descent scene.

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "game/descent.h"
#include "game/game.h"

namespace lt {

void Game::updateDescent() {
    DescentSim& d = descent();

    if (d.phase() == DescentPhase::Ready) {
        // Site selection before committing.
        if (in().pressed(Key::Up) || in().pressed(Key::W) || in().pressed(Key::Left) ||
            in().pressed(Key::A)) {
            d.selectSite((d.selectedSite() + d.siteCount() - 1) % d.siteCount());
            audio_.play(Sfx::Beep);
        }
        if (in().pressed(Key::Down) || in().pressed(Key::S) || in().pressed(Key::Right) ||
            in().pressed(Key::D)) {
            d.selectSite((d.selectedSite() + 1) % d.siteCount());
            audio_.play(Sfx::Beep);
        }
        if (in().pressed(Key::Space) || in().pressed(Key::Enter)) {
            audio_.play(Sfx::Power);
        }
    } else if (d.phase() == DescentPhase::Flying) {
        const double dt = 1.0 / 60.0;
        if (in().held(Key::Up) || in().held(Key::W))       d.throttleUp(dt);
        if (in().held(Key::Down) || in().held(Key::S))     d.throttleDown(dt);
        if (in().held(Key::Left) || in().held(Key::A))     d.tiltLeft(dt);
        if (in().held(Key::Right) || in().held(Key::D))    d.tiltRight(dt);
        if (in().pressed(Key::Space))                      d.setThrottle(0.0);
        if (in().pressed(Key::LShift))                     d.levelTilt();

        d.step(dt);
        if (d.phase() != DescentPhase::Flying) {
            audio_.play(d.phase() == DescentPhase::Success ? Sfx::Select : Sfx::Impact);
        }
    } else {
        if (in().pressed(Key::Enter) || in().pressed(Key::Space)) {
            if (d.phase() == DescentPhase::Success) {
                // Land it. Convert the arrival stack into a surface manifest.
                const double damage = d.cargoDamage();
                runState().stock.cargoKg *= (1.0 - damage);
                runState().progress.cargoLostKg += runState().stock.cargoKg * damage;

                // Whatever you did not spend getting down is what you land with.
                runState().stock.waterL += 900.0;
                runState().stock.o2Kg   += 90.0;
                runState().stock.foodKg += 150.0;
                runState().stock.fuelKg += 80.0;
                runState().stock.oxidiserKg += 400.0;
                runState().stock.suitSets += 10;
                runState().stock.cuttingCharges += 24;
                runState().hardware.sparesWheel = 3;
                runState().hardware.sparesBogie = 2;
                runState().hardware.sparesSeal = 2;
                runState().energyKwh = balance_.batteryCapacityKwh;
                runState().progress.zone = Zone::Sunlit;

                // You are standing AT Shackleton Rim Depot, so the depot is the
                // current landmark -- not the next one. Getting this wrong skipped
                // the single most valuable store on the route and made the whole
                // Chapter 2 economy unreachable.
                runState().progress.landmarkIndex = 0;

                // Clamp the manifest to the payload cap. Anything over it is
                // jettisoned at the pad, which is the honest consequence.
                const double cap = balance_.payloadCapKg;
                if (runState().totalPayloadKg(balance_) > cap) {
                    const double excess = runState().totalPayloadKg(balance_) - cap;
                    const double take = std::min(excess, runState().stock.waterL);
                    runState().stock.waterL -= take;
                    notify("Manifest over capacity. " + fmtMass(excess) + " jettisoned.", kAmberHi);
                }

                log().push("Touchdown. " + std::to_string(static_cast<int>(d.deltaVUsedMps())) +
                           " m/s expended, " + fmtMass(d.propellantUsedKg()) + " burned.");
                if (damage > 0.05) {
                    log().push("Hard landing. Cargo is damaged.");
                }

                // Arrive at the depot, which has the base price list and is the
                // only place where a load can be assembled before the route
                // begins. Chapter Two starts here, not one landmark later.
                pendingArrival_ = true;
                pendingLandmark_ = 0;
                pendingLandmarkName_ = landmarkNow().name;

                data_.unlockTrail();
                saveData();
                notify("Surface. " + fmtKm(balance_.trailTotalKm) + " to the colony site.",
                       kGreenHi);
                setScene(Scene::TrailMenu);
                audio_.play(Sfx::Select);
            } else {
                // Crash: the run is over before Chapter Two begins.
                runState().outcome = Outcome::Lost;
                runState().lossReason = LossReason::DescentCrash;
                runState().progress.kmTravelled = 0.0;
                runState().progress.sol = 0;
                endingEpitaph_ = "Down on the rim, " + std::to_string(
                    static_cast<int>(balance_.descentStartAltitudeM)) + " m short of it.";
                setScene(Scene::Death);
            }
        }
    }
}

void Game::drawDescent() {
    Renderer& r = ren();
    DescentSim& d = descent();
    const auto& t = d.telemetry();

    r.fillRect(0, 0, kScreenW, kScreenH, kUiBlack);

    // Sky: black, because there is no atmosphere. Stars do not twinkle.
    for (int i = 0; i < 90; ++i) {
        const int sx = (i * 137) % kScreenW;
        const int sy = (i * 71) % 110;
        r.pixel(sx, sy, (i % 5 == 0) ? kUiWhite : kMetalDeep);
    }

    const bool ready = d.phase() == DescentPhase::Ready;
    const bool flying = d.phase() == DescentPhase::Flying;

    // ---- terrain ----
    const int groundY = 110;
    if (!ready) {
        // Surface rises as altitude falls: a crude but honest perspective cue.
        const double altFrac = std::max(0.0, std::min(1.0, t.altitudeM / 15000.0));
        const int horizon = groundY - static_cast<int>((1.0 - altFrac) * 55.0);
        for (int y = horizon; y < kScreenH; ++y) {
            const int depth = y - horizon;
            uint8_t col = depth < 3 ? kRegBright : (depth < 8 ? kRegMid :
                          (depth < 18 ? kRegDark : kRegDeep));
            r.hline(0, y, kScreenW, col);
        }
        // Crater rims on the horizon.
        for (int i = 0; i < 9; ++i) {
            const int cx = (i * 41 + 13) % kScreenW;
            const int cw = 14 + (i * 7) % 22;
            const int ch = 4 + (i * 5) % 7;
            for (int dx = 0; dx < cw / 2; ++dx) {
                const int hh = static_cast<int>(ch * (1.0 - static_cast<double>(dx) / (cw / 2.0)));
                r.vline(cx - cw / 2 + dx, horizon - hh, hh, kRegMid);
            }
        }

        // The lander.
        const int lx = kScreenW / 2 + static_cast<int>(t.hsMps * -2.0);
        const int ly = horizon - 6 - static_cast<int>((1.0 - altFrac) * 40.0);
        r.fillRect(lx - 4, ly, 9, 6, kMetalMid);
        r.fillRect(lx - 5, ly, 2, 2, kMetalHi);
        r.fillRect(lx + 4, ly, 2, 2, kMetalHi);
        r.hline(lx - 7, ly + 6, 15, kMetalDark);
        r.pixel(lx, ly + 2, t.throttle > 0.1 ? kAmberHi : kRedDeep);
        if (t.throttle > 0.05) {
            const int plume = 2 + static_cast<int>(t.throttle * 7.0);
            r.fillRect(lx - 2, ly + 7, 5, plume, kAmber);
            r.fillRect(lx - 1, ly + 7 + plume, 3, plume / 2, kAmberHi);
        }
    } else {
        r.hline(0, groundY, kScreenW, kRegBright);
    }

    // ---- HUD ----
    r.titledPanel(4, 4, 150, 62, "DESCENT", kUiBlack, kCyan, kMetalDeep);

    drawStatusLine(r, 8, 16, 142, "ALTITUDE",
                     t.altitudeM > 999.0
                         ? fmtFixed(t.altitudeM / 1000.0, 2) + " km"
                         : fmtFixed(t.altitudeM, 0) + " m", kUiGrey, kUiWhite);
    drawStatusLine(r, 8, 25, 142, "VERTICAL",
                     fmtFixed(t.vsMps, 1) + " m/s", kUiGrey,
                     std::abs(t.vsMps) > balance_.descentSafeVsMps ? kRedHi : kGreenHi);
    drawStatusLine(r, 8, 34, 142, "HORIZONTAL",
                     fmtFixed(t.hsMps, 1) + " m/s", kUiGrey,
                     std::abs(t.hsMps) > balance_.descentSafeHsMps ? kRedHi : kGreenHi);
    drawStatusLine(r, 8, 43, 142, "PROPELLANT",
                     fmtMass(t.propellantKg), kUiGrey,
                     t.propellantKg < 200.0 ? kRedHi : kAmberHi);
    drawStatusLine(r, 8, 52, 142, "MASS", fmtMass(t.massKg), kUiGrey, kUiWhite);
    drawStatusLine(r, 8, 61, 142, "TILT", fmtFixed(t.tiltDeg, 0) + "\xef\xbf\xb0", kUiGrey, kUiWhite);

    // Throttle bar.
    r.drawText(158, 16, "THROTTLE", kUiGrey);
    drawBar(r, 158, 25, 60, 8, t.throttle, t.throttle > 0.85 ? kRed : kCyan, kUiBlack, kUiGrey);
    r.drawText(158, 37, "ACCEL " + fmtFixed(t.vAccelMps2, 2), kRegShadow);
    r.drawText(158, 46, "dv USED " + fmtFixed(d.deltaVUsedMps(), 0), kRegShadow);
    r.drawText(158, 55, "dv LEFT ~" + fmtFixed(t.propellantKg * t.massKg > 0
                                                   ? (balance_.descentIspS * 9.80665 *
                                                      std::log(std::max(1.0,
                                                          (t.massKg + t.propellantKg) /
                                                          std::max(1.0, t.massKg))))
                                                   : 0.0, 0), kAmberHi);

    // The predicted intercept. This is the instrument that makes the descent
    // hard rather than merely punishing.
    r.titledPanel(4, 120, 312, 40, "PREDICTED TOUCHDOWN  (throttle held)", kUiBlack, kAmber,
                  kMetalDeep);
    const bool predOk = flying && std::abs(t.predictedVsMps) <= balance_.descentSafeVsMps;
    r.drawText(8, 132, "VERTICAL", kUiGrey);
    r.drawText(70, 132, flying ? fmtFixed(t.predictedVsMps, 2) + " m/s" : "--",
               flying ? (predOk ? kGreenHi : kRedHi) : kRegShadow);
    r.drawText(130, 132, "LIMIT " + fmtFixed(balance_.descentSafeVsMps, 0) + " m/s", kRegShadow);
    r.drawText(210, 132, "RANGE", kUiGrey);
    r.drawText(252, 132, flying ? fmtFixed(t.predictedRangeM, 0) + " m" : "--",
               flying ? kUiWhite : kRegShadow);

    const double slope = d.site(d.selectedSite()).slopeDeg;
    const bool slopeOk = slope <= balance_.descentMaxSlopeDeg;
    r.drawText(8, 142, "SITE", kUiGrey);
    r.drawText(70, 142, d.site(d.selectedSite()).name, slopeOk ? kUiWhite : kRedHi);
    r.drawText(210, 142, "SLOPE " + fmtFixed(slope, 0) + "\xef\xbf\xb0",
               slopeOk ? kGreenHi : kRedHi);

    // ---- site selection ----
    if (ready) {
        r.titledPanel(4, 74, 312, 44, "SELECT LANDING SITE", kUiBlack, kAmber, kMetalDeep);
        int y = 86;
        for (int i = 0; i < d.siteCount(); ++i) {
            const auto& s = d.site(i);
            const bool sel = (i == d.selectedSite());
            if (sel) {
                r.fillRect(8, y - 2, 304, 10, kMetalBlack);
                r.rect(8, y - 2, 304, 10, kCyanHi);
            }
            r.drawText(12, y, s.name, sel ? kHiWhite : kUiWhite);
            r.drawText(110, y, fmtFixed(s.slopeDeg, 0) + "\xef\xbf\xb0", kRegShadow);
            r.drawText(150, y, s.blurb, kRegShadow);
            y += 11;
        }
        if ((static_cast<int>(frame_) / 24) % 2 == 0) {
            r.drawTextCentered(kScreenW / 2, 182, "SPACE to begin descent", kCyanHi);
        }
        r.drawTextCentered(kScreenW / 2, 191, "UP/DOWN change site", kRegShadow);
    } else if (flying) {
        r.drawTextCentered(kScreenW / 2, 176,
                           "UP throttle    DOWN throttle    LEFT/RIGHT tilt    SHIFT level", kUiGrey);
        r.drawTextCentered(kScreenW / 2, 187, "SPACE throttle to zero", kRegShadow);
    }

    // ---- outcome ----
    if (!ready && !flying) {
        const bool ok = d.phase() == DescentPhase::Success;
        const bool dry = d.phase() == DescentPhase::OutOfPropellant;
        std::vector<std::string> body;
        body.push_back(d.failureReason());
        if (ok) {
            body.push_back("");
            body.push_back("Touchdown at " + fmtFixed(std::abs(t.vsMps), 2) + " m/s vertical, " +
                           fmtFixed(std::abs(t.hsMps), 2) + " m/s horizontal, on a " +
                           fmtFixed(slope, 0) + " degree slope.");
            body.push_back("");
            body.push_back(d.cargoDamage() > 0.05
                ? "Cargo damage: " + fmtPct(d.cargoDamage()) + "."
                : "Cargo intact.");
            body.push_back("Propellant remaining: " + fmtMass(t.propellantKg) +
                           ". That is what you have to live on.");
        } else if (dry) {
            body.push_back("");
            body.push_back("The ascent stage had 100 x 100 km of orbit to spend and "
                           "did not spend it.");
        } else {
            body.push_back("");
            body.push_back("Everything the crew bought with their credits is at the "
                           "bottom of a crater on the south pole rim.");
        }
        drawMessageBox(r, ok ? "TOUCHDOWN" : "DESCENT LOST", body,
                       "ENTER to continue", ok ? kGreenHi : kRedHi);
    }
}

}  // namespace lt
