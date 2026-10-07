#include "game/ui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "sim/trail.h"

namespace lt {

// ---------------------------------------------------------------- formatting

std::string fmtInt(int v) { return std::to_string(v); }

std::string fmtNum(double v) {
    const double r = std::round(v);
    if (std::abs(v - r) > 0.005 && std::abs(v) < 100000.0) return fmtFixed(v, 1);

    char buf[48];
    std::snprintf(buf, sizeof(buf), "%.0f", r);

    // Insert thousands separators.
    std::string s(buf);
    bool neg = !s.empty() && s[0] == '-';
    if (neg) s.erase(0, 1);
    std::string out;
    int count = 0;
    for (int i = static_cast<int>(s.size()) - 1; i >= 0; --i) {
        out.push_back(s[static_cast<size_t>(i)]);
        if (++count % 3 == 0 && i > 0) out.push_back(',');
    }
    std::reverse(out.begin(), out.end());
    return neg ? "-" + out : out;
}

std::string fmtFixed(double v, int decimals) {
    if (decimals < 0) decimals = 0;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    std::string s(buf);
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    if (s == "-0") s = "0";
    return s;
}

std::string fmtMass(double kg) {
    if (std::abs(kg) >= 1000.0) return fmtFixed(kg / 1000.0, 2) + " t";
    return fmtFixed(kg, 0) + " kg";
}

std::string fmtWater(double litres) {
    if (std::abs(litres) >= 1000.0) return fmtFixed(litres / 1000.0, 2) + " kl";
    return fmtFixed(litres, 0) + " L";
}

std::string fmtKwh(double kwh) { return fmtFixed(kwh, 0) + " kWh"; }

std::string fmtKm(double km) { return fmtFixed(km, 1) + " km"; }

std::string fmtCredits(double cr) {
    char buf[40];
    // Credits are large numbers; no decimals, comma-grouped, with a unit.
    std::snprintf(buf, sizeof(buf), "%.0f", std::round(cr));
    std::string s(buf);
    bool neg = !s.empty() && s[0] == '-';
    if (neg) s.erase(0, 1);
    std::string out;
    int count = 0;
    for (int i = static_cast<int>(s.size()) - 1; i >= 0; --i) {
        out.push_back(s[static_cast<size_t>(i)]);
        if (++count % 3 == 0 && i > 0) out.push_back(',');
    }
    std::reverse(out.begin(), out.end());
    return (neg ? "-" : "") + out + " cr";
}

std::string fmtPct(double frac) { return fmtFixed(frac * 100.0, 0) + "%"; }

std::string fmtSolsCompact(double sols) {
    if (sols < 0.0) return "--";
    if (sols < 1.0) return fmtFixed(sols * 24.0, 0) + "h";
    return std::to_string(static_cast<int>(std::lround(sols))) + "s";
}

std::string fmtSols(double sols) {
    if (sols < 0.0) return "--";
    if (sols < 1.0) return fmtFixed(sols * 24.0, 0) + " h";
    const double rounded = std::round(sols);
    if (rounded == 1.0) return "1 sol";
    return std::to_string(static_cast<int>(rounded)) + " sols";
}

std::string fmtDeltaPct(double before, double after) {
    if (before <= 0.0) return "  --";
    const double d = (after - before) / before;
    const char* sign = d >= 0.0 ? "+" : "-";
    return std::string(sign) + fmtFixed(std::abs(d) * 100.0, 0) + "%";
}

// ---------------------------------------------------------------- widgets

void beginButton(Renderer& r, Button& btn, const Input& in, uint8_t accent) {
    btn.hovered = btn.enabled && in.mouseInside() && btn.contains(in.mouseX(), in.mouseY());
    btn.pressed = btn.hovered && in.mouseHeld(MouseButton::Left);
}

void endButton(Renderer& r, const Button& btn, uint8_t accent) {
    if (!btn.enabled) {
        r.panel(btn.x, btn.y, btn.w, btn.h, kRegDeep, kRegShadow);
        r.drawText(btn.x + 5, btn.y + (btn.h - 7) / 2, btn.label, kRegShadow);
        if (!btn.right.empty()) r.drawTextRight(btn.x + btn.w - 5, btn.y + (btn.h - 7) / 2,
                                                 btn.right, kRegShadow);
        return;
    }

    uint8_t fill = kRegDark;
    uint8_t edge = accent;
    if (btn.pressed)      { fill = kRegDeep; edge = kRegBright; }
    else if (btn.hovered) { fill = kRegMid;  edge = kCyanHi; }

    r.panel(btn.x, btn.y, btn.w, btn.h, fill, edge);
    const int ty = btn.y + (btn.h - 7) / 2;
    r.drawText(btn.x + 5, ty, btn.label, btn.hovered ? kHiWhite : kUiWhite);
    if (!btn.right.empty()) {
        r.drawTextRight(btn.x + btn.w - 5, ty, btn.right, btn.hovered ? kHiWhite : kUiWhite);
    }
}

void drawBar(Renderer& r, int x, int y, int w, int h, double frac, uint8_t fill,
             uint8_t bg, uint8_t border, int ticks) {
    frac = std::max(0.0, std::min(1.0, frac));
    r.fillRect(x, y, w, h, bg);
    const int inner = static_cast<int>(std::lround(frac * static_cast<double>(w - 2)));
    if (inner > 0) {
        r.fillRect(x + 1, y + 1, std::min(inner, w - 2), h - 2, fill);
        // A one-pixel highlight along the top of the fill reads as depth at
        // this size; without it the bars look like flat blocks.
        if (h >= 4 && inner > 1) r.hline(x + 1, y + 1, std::min(inner, w - 2), kHiWhite);
    }
    r.rect(x, y, w, h, border);
    if (ticks > 1) {
        for (int i = 1; i < ticks; ++i) {
            const int tx = x + (w * i) / ticks;
            r.vline(tx, y + 1, h - 2, bg);
        }
    }
}

void drawGauge(Renderer& r, int x, int y, int w, const std::string& label,
               const std::string& value, double frac, double daysLeft, uint8_t colour,
               bool alarm) {
    // Two rows, 23px pitch:
    //   row 1   label (left)          sols remaining (right)
    //   row 2   bar (left)            absolute value (right)
    //
    // The value must never share a row with the label. In a 146px panel the
    // widest pair is PROPELLANT + "70 sols" = 102px, which fits; but a label and
    // a value together reach 96px in a 70px panel and overprint. Keeping them on
    // separate rows removes the collision by construction rather than by tuning
    // label lengths.
    const uint8_t textCol = alarm ? static_cast<uint8_t>(kRedHi) : static_cast<uint8_t>(kUiWhite);
    const uint8_t labelCol = alarm ? static_cast<uint8_t>(kRedHi)
                                   : static_cast<uint8_t>(kUiGrey);
    const std::string days = daysLeft < 0 ? std::string("--") : fmtSols(daysLeft);

    // Right-align the days figure, shrinking it to a compact form rather than
    // dropping it. PROPELLANT is 10 characters (60px) and "70 sols" is 42px,
    // which needs 108px in a 102px panel, so the full form had nowhere to go and
    // used to be pushed onto a third row where it overprinted the bar.
    const int labelW = Renderer::textWidth(label);
    std::string daysText = days;
    if (labelW + Renderer::textWidth(daysText) + 6 > w) {
        daysText = fmtSolsCompact(daysLeft);
    }
    const bool showDays = (labelW + Renderer::textWidth(daysText) + 6) <= w;

    r.drawText(x, y, label, labelCol);
    if (showDays) r.drawTextRight(x + w, y, daysText, textCol);

    const int valueW = Renderer::textWidth(value);
    const int barW = std::max(10, w - valueW - 5);
    const uint8_t fillCol = alarm ? static_cast<uint8_t>(kRed) : colour;
    drawBar(r, x, y + 9, barW, 6, frac, fillCol, kUiBlack, kUiGrey, 3);
    r.drawTextRight(x + w, y + 8, value, textCol);

    // Last resort: the panel is too narrow for any figure beside the label, so
    // put it under the bar rather than lose the number that drives decisions.
    if (!showDays) r.drawText(x, y + 16, daysText, textCol);
}

void drawStatusLine(Renderer& r, int x, int y, int w, const std::string& key,
                    const std::string& val, uint8_t keyColour, uint8_t valColour) {
    r.drawText(x, y, key, keyColour);
    r.drawTextRight(x + w, y, val, valColour);
}

void LogPanel::clear() { lines_.clear(); }

void LogPanel::push(const std::string& line) {
    lines_.push_back(line);
    // Bound the buffer; nobody scrolls back 500 lines.
    if (lines_.size() > 200) lines_.erase(lines_.begin(), lines_.begin() + 100);
}

void LogPanel::pushAll(const std::vector<std::string>& lines) {
    for (const auto& l : lines) push(l);
}

void LogPanel::draw(Renderer& r, int x, int y, int w, int h, const std::string& title,
                    uint8_t accent) const {
    r.titledPanel(x, y, w, h, title, kUiBlack, accent, kMetalDeep);

    // Wrap every line, then show the tail that fits. Uses the shared wrap logic
    // so the log, the manual, and message boxes all break at the same places.
    const int maxW = w - 12;
    std::vector<std::string> wrapped;
    for (const auto& l : lines_) {
        const std::vector<std::string> rows = Renderer::wrapText(l, maxW);
        wrapped.insert(wrapped.end(), rows.begin(), rows.end());
    }

    const int capacity = (h - 16) / 8;
    const int start = std::max(0, static_cast<int>(wrapped.size()) - capacity);
    for (int i = 0; i < capacity; ++i) {
        const size_t idx = static_cast<size_t>(start + i);
        if (idx >= wrapped.size()) break;
        const bool newest = (start + i) == static_cast<int>(wrapped.size()) - 1;
        r.drawText(x + 5, y + 13 + i * 8, wrapped[idx], newest ? kUiWhite : kUiGrey);
    }

    if (wrapped.size() > static_cast<size_t>(capacity)) {
        r.drawTextRight(x + w - 4, y + h - 10, "^ older", kRegShadow);
    }
}

void TextField::update(const Input& in, bool active, size_t maxLen, bool allowSpace) {
    if (!active) return;

    if (in.pressed(Key::Backspace)) {
        if (cursor_ > 0 && static_cast<size_t>(cursor_) <= text_.size()) {
            text_.erase(static_cast<size_t>(cursor_) - 1, 1);
            --cursor_;
        }
    }
    if (in.pressed(Key::Delete)) {
        if (cursor_ < static_cast<int>(text_.size())) text_.erase(static_cast<size_t>(cursor_), 1);
    }
    if (in.pressed(Key::Left))  cursor_ = std::max(0, cursor_ - 1);
    if (in.pressed(Key::Right)) cursor_ = std::min(static_cast<int>(text_.size()), cursor_ + 1);
    if (in.pressed(Key::Home))  cursor_ = 0;
    if (in.pressed(Key::End))   cursor_ = static_cast<int>(text_.size());

    if (in.pressed(Key::V) && in.shiftHeld()) {
        const char* clip = SDL_GetClipboardText();
        if (clip != nullptr) {
            std::string paste(clip);
            SDL_free(const_cast<char*>(clip));
            if (!allowSpace) {
                paste.erase(std::remove_if(paste.begin(), paste.end(),
                                           [](char c) { return c == ' ' || c == '\n'; }),
                            paste.end());
            }
            if (text_.size() + paste.size() <= maxLen) {
                text_.insert(static_cast<size_t>(cursor_), paste);
                cursor_ += static_cast<int>(paste.size());
            }
        }
    }

    if (!in.lastChar().empty()) {
        const std::string& s = in.lastChar();
        for (char raw : s) {
            const unsigned char c = static_cast<unsigned char>(raw);
            if (c < 0x20) continue;                      // control chars
            if (c == 0x7f) continue;
            if (!allowSpace && (c == ' ' || c == '\t')) continue;
            if (text_.size() >= maxLen) break;
            text_.insert(static_cast<size_t>(cursor_), 1, raw);
            ++cursor_;
        }
    }
}

void TextField::draw(Renderer& r, int x, int y, int w, const std::string& label,
                     uint8_t accent, bool active, const std::string& hint) const {
    r.drawText(x, y, label, kUiGrey);
    const int fieldY = y + 9;
    const uint8_t fillCol = active ? static_cast<uint8_t>(kMetalBlack) : static_cast<uint8_t>(kRegDeep);
    const uint8_t edgeCol = active ? accent : static_cast<uint8_t>(kRegShadow);
    r.panel(x, fieldY, w, 13, fillCol, edgeCol);
    const std::string shown = text_.empty() ? hint : text_;
    r.drawText(x + 4, fieldY + 3, shown, text_.empty() ? kRegShadow : kUiWhite);

    if (active) {
        // Blinking caret, 2Hz off the frame clock.
        const int cw = Renderer::textWidth(text_.substr(0, static_cast<size_t>(cursor_)));
        const int cx = x + 4 + cw;
        if ((SDL_GetTicks() / 500) % 2 == 0) {
            r.vline(cx, fieldY + 3, 9, kCyanHi);
        }
    }
}

void drawTrailMap(Renderer& r, int x, int y, int w, int h, const RunState& s,
                  const std::vector<double>& wreckKm, uint8_t accent) {
    const auto& t = trail();
    const double total = std::max(1.0, s.progress.kmTravelled);

    const int trackY = y + h / 2 + 6;
    r.hline(x + 6, trackY, w - 12, kRegDeep);

    // Route polyline, coloured by the zone each segment sits in.
    for (size_t i = 1; i < t.size(); ++i) {
        const int x0 = x + 6 + static_cast<int>((t[i - 1].km / total) * (w - 12));
        const int x1 = x + 6 + static_cast<int>((t[i].km / total) * (w - 12));
        uint8_t col = kRegBright;
        switch (t[i].zone) {
            case Zone::Sunlit:     col = kRegBright; break;
            case Zone::Transition: col = kMetalMid;  break;
            case Zone::Psr:        col = kVoidRim;   break;
        }
        r.hline(x0, trackY, std::max(1, x1 - x0), col);
    }

    // Landmark ticks. Stores are drawn taller.
    for (const auto& l : t) {
        const int lx = x + 6 + static_cast<int>((l.km / total) * (w - 12));
        const int tall = (l.storeIndex >= 0) ? 9 : 5;
        uint8_t col = kUiGrey;
        if (l.kind == LandmarkKind::Chasm)      col = kRed;
        else if (l.kind == LandmarkKind::Depot) col = kAmber;
        else if (l.kind == LandmarkKind::Colony) col = kGreenHi;
        r.vline(lx, trackY - tall, tall + 1, col);
        if (l.kind == LandmarkKind::Chasm) {
            // Chasms get a notch in the track so they read as breaks.
            r.pixel(lx - 1, trackY, kRegBright);
            r.pixel(lx + 1, trackY, kRegBright);
        }
    }

    // Prior wrecks: small crosses below the track.
    for (double km : wreckKm) {
        const int wx = x + 6 + static_cast<int>((km / total) * (w - 12));
        r.pixel(wx - 2, trackY + 5, kRedHi);
        r.pixel(wx + 2, trackY + 5, kRedHi);
        r.pixel(wx - 1, trackY + 4, kRedHi);
        r.pixel(wx + 1, trackY + 4, kRedHi);
        r.pixel(wx - 1, trackY + 6, kRedHi);
        r.pixel(wx + 1, trackY + 6, kRedHi);
        r.pixel(wx, trackY + 5, kRedHi);
    }

    // Convoy marker.
    const int cx = x + 6 + static_cast<int>(
        std::min(1.0, s.progress.kmTravelled / total) * (w - 12));
    // Upward-pointing convoy marker above the track, with a stem down to it.
    for (int dy = 0; dy < 5; ++dy) {
        const int half = 4 - dy;
        r.hline(cx - half, trackY - 9 + dy, half * 2 + 1, kCyanHi);
    }
    r.vline(cx, trackY - 4, 5, kCyanHi);

    // Labels for the route start and the next landmark. At the start of the
    // run those are the same place, and printing the depot name twice on one row
    // reads as a rendering fault rather than as information.
    r.drawText(x + 6, trackY + 12, t.front().name.c_str(), kRegShadow);
    const Landmark& next = t[static_cast<size_t>(
        std::min<size_t>(static_cast<size_t>(s.progress.landmarkIndex), t.size() - 1))];
    if (next.name != t.front().name) {
        r.drawTextRight(x + w - 6, trackY + 12, next.name.c_str(), kUiGrey);
    }

    char buf[64];
    std::snprintf(buf, sizeof(buf), "SOL %d   %s / %s   %s",
                  s.progress.sol, fmtKm(s.progress.kmTravelled).c_str(),
                  fmtKm(t.back().km).c_str(), zoneName(s.progress.zone));
    r.drawText(x + 6, y, buf, accent);
}

void drawMessageBox(Renderer& r, const std::string& title, const std::vector<std::string>& body,
                    const std::string& prompt, uint8_t accent) {
    const int w = 260;
    const int h = 130;
    const int x = (kScreenW - w) / 2;
    const int y = (kScreenH - h) / 2;

    // Dim the play area behind the box so the modal reads clearly.
    r.fillRect(0, 0, kScreenW, kScreenH, kUiBlack);
    r.titledPanel(x, y, w, h, title, kMetalBlack, accent, kMetalDeep);

    int ty = y + 15;
    for (const auto& line : body) {
        if (line.empty()) {
            ty += 4;
            continue;
        }
        // Wrap within the box.
        int remaining = static_cast<int>(line.size());
        size_t offset = 0;
        bool first = true;
        while (offset < line.size()) {
            int fit = 0;
            int wpx = 0;
            while (offset + static_cast<size_t>(fit) < line.size() && wpx < w - 16) {
                wpx += 6;
                ++fit;
            }
            std::string chunk = line.substr(offset, static_cast<size_t>(fit));
            r.drawText(x + 8 + (first ? 0 : 4), ty, chunk, first ? kUiWhite : kUiGrey);
            offset += static_cast<size_t>(fit);
            remaining = static_cast<int>(line.size() - offset);
            ty += 8;
            first = false;
            if (ty > y + h - 20) break;
        }
        if (remaining > 0 && ty > y + h - 20) break;
        if (!first) ty += 0;
    }

    if ((SDL_GetTicks() / 600) % 2 == 0) {
        r.drawTextCentered(x + w / 2, y + h - 13, prompt, accent);
    }
}

void drawTitleCard(Renderer& r, const std::string& line1, const std::string& line2,
                   const std::string& line3, uint8_t accent, int t) {
    const int cx = kScreenW / 2;
    r.fillRect(0, 0, kScreenW, kScreenH, kUiBlack);

    // A slowly pulsing rule above and below the title.
    const int glow = 1 + ((t / 18) % 3);
    r.hline(40, 74, kScreenW - 80, kMetalDeep);
    r.hline(40, 76 + glow, kScreenW - 80, accent);

    r.drawTextCenteredScaled(cx, 46, line1, kUiWhite, 2);
    if (!line2.empty()) r.drawTextCentered(cx, 84, line2, kUiGrey);
    if (!line3.empty()) r.drawTextCentered(cx, 96, line3, kRegShadow);

    r.hline(40, 108 - glow, kScreenW - 80, accent);
    r.hline(40, 110, kScreenW - 80, kMetalDeep);
}

int drawMenu(Renderer& r, int x, int y, int w, const std::vector<std::string>& items,
             const std::vector<std::string>& notes, int selected, const Input& in,
             const std::vector<bool>& enabled, uint8_t accent, int rowH) {
    int clicked = -1;

    for (size_t i = 0; i < items.size(); ++i) {
        const int ry = y + static_cast<int>(i) * rowH;
        const bool on = (enabled.empty() || i >= enabled.size()) ? true : enabled[i];
        const bool hover = on && in.mouseInside() && in.mouseX() >= x && in.mouseX() < x + w &&
                           in.mouseY() >= ry && in.mouseY() < ry + rowH;
        const bool isSel = static_cast<int>(i) == selected;

        uint8_t fill = kUiBlack;
        uint8_t edge = kMetalDeep;
        if (isSel)      { fill = kMetalBlack; edge = accent; }
        if (hover)      { fill = kRegDark;    edge = kCyanHi; }

        r.fillRect(x, ry, w, rowH - 1, fill);
        if (isSel || hover) r.rect(x, ry, w, rowH - 1, edge);

        // Number key hint, as the original's menus were numbered.
        const std::string num = std::to_string(i + 1);
        r.drawText(x + 3, ry + 4, num, on ? kRegShadow : kRegShadow);

        uint8_t textCol = on ? kUiWhite : kRegShadow;
        if (!on) textCol = kRegShadow;
        r.drawText(x + 12, ry + 4, items[i], textCol);

        // Right-aligned notes must not overprint the label. "Settings" is 8
        // characters and "SOUND OFF" is 9, which together are 108px in a 108px
        // panel -- so measure before drawing the note.
        if (!notes.empty() && i < notes.size() && !notes[i].empty()) {
            const int noteW = Renderer::textWidth(notes[i]);
            const int labelEnd = x + 12 + Renderer::textWidth(items[i]);
            if (labelEnd + 6 + noteW <= x + w - 4) {
                r.drawTextRight(x + w - 4, ry + 4, notes[i], on ? kUiGrey : kRegShadow);
            }
        }

        if (hover && in.mousePressed(MouseButton::Left)) clicked = static_cast<int>(i);
    }

    return clicked;
}

void drawBackdrop(Renderer& r, int t, uint8_t skyA, uint8_t skyB, uint8_t accent) {
    // Vertical gradient, banded. Banding is deliberate: a smooth gradient at 320x200
    // with 32 colours looks like noise, whereas bands read as sky depth.
    for (int y = 0; y < kScreenH; ++y) {
        const int band = (y / 8) % 3;
        const uint8_t rowCol = band == 0 ? skyA : (band == 1 ? skyB : static_cast<uint8_t>(kUiBlack));
        r.hline(0, y, kScreenW, rowCol);
    }

    // Drifting specks. Deterministic from the frame clock so the motion is
    // smooth and repeatable rather than jittering on load.
    for (int i = 0; i < 60; ++i) {
        const int sx = (i * 97 + t / 3) % kScreenW;
        const int sy = (i * 53 + t / 5) % kScreenH;
        if (((i + t / 40) % 7) == 0) {
            r.pixel(sx, sy, accent);
        } else {
            r.pixel(sx, sy, kMetalDeep);
        }
    }
}

}  // namespace lt
