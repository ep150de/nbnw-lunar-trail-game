#include "core/renderer.h"

#include <SDL_image.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

#include "font5x7.h"

namespace lt {

namespace fs = std::filesystem;

// ---------------------------------------------------------------- palette

const uint8_t* paletteRgb() {
    static const uint8_t rgb[kPaletteSize * 3] = {
        0xFF, 0xFF, 0xFF,  //  0 hi white      - specular, sunlit ice
        0xD8, 0xD2, 0xC8,  //  1 regolith bright
        0xB0, 0xAA, 0xA0,  //  2 regolith mid
        0x8A, 0x85, 0x7E,  //  3 regolith dark
        0x60, 0x5C, 0x57,  //  4 regolith deep
        0x38, 0x35, 0x2F,  //  5 regolith shadow

        0x6E, 0x86, 0xA8,  //  6 void rim
        0x4A, 0x5F, 0x80,  //  7 void mid
        0x32, 0x41, 0x5C,  //  8 void deep
        0x1E, 0x28, 0x39,  //  9 void dark
        0x11, 0x17, 0x26,  // 10 void near
        0x07, 0x0A, 0x12,  // 11 void black

        0xE0, 0xE6, 0xEC,  // 12 metal hi
        0xA8, 0xB4, 0xC0,  // 13 metal mid
        0x7A, 0x88, 0x94,  // 14 metal dark
        0x55, 0x60, 0x6B,  // 15 metal deep
        0x33, 0x3A, 0x42,  // 16 metal black

        0xFF, 0xC4, 0x3D,  // 17 amber hi
        0xE0, 0x8A, 0x1E,  // 18 amber
        0xA8, 0x5C, 0x10,  // 19 amber deep

        0x7F, 0xE8, 0xFF,  // 20 cyan hi
        0x35, 0xB4, 0xD8,  // 21 cyan
        0x1A, 0x6E, 0x88,  // 22 cyan deep

        0x9B, 0xE8, 0x6B,  // 23 green hi
        0x4F, 0xA8, 0x3C,  // 24 green
        0x2A, 0x6B, 0x26,  // 25 green deep

        0xFF, 0x6B, 0x6B,  // 26 red hi
        0xD6, 0x2D, 0x3C,  // 27 red
        0x7E, 0x16, 0x22,  // 28 red deep

        0xF2, 0xF2, 0xF2,  // 29 ui white
        0x9A, 0xA0, 0xA6,  // 30 ui grey
        0x10, 0x12, 0x16,  // 31 ui black
    };
    return rgb;
}

namespace {
// Precomputed 0x00RRGGBB per palette index, so present() is a table lookup.
const uint32_t* paletteArgbTable() {
    static uint32_t table[kPaletteSize];
    static bool built = false;
    if (!built) {
        const uint8_t* rgb = paletteRgb();
        for (int i = 0; i < kPaletteSize; ++i) {
            table[i] = 0xFF000000u | (static_cast<uint32_t>(rgb[i * 3 + 0]) << 16) |
                       (static_cast<uint32_t>(rgb[i * 3 + 1]) << 8) |
                       static_cast<uint32_t>(rgb[i * 3 + 2]);
        }
        built = true;
    }
    return table;
}

// Nearest palette index for an RGB triple. Sprites are authored against the
// palette, so this is exact for our own art and a graceful approximation for
// anything else a contributor drops in.
uint8_t nearestPaletteIndex(uint8_t r, uint8_t g, uint8_t b) {
    const uint8_t* pal = paletteRgb();
    int best = 0;
    int bestDist = 1 << 30;
    for (int i = 0; i < kPaletteSize; ++i) {
        const int dr = static_cast<int>(r) - pal[i * 3 + 0];
        const int dg = static_cast<int>(g) - pal[i * 3 + 1];
        const int db = static_cast<int>(b) - pal[i * 3 + 2];
        // Perceptually weighted; green dominates luminance, hence the weights.
        const int dist = 3 * dr * dr + 6 * dg * dg + db * db;
        if (dist < bestDist) {
            bestDist = dist;
            best = i;
        }
    }
    return static_cast<uint8_t>(best);
}

const GlyphDef* findGlyph(uint32_t cp) {
    // Linear scan: 111 entries, and text is drawn in batch, so the branch
    // predictor handles this well. A binary search would cost more than it saves.
    for (int i = 0; i < kGlyphCount; ++i) {
        if (kGlyphTable[i].codepoint == cp) return &kGlyphTable[i];
    }
    return nullptr;
}
}  // namespace

// ---------------------------------------------------------------- asset paths

std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string findAssetDir(const std::string& overrideDir) {
    if (!overrideDir.empty()) return overrideDir;

    std::error_code ec;
    std::vector<fs::path> candidates;

    const fs::path cwd = fs::current_path(ec);
    if (!ec) {
        candidates.push_back(cwd / "assets");
        candidates.push_back(cwd / ".." / "assets");
        candidates.push_back(cwd / ".." / ".." / "assets");
        candidates.push_back(cwd / ".." / ".." / ".." / "assets");
    }

    if (char* base = SDL_GetBasePath()) {
        const fs::path exeDir(base);
        SDL_free(base);
        candidates.push_back(exeDir / "assets");                        // build/assets
        candidates.push_back(exeDir / ".." / "assets");                 // build/../assets
        candidates.push_back(exeDir / ".." / "share" / "lunar_trail" / "assets");
    }

    for (const auto& c : candidates) {
        if (fs::exists(c, ec) && fs::is_directory(c, ec)) {
            return fs::weakly_canonical(c, ec).string();
        }
    }
    return cwd.string() + "/assets";
}

// ---------------------------------------------------------------- lifecycle

Renderer::~Renderer() { shutdown(); }

bool Renderer::init(const std::string& title) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init: " << SDL_GetError() << "\n";
        return false;
    }
    if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0) {
        std::cerr << "IMG_Init: " << SDL_GetError() << "\n";
        return false;
    }

    // Start at 3x, shrink to fit a typical desktop, never below 1x.
    int best = 1;
    for (int s = 6; s >= 1; --s) {
        if (kScreenW * s <= 1920 && kScreenH * s <= 1200) {
            best = s;
            break;
        }
    }
    windowW_ = kScreenW * best;
    windowH_ = kScreenH * best;

    window_ = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED, windowW_, windowH_,
                               SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (window_ == nullptr) {
        std::cerr << "SDL_CreateWindow: " << SDL_GetError() << "\n";
        return false;
    }

    Uint32 rflags = SDL_RENDERER_ACCELERATED;
    if (vsync_) rflags |= SDL_RENDERER_PRESENTVSYNC;
    renderer_ = SDL_CreateRenderer(window_, -1, rflags);
    if (renderer_ == nullptr) {
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    }
    if (renderer_ == nullptr) {
        std::cerr << "SDL_CreateRenderer: " << SDL_GetError() << "\n";
        return false;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    if (!createTarget()) return false;

    idx_.assign(static_cast<size_t>(kScreenW) * kScreenH, kUiBlack);
    argb_.assign(static_cast<size_t>(kScreenW) * kScreenH, 0xFF101216u);
    return true;
}

bool Renderer::createTarget() {
    if (target_ != nullptr) {
        SDL_DestroyTexture(target_);
        target_ = nullptr;
    }
    target_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, kScreenW, kScreenH);
    if (target_ == nullptr) {
        std::cerr << "SDL_CreateTexture: " << SDL_GetError() << "\n";
        return false;
    }
    SDL_SetTextureScaleMode(target_, SDL_ScaleModeNearest);
    return true;
}

void Renderer::shutdown() {
    bitmaps_.clear();
    if (target_ != nullptr)   { SDL_DestroyTexture(target_);   target_ = nullptr; }
    if (renderer_ != nullptr) { SDL_DestroyRenderer(renderer_); renderer_ = nullptr; }
    if (window_ != nullptr)   { SDL_DestroyWindow(window_);     window_ = nullptr; }
    IMG_Quit();
    SDL_Quit();
}

void Renderer::recomputeWindowSize() {
    if (window_ == nullptr) return;
    int best = 1;
    for (int s = 6; s >= 1; --s) {
        if (kScreenW * s <= 1920 && kScreenH * s <= 1200) { best = s; break; }
    }
    windowW_ = kScreenW * best;
    windowH_ = kScreenH * best;
}

void Renderer::toggleFullscreen() {
    if (window_ == nullptr) return;
    const Uint32 flags = fullscreen_ ? 0u
                                : static_cast<Uint32>(SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (SDL_SetWindowFullscreen(window_, flags) == 0) {
        fullscreen_ = !fullscreen_;
        if (!fullscreen_) {
            recomputeWindowSize();
            SDL_SetWindowSize(window_, windowW_, windowH_);
        }
        SDL_ShowCursor(fullscreen_ ? SDL_DISABLE : SDL_ENABLE);
    }
}

void Renderer::setVsync(bool on) {
    // vsync is a context-creation flag on SDL2, so a live toggle is not
    // possible. Record the preference; it takes effect on the next start.
    vsync_ = on;
}

void Renderer::beginFrame() {
    std::fill(idx_.begin(), idx_.end(), static_cast<uint8_t>(kUiBlack));
}

void Renderer::present() {
    if (target_ == nullptr) return;

    const uint32_t* table = paletteArgbTable();
    for (int y = 0; y < kScreenH; ++y) {
        const uint8_t* row = idx_.data() + static_cast<size_t>(y) * kScreenW;
        uint32_t* out = argb_.data() + static_cast<size_t>(y) * kScreenW;
        for (int x = 0; x < kScreenW; ++x) out[x] = table[row[x]];
    }
    SDL_UpdateTexture(target_, nullptr, argb_.data(), kScreenW * 4);

    int outW = 0, outH = 0;
    SDL_GetRendererOutputSize(renderer_, &outW, &outH);

    // Integer scale + letterbox. Never a fractional factor: pixel art at 3.4x
    // is the exact thing this architecture exists to prevent.
    const int s = std::max(1, std::min(outW / kScreenW, outH / kScreenH));
    const int drawW = kScreenW * s;
    const int drawH = kScreenH * s;

    SDL_SetRenderDrawColor(renderer_, 8, 8, 10, 255);
    SDL_RenderClear(renderer_);

    const SDL_Rect dst{(outW - drawW) / 2, (outH - drawH) / 2, drawW, drawH};
    SDL_RenderCopy(renderer_, target_, nullptr, &dst);
    SDL_RenderPresent(renderer_);
}

// ---------------------------------------------------------------- primitives

void Renderer::clear(uint8_t i) {
    std::fill(idx_.begin(), idx_.end(), i);
}

void Renderer::pixel(int x, int y, uint8_t i) {
    if (x < 0 || y < 0 || x >= kScreenW || y >= kScreenH) return;
    idx_[static_cast<size_t>(y) * static_cast<size_t>(kScreenW) +
          static_cast<size_t>(x)] = i;
}

void Renderer::hline(int x, int y, int w, uint8_t i) {
    if (y < 0 || y >= kScreenH || w <= 0) return;
    const int x0 = std::max(0, x);
    const int x1 = std::min(kScreenW, x + w);
    if (x1 <= x0) return;
    uint8_t* const row = idx_.data() + static_cast<size_t>(y) * kScreenW;
    std::fill(row + x0, row + x1, i);
}

void Renderer::vline(int x, int y, int h, uint8_t i) {
    if (x < 0 || x >= kScreenW || h <= 0) return;
    const int y0 = std::max(0, y);
    const int y1 = std::min(kScreenH, y + h);
    for (int yy = y0; yy < y1; ++yy) {
        idx_[static_cast<size_t>(yy) * static_cast<size_t>(kScreenW) +
              static_cast<size_t>(x)] = i;
    }
}

void Renderer::fillRect(int x, int y, int w, int h, uint8_t i) {
    if (w <= 0 || h <= 0) return;
    const int x0 = std::max(0, x);
    const int y0 = std::max(0, y);
    const int x1 = std::min(kScreenW, x + w);
    const int y1 = std::min(kScreenH, y + h);
    // Essential when the rect starts off-screen: clipping to the left or top
    // edge can invert the range, and an inverted range makes std::fill walk
    // backwards off the front of the buffer.
    if (x1 <= x0 || y1 <= y0) return;
    for (int yy = y0; yy < y1; ++yy) {
        uint8_t* const row = idx_.data() + static_cast<size_t>(yy) * kScreenW;
        std::fill(row + x0, row + x1, i);
    }
}

void Renderer::rect(int x, int y, int w, int h, uint8_t i) {
    if (w <= 0 || h <= 0) return;
    hline(x, y, w, i);
    hline(x, y + h - 1, w, i);
    vline(x, y + 1, h - 2, i);
    vline(x + w - 1, y + 1, h - 2, i);
}

void Renderer::panel(int x, int y, int w, int h, uint8_t fill, uint8_t border) {
    fillRect(x, y, w, h, border);
    rect(x, y, w, h, border);
    if (w > 4 && h > 4) {
        rect(x + 1, y + 1, w - 2, h - 2, kUiBlack);
        fillRect(x + 2, y + 2, w - 4, h - 4, fill);
    }
}

void Renderer::titledPanel(int x, int y, int w, int h, const std::string& title, uint8_t fill,
                           uint8_t border, uint8_t strip) {
    panel(x, y, w, h, fill, border);
    if (h > 10) {
        fillRect(x + 2, y + 2, w - 4, 9, strip);
        if (!title.empty()) {
            const int tx = x + 4;
            drawText(tx, y + 3, title, kUiWhite);
        }
    }
}

void Renderer::fillCircle(int cx, int cy, int r, uint8_t i) {
    if (r < 0) return;
    for (int dy = -r; dy <= r; ++dy) {
        const int span = static_cast<int>(
            std::sqrt(static_cast<double>(r * r - dy * dy)));
        hline(cx - span, cy + dy, span * 2 + 1, i);
    }
}

void Renderer::ring(int cx, int cy, int r, uint8_t i) {
    if (r < 0) return;
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        pixel(cx + x, cy + y, i); pixel(cx + y, cy + x, i);
        pixel(cx - y, cy + x, i); pixel(cx - x, cy + y, i);
        pixel(cx - x, cy - y, i); pixel(cx - y, cy - x, i);
        pixel(cx + y, cy - x, i); pixel(cx + x, cy - y, i);
        ++y;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            --x;
            err += 2 * (y - x) + 1;
        }
    }
}

void Renderer::line(int x0, int y0, int x1, int y1, uint8_t i) {
    // Bresenham; integer-only so the result is identical on every machine.
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        pixel(x0, y0, i);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// ---------------------------------------------------------------- images

const Bitmap* Renderer::load(const std::string& relPath) {
    auto it = bitmaps_.find(relPath);
    if (it != bitmaps_.end()) {
        return it->second.valid() ? &it->second : nullptr;
    }

    SDL_Surface* surf = IMG_Load(relPath.c_str());
    if (surf == nullptr) {
        std::cerr << "asset missing: " << relPath << " (" << IMG_GetError() << ")\n";
        bitmaps_[relPath] = Bitmap{};  // cache the failure, don't retry every frame
        return nullptr;
    }

    Bitmap bmp;
    bmp.w = surf->w;
    bmp.h = surf->h;
    bmp.px.resize(static_cast<size_t>(bmp.w) * static_cast<size_t>(bmp.h), kTransparent);

    // Lock once; sprites are static so a single pass is fine.
    if (SDL_MUSTLOCK(surf) && SDL_LockSurface(surf) != 0) {
        SDL_FreeSurface(surf);
        bitmaps_[relPath] = Bitmap{};
        return nullptr;
    }

    const uint8_t* base = static_cast<const uint8_t*>(surf->pixels);
    const int bpp = surf->format->BytesPerPixel;
    const bool hasAlpha = SDL_ISPIXELFORMAT_ALPHA(surf->format->BitsPerPixel) != 0 ||
                          surf->format->Amask != 0;

    for (int y = 0; y < bmp.h; ++y) {
        const uint8_t* src =
            base + static_cast<size_t>(y) * static_cast<size_t>(surf->pitch);
        uint8_t* dst = bmp.row(y);
        for (int x = 0; x < bmp.w; ++x) {
            uint32_t p;
            switch (bpp) {
                case 1:  p = src[x]; break;
                case 2:  p = (static_cast<uint32_t>(src[x * 2]) << 8) | src[x * 2 + 1]; break;
                case 4:  p = static_cast<uint32_t>(src[x * 4]) | (static_cast<uint32_t>(src[x * 4 + 1]) << 8) |
                              (static_cast<uint32_t>(src[x * 4 + 2]) << 16) |
                              (static_cast<uint32_t>(src[x * 4 + 3]) << 24); break;
                default: p = 3; break;
            }
            uint8_t r = static_cast<uint8_t>(p & 0xFF);
            uint8_t g = static_cast<uint8_t>((p >> 8) & 0xFF);
            uint8_t b = static_cast<uint8_t>((p >> 16) & 0xFF);
            uint8_t a = 255;
            if (hasAlpha) a = static_cast<uint8_t>((p >> 24) & 0xFF);
            // Below ~1/8 opacity counts as empty: our art is either fully solid
            // or fully absent, and this kills any stray antialiasing edges.
            if (a < 32) {
                dst[x] = kTransparent;
            } else {
                dst[x] = nearestPaletteIndex(r, g, b);
            }
        }
    }

    if (SDL_MUSTLOCK(surf)) SDL_UnlockSurface(surf);
    SDL_FreeSurface(surf);

    auto ins = bitmaps_.emplace(relPath, std::move(bmp));
    return ins.first->second.valid() ? &ins.first->second : nullptr;
}

void Renderer::blit(const Bitmap& b, int x, int y) {
    for (int sy = 0; sy < b.h; ++sy) {
        const int dy = y + sy;
        if (dy < 0 || dy >= kScreenH) continue;
        const uint8_t* src = b.row(sy);
        uint8_t* dst = idx_.data() + static_cast<size_t>(dy) * kScreenW;
        for (int sx = 0; sx < b.w; ++sx) {
            const int dx = x + sx;
            if (dx < 0 || dx >= kScreenW) continue;
            const uint8_t v = src[sx];
            if (v != kTransparent) dst[dx] = v;
        }
    }
}

void Renderer::blitRegion(const Bitmap& b, int sx0, int sy0, int sw, int sh, int x, int y) {
    for (int sy = 0; sy < sh; ++sy) {
        const int by = sy0 + sy;
        if (by < 0 || by >= b.h) continue;
        const int dy = y + sy;
        if (dy < 0 || dy >= kScreenH) continue;
        const uint8_t* src = b.row(by);
        uint8_t* dst = idx_.data() + static_cast<size_t>(dy) * kScreenW;
        for (int sx = 0; sx < sw; ++sx) {
            const int bx = sx0 + sx;
            if (bx < 0 || bx >= b.w) continue;
            const int dx = x + sx;
            if (dx < 0 || dx >= kScreenW) continue;
            const uint8_t v = src[bx];
            if (v != kTransparent) dst[dx] = v;
        }
    }
}

void Renderer::blitFlipped(const Bitmap& b, int x, int y) {
    for (int sy = 0; sy < b.h; ++sy) {
        const int dy = y + sy;
        if (dy < 0 || dy >= kScreenH) continue;
        const uint8_t* src = b.row(sy);
        uint8_t* dst = idx_.data() + static_cast<size_t>(dy) * kScreenW;
        for (int sx = 0; sx < b.w; ++sx) {
            const int dx = x + (b.w - 1 - sx);
            if (dx < 0 || dx >= kScreenW) continue;
            const uint8_t v = src[sx];
            if (v != kTransparent) dst[dx] = v;
        }
    }
}

void Renderer::blitRemapped(const Bitmap& b, int x, int y, const uint8_t remap[32]) {
    if (remap == nullptr) { blit(b, x, y); return; }
    for (int sy = 0; sy < b.h; ++sy) {
        const int dy = y + sy;
        if (dy < 0 || dy >= kScreenH) continue;
        const uint8_t* src = b.row(sy);
        uint8_t* dst = idx_.data() + static_cast<size_t>(dy) * kScreenW;
        for (int sx = 0; sx < b.w; ++sx) {
            const int dx = x + sx;
            if (dx < 0 || dx >= kScreenW) continue;
            const uint8_t v = src[sx];
            if (v != kTransparent) dst[dx] = remap[v];
        }
    }
}

void Renderer::blitAlpha(const Bitmap& b, int x, int y, uint8_t alpha) {
    // Alpha in this game is only ever used for fades. Blend against whatever is
    // already in the index buffer using a few ordered dither patterns, which
    // suits 320x200 far better than a smooth alpha ramp.
    if (alpha >= 250) { blit(b, x, y); return; }
    if (alpha < 12)   return;

    static const uint8_t kBayer4[16] = {
         0,  8,  2, 10,
        12,  4, 14,  6,
         3, 11,  1,  9,
        15,  7, 13,  5,
    };
    const int threshold = static_cast<int>(alpha) * 16 / 255;

    for (int sy = 0; sy < b.h; ++sy) {
        const int dy = y + sy;
        if (dy < 0 || dy >= kScreenH) continue;
        const uint8_t* src = b.row(sy);
        uint8_t* dst = idx_.data() + static_cast<size_t>(dy) * kScreenW;
        for (int sx = 0; sx < b.w; ++sx) {
            const int dx = x + sx;
            if (dx < 0 || dx >= kScreenW) continue;
            const uint8_t v = src[sx];
            if (v == kTransparent) continue;
            const uint8_t d = kBayer4[(dy & 3) * 4 + (dx & 3)];
            if (d < threshold) dst[dx] = v;
        }
    }
}

void Renderer::blitWrapped(const Bitmap& b, int srcX, int srcW, int dstX, int y) {
    for (int sy = 0; sy < b.h; ++sy) {
        const int dy = y + sy;
        if (dy < 0 || dy >= kScreenH) continue;
        const uint8_t* src = b.row(sy);
        uint8_t* dst = idx_.data() + static_cast<size_t>(dy) * kScreenW;
        for (int i = 0; i < srcW; ++i) {
            int sx = srcX + i;
            sx %= b.w;
            if (sx < 0) sx += b.w;
            const int dx = (dstX + i) % kScreenW;
            const uint8_t v = src[sx];
            if (v != kTransparent) dst[dx < 0 ? dx + kScreenW : dx] = v;
        }
    }
}

// ---------------------------------------------------------------- text

int Renderer::textWidth(const std::string& s) {
    if (s.empty()) return 0;
    // UTF-8 aware: count codepoints, not bytes.
    int chars = 0;
    for (char ch : s) {
        // Mask rather than compare signed: bytes >= 0x80 are negative as char.
        if ((static_cast<unsigned char>(ch) & 0xC0) != 0x80) ++chars;
    }
    return chars * kGlyphAdvance - 1;
}

int Renderer::textWidthScaled(const std::string& s, int scale) { return textWidth(s) * scale; }
int Renderer::textHeight(int scale) { return kGlyphH * scale; }

void Renderer::drawText(int x, int y, const std::string& s, uint8_t color) {
    int cx = x;
    // Decode UTF-8 incrementally so multi-byte symbols work everywhere.
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        uint32_t cp = c;
        size_t len = 1;
        if (c >= 0xF0)      { cp = c & 0x07u; len = 4; }
        else if (c >= 0xE0) { cp = c & 0x0Fu; len = 3; }
        else if (c >= 0xC0) { cp = c & 0x1Fu; len = 2; }
        if (i + len > s.size()) { len = 1; cp = c; }
        for (size_t k = 1; k < len; ++k) {
            cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3Fu);
        }
        i += len;

        const GlyphDef* g = findGlyph(cp);
        if (g == nullptr) g = findGlyph('?');
        if (g == nullptr) { cx += kGlyphAdvance; continue; }
        for (int col = 0; col < kGlyphW; ++col) {
            const uint8_t bits = g->cols[col];
            if (bits == 0) continue;
            // One fillRect per vertical run in the column.
            int run = 0;
            for (int row = 0; row <= kGlyphH; ++row) {
                const bool on = (row < kGlyphH) && ((bits >> row) & 1u);
                if (on) {
                    ++run;
                } else if (run > 0) {
                    fillRect(cx + col, y + row - run, 1, run, color);
                    run = 0;
                }
            }
        }
        cx += kGlyphAdvance;
    }
}

void Renderer::drawTextShadow(int x, int y, const std::string& s, uint8_t color, uint8_t shadow) {
    drawText(x + 1, y + 1, s, shadow);
    drawText(x, y, s, color);
}

void Renderer::drawTextCentered(int cx, int y, const std::string& s, uint8_t color) {
    drawText(cx - textWidth(s) / 2, y, s, color);
}

void Renderer::drawTextRight(int rx, int y, const std::string& s, uint8_t color) {
    drawText(rx - textWidth(s), y, s, color);
}

void Renderer::drawTextScaled(int x, int y, const std::string& s, uint8_t color, int scale) {
    if (scale <= 1) { drawText(x, y, s, color); return; }
    // Scale by drawing each glyph pixel as a scale x scale block. Cheaper than
    // holding scaled atlases, and text is short.
    int cx = x;
    for (size_t i = 0; i < s.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        uint32_t cp = c;
        size_t len = 1;
        if (c >= 0xF0)      { cp = c & 0x07u; len = 4; }
        else if (c >= 0xE0) { cp = c & 0x0Fu; len = 3; }
        else if (c >= 0xC0) { cp = c & 0x1Fu; len = 2; }
        if (i + len > s.size()) { len = 1; cp = c; }
        for (size_t k = 1; k < len; ++k) {
            cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3Fu);
        }
        i += len;

        const GlyphDef* g = findGlyph(cp);
        if (g == nullptr) g = findGlyph('?');
        if (g == nullptr) { cx += kGlyphAdvance * scale; continue; }
        for (int col = 0; col < kGlyphW; ++col) {
            const uint8_t bits = g->cols[col];
            if (bits == 0) continue;
            for (int row = 0; row < kGlyphH; ++row) {
                if (((bits >> row) & 1u) == 0) continue;
                fillRect(cx + col * scale, y + row * scale, scale, scale, color);
            }
        }
        cx += kGlyphAdvance * scale;
    }
}

void Renderer::drawTextCenteredScaled(int cx, int y, const std::string& s, uint8_t color, int scale) {
    drawTextScaled(cx - textWidthScaled(s, scale) / 2, y, s, color, scale);
}

std::vector<std::string> Renderer::wrapText(const std::string& s, int maxW) {
    return lt::wrapText(s, maxW);
}

int Renderer::wrappedRows(const std::string& s, int maxW) {
    return lt::wrappedRows(s, maxW);
}

void Renderer::drawTextWrapped(int x, int y, int maxW, const std::string& s, uint8_t color,
                               int lineH) {
    const std::vector<std::string> rows = lt::wrapText(s, maxW);
    int lineY = y;
    for (const std::string& row : rows) {
        drawText(x, lineY, row, color);
        lineY += lineH;
        if (lineY > kScreenH) break;
    }
}

}  // namespace lt
