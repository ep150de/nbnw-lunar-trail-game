// Fixed-resolution pixel renderer.
//
// All drawing goes into a CPU-side framebuffer (320x200 palette indices plus a
// parallel ARGB buffer for upload), which is blitted to the window once per
// frame. This is the right architecture for pixel art at this resolution:
//
//   - Every draw is a memory write. No per-pixel SDL state changes.
//   - The destination palette index is directly readable, so zone relighting
//     (remap a sprite through a 32-entry table) is exact and free.
//   - Integer window scaling is trivial and always square.
//   - Text is a handful of fill-rect runs per glyph instead of 35 draw points.
//
// The 320x200 target is the Apple II text-mode resolution. The original built
// its entire visual identity on it; keeping it means keeping the constraints
// that made the original's look good.

#pragma once

#include <SDL.h>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/textwrap.h"

#include "core/palette.h"

namespace lt {

constexpr int kScreenW = 320;
constexpr int kScreenH = 200;

// Sentinel stored in the index buffer for "nothing here", letting sprites carry
// transparency without a second alpha plane.
constexpr uint8_t kTransparent = 255;

// A palette-indexed image. Loaded from a PNG by nearest-palette-colour
// mapping; every sprite in the game is authored against the 32-colour palette.
struct Bitmap {
    int w = 0;
    int h = 0;
    std::vector<uint8_t> px;   // w*h palette indices, or kTransparent

    const uint8_t* row(int y) const { return px.data() + static_cast<size_t>(y) * static_cast<size_t>(w); }
    uint8_t* row(int y) { return px.data() + static_cast<size_t>(y) * static_cast<size_t>(w); }
    bool valid() const { return w > 0 && h > 0 && !px.empty(); }
};

class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool init(const std::string& title);
    void shutdown();

    void beginFrame();   // clears to kUiBlack
    void present();      // uploads the framebuffer and presents

    SDL_Renderer* sdl() const { return renderer_; }

    void toggleFullscreen();
    bool fullscreen() const { return fullscreen_; }
    void setVsync(bool on);

    // ---- primitives, all in 320x200 game space ----

    void clear(uint8_t i);
    void pixel(int x, int y, uint8_t i);
    void hline(int x, int y, int w, uint8_t i);
    void vline(int x, int y, int h, uint8_t i);
    void fillRect(int x, int y, int w, int h, uint8_t i);
    void rect(int x, int y, int w, int h, uint8_t i);
    // Double-ruled panel, the standard chrome for the whole UI.
    void panel(int x, int y, int w, int h, uint8_t fill, uint8_t border);
    // Panel with a 1px title strip.
    void titledPanel(int x, int y, int w, int h, const std::string& title, uint8_t fill,
                     uint8_t border, uint8_t strip);
    void fillCircle(int cx, int cy, int r, uint8_t i);
    void ring(int cx, int cy, int r, uint8_t i);
    // Bresenham line, used for the trail progress bar and sight lines.
    void line(int x0, int y0, int x1, int y1, uint8_t i);

    // ---- images ----

    // Loads (and caches) a PNG, quantised to the palette. Returns nullptr on
    // failure and logs the reason -- a missing sprite must never be fatal.
    const Bitmap* load(const std::string& relPath);

    void blit(const Bitmap& b, int x, int y);
    void blitRegion(const Bitmap& b, int sx, int sy, int sw, int sh, int x, int y);
    void blitFlipped(const Bitmap& b, int x, int y);
    // Remaps every non-transparent index through `remap`. This is how a
    // regolith-grey rover is shown as standing in cold blue shadow.
    void blitRemapped(const Bitmap& b, int x, int y, const uint8_t remap[32]);
    void blitAlpha(const Bitmap& b, int x, int y, uint8_t alpha);
    // Horizontal wrap blit, for terrain scrolling.
    void blitWrapped(const Bitmap& b, int srcX, int srcW, int dstX, int y);

    // Read-only access to the palette-index framebuffer. Used by the UI capture
    // test to assert that every screen paints something and never writes an
    // index outside the 32-colour palette.
    const std::vector<uint8_t>& idxBuffer() const { return idx_; }

    // ---- text (5x7 bitmap font, see tools/gen_font.py) ----

    static int textWidth(const std::string& s);
    static int textWidthScaled(const std::string& s, int scale);
    static int textHeight(int scale);

    void drawText(int x, int y, const std::string& s, uint8_t color);
    // Drop-shadowed text. Readable over any background, which the game's
    // terrain scrolling requires.
    void drawTextShadow(int x, int y, const std::string& s, uint8_t color, uint8_t shadow);
    void drawTextCentered(int cx, int y, const std::string& s, uint8_t color);
    void drawTextRight(int rx, int y, const std::string& s, uint8_t color);
    void drawTextScaled(int x, int y, const std::string& s, uint8_t color, int scale);
    void drawTextCenteredScaled(int cx, int y, const std::string& s, uint8_t color, int scale);
    // Delegates to the SDL-free wrapper in core/textwrap.h, which the headless
    // test suite exercises directly. See that header for the guarantees.
    static std::vector<std::string> wrapText(const std::string& s, int maxW);
    static int wrappedRows(const std::string& s, int maxW);

    void drawTextWrapped(int x, int y, int maxW, const std::string& s, uint8_t color, int lineH);

    // Sets the cursor colour used by text entry fields.
    void setTextEntryActive(bool on) { textEntryActive_ = on; }

private:
    bool createTarget();
    void recomputeWindowSize();

    SDL_Window*   window_   = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture*  target_   = nullptr;

    int  windowW_ = kScreenW * 3;
    int  windowH_ = kScreenH * 3;
    bool fullscreen_ = false;
    bool vsync_ = true;

    // Palette indices, then ARGB8888 for upload. Keeping both means a draw
    // touches only the index buffer (1 byte/px) and present() does the RGB.
    std::vector<uint8_t>  idx_;
    std::vector<uint32_t> argb_;
    // Scratch for glyph column runs, reused to avoid per-frame allocation.
    std::vector<uint8_t> remapScratch_;
    bool textEntryActive_ = false;

    std::unordered_map<std::string, Bitmap> bitmaps_;
};

// Locates the asset root: --assets override, then ./assets relative to a few
// sensible bases, then alongside the executable (so it runs from build/).
std::string findAssetDir(const std::string& overrideDir);

// Reads a whole file, or returns an empty string.
std::string readFile(const std::string& path);

}  // namespace lt
