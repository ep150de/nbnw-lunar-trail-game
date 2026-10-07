// The Field Manual -- the in-game how-to-play.
//
// The original shipped a "Learn About The Trail" option on the main menu and a
// printed manual with a Study Guide appendix. Both existed because the game's
// difficulty is entirely a function of systems the player must understand before
// being asked to execute them under pressure. This is that manual, in the same
// spirit and at similar length.
//
// It is written to be genuinely complete: a player who reads only this, and
// never plays, should be able to run a competent mission.

#include <algorithm>
#include <vector>

#include "game/game.h"
#include "game/manual.h"
#include "sim/trail.h"

namespace lt {

void Game::updateManual() {
    const auto& pages = manualPageTable();
    if (in().pressed(Key::Right) || in().pressed(Key::D)) {
        if (manualPage_ < static_cast<int>(pages.size()) - 1) {
            ++manualPage_;
            manualScroll_ = 0;
            audio_.play(Sfx::Click);
        } else {
            audio_.play(Sfx::Deny);
        }
    }
    if (in().pressed(Key::Left) || in().pressed(Key::A)) {
        if (manualPage_ > 0) {
            --manualPage_;
            manualScroll_ = 0;
            audio_.play(Sfx::Click);
        } else {
            audio_.play(Sfx::Deny);
        }
    }
    if (in().pressed(Key::Down) || in().pressed(Key::S)) {
        manualScroll_ = std::min(manualScroll_ + 2, 40);
        audio_.play(Sfx::Beep);
    }
    if (in().pressed(Key::Up) || in().pressed(Key::W)) {
        manualScroll_ = std::max(manualScroll_ - 2, 0);
        audio_.play(Sfx::Beep);
    }
    if (in().pressed(Key::Enter) || in().mousePressed(MouseButton::Left)) {
        setScene(Scene::MainMenu);
        audio_.play(Sfx::Select);
    }
}

void Game::drawManual() {
    Renderer& r = ren();
    const auto& pages = manualPageTable();
    const int page = std::min(manualPage_, static_cast<int>(pages.size()) - 1);
    const ManualPage& p = pages[static_cast<size_t>(page)];

    r.fillRect(0, 0, kScreenW, kScreenH, kUiBlack);
    r.fillRect(0, 0, kScreenW, 12, kMetalBlack);
    r.drawText(4, 3, "FIELD MANUAL", kCyanHi);

    const std::string pageNo = fmtInt(page + 1) + "/" + fmtInt(static_cast<int>(pages.size()));
    r.drawTextRight(kScreenW - 4, 3, pageNo, kUiGrey);

    r.drawTextShadow(6, 15, p.title, kAmberHi, kAmberDeep);

    const int viewX = 6;
    const int viewW = kScreenW - 12;
    const int viewTop = 26;
    const int viewH = 146;

    r.fillRect(viewX - 2, viewTop - 1, viewW + 4, viewH + 2, kMetalDeep);
    r.fillRect(viewX, viewTop, viewW, viewH, kUiBlack);

    // The body is authored to the viewport width (tools/reflow_manual.py keeps
    // it there), so wrapping is a safety net rather than the normal case. A
    // wrapped block can reach past the viewport bottom, so advance by the row
    // count the wrapper reports instead of assuming one row per source line.
    const int maxW = viewW - 6;
    int y = viewTop - manualScroll_;
    for (const char* line : p.body) {
        // A wrapped block may extend past the viewport bottom; only draw the
        // rows that are actually visible rather than letting the block overrun.
        const int blockRows = Renderer::wrappedRows(line, maxW);
        if (y + 8 >= viewTop && y <= viewTop + viewH) {
            const bool heading = line[0] != ' ' && line[0] != '\0';
            r.drawTextWrapped(viewX + 3, y, maxW, line,
                              heading ? kUiWhite : kUiGrey, 8);
            y += Renderer::wrappedRows(line, maxW) * 8;
        } else {
            y += blockRows * 8;
        }
        if (y > viewTop + viewH) break;
    }

    // Contents strip: one tick per section, so the player can see where they are.
    const int count = static_cast<int>(pages.size());
    const int stripY = kScreenH - 22;
    r.hline(4, stripY, kScreenW - 8, kMetalDeep);
    for (int i = 0; i < count; ++i) {
        const int bx = 4 + (i * (kScreenW - 8)) / count;
        const bool cur = (i == page);
        r.fillRect(bx, stripY + 2, 2, cur ? 5 : 2, cur ? kCyanHi : kMetalDeep);
    }

    r.fillRect(0, kScreenH - 14, kScreenW, 14, kUiBlack);
    r.drawText(6, kScreenH - 10, "L/R section   U/D scroll   ENTER back", kRegShadow);
    if (manualScroll_ > 0) {
        r.drawTextRight(kScreenW - 6, kScreenH - 10, "^ more above", kRegShadow);
    }
}

}  // namespace lt
