// UI toolkit: buttons, gauges, bars, panels, and number formatting.
//
// The resource readout is the most important widget in the game. In the
// original, a single screen showed date, weather, miles travelled, miles to the
// next landmark, health, food, wagon weight, and travel status -- all at once,
// all glanceable. Every one of those fields had a plausible reading that could
// kill you. The gauge here reproduces that: label, bar, absolute value, and
// most importantly the SOLS REMAINING, so a player can see a coming crisis
// rather than discover it.

#pragma once

#include <string>
#include <vector>

#include "core/input.h"
#include "core/renderer.h"
#include "sim/balance.h"
#include "sim/convoy.h"

namespace lt {

// ---------------------------------------------------------------- formatting

std::string fmtInt(int v);
// Thousands separators. The original used comma-grouped pounds throughout.
std::string fmtNum(double v);
// Fixed decimals with trailing zeros stripped ("3.0", "12.5", "100").
std::string fmtFixed(double v, int decimals);
// Chooses sensible units: kg below 1000, tonnes above.
std::string fmtMass(double kg);
std::string fmtWater(double litres);
std::string fmtKwh(double kwh);
std::string fmtKm(double km);
std::string fmtCredits(double cr);
std::string fmtPct(double frac);
// "12 sols" / "1 sol" / "4 hours"
std::string fmtSols(double sols);
// Signed percentage change, for the day report ("-14%").
std::string fmtDeltaPct(double before, double after);

// ---------------------------------------------------------------- widgets

struct Button {
    int x = 0, y = 0, w = 0, h = 0;
    std::string label;
    bool enabled = true;
    bool hovered = false;
    bool pressed = false;
    // Right-aligned secondary text, e.g. a price or a count.
    std::string right;

    bool contains(int mx, int my) const {
        return mx >= x && my >= y && mx < x + w && my < y + h;
    }
    bool hit(const Input& in) const {
        return enabled && in.mouseInside() && contains(in.mouseX(), in.mouseY()) &&
               in.mousePressed(MouseButton::Left);
    }
};

// Draws and updates hover state. Call before hit().
void beginButton(Renderer& r, Button& btn, const Input& in, uint8_t accent = kCyan);
void endButton(Renderer& r, const Button& btn, uint8_t accent = kCyan);

// Horizontal bar. `frac` is clamped to 0..1. Draws a 1px border and optional
// tick marks, which matter when you are trying to read a quantity precisely.
void drawBar(Renderer& r, int x, int y, int w, int h, double frac, uint8_t fill,
             uint8_t bg = kUiBlack, uint8_t border = kUiGrey, int ticks = 0);

// The signature resource readout.
//   label        e.g. "OXYGEN (O2)"
//   value        absolute, e.g. "412 kg"
//   frac         0..1 fill
//   daysLeft     sols remaining; <0 renders as "--"
//   colour       accent for the bar
void drawGauge(Renderer& r, int x, int y, int w, const std::string& label,
               const std::string& value, double frac, double daysLeft, uint8_t colour,
               bool alarm = false);

// A single status line, e.g. "PARTY  NOMINAL   O2 94%".
void drawStatusLine(Renderer& r, int x, int y, int w, const std::string& key,
                    const std::string& val, uint8_t keyColour, uint8_t valColour);

// A bordered message log. Keeps the newest `lines` entries and wraps each.
class LogPanel {
public:
    void clear();
    void push(const std::string& line);
    void pushAll(const std::vector<std::string>& lines);
    const std::vector<std::string>& lines() const { return lines_; }

    void draw(Renderer& r, int x, int y, int w, int h, const std::string& title,
              uint8_t accent) const;

private:
    std::vector<std::string> lines_;
};

// A keyboard/mouse text field. Used for the seed, crew names, and epitaphs.
class TextField {
public:
    void setText(const std::string& s) { text_ = s; cursor_ = static_cast<int>(s.size()); }
    void clear() { text_.clear(); cursor_ = 0; }
    const std::string& text() const { return text_; }
    int cursor() const { return cursor_; }

    void update(const Input& in, bool active, size_t maxLen, bool allowSpace = true);
    void draw(Renderer& r, int x, int y, int w, const std::string& label, uint8_t accent,
              bool active, const std::string& hint = "") const;

private:
    std::string text_;
    int cursor_ = 0;
};

// The horizontal route map. Shows every landmark, the chasms, the branch, the
// current position, and any prior wrecks -- the visual equivalent of the
// original's trail map screen.
void drawTrailMap(Renderer& r, int x, int y, int w, int h, const RunState& s,
                  const std::vector<double>& wreckKm, uint8_t accent);

// A full-screen modal message box. Returns when dismissed.
void drawMessageBox(Renderer& r, const std::string& title, const std::vector<std::string>& body,
                    const std::string& prompt, uint8_t accent);

// Big chapter/section title card.
void drawTitleCard(Renderer& r, const std::string& line1, const std::string& line2,
                   const std::string& line3, uint8_t accent, int t);

// A horizontal menu of options. Returns the index clicked, or -1.
// `enabled[i]` false draws the row dimmed and unclickable.
int drawMenu(Renderer& r, int x, int y, int w, const std::vector<std::string>& items,
             const std::vector<std::string>& notes, int selected, const Input& in,
             const std::vector<bool>& enabled, uint8_t accent, int rowH = 16);

// Starfield / regolith speckle used behind title and menu screens, animated by
// `t`. Keeps large empty areas from looking like a rendering failure.
void drawBackdrop(Renderer& r, int t, uint8_t skyA, uint8_t skyB, uint8_t accent);

}  // namespace lt
