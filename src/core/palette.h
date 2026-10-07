// 32-colour indexed palette.
//
// The game renders into a fixed 320x200 target and blits with nearest-neighbour,
// so every pixel is exactly one of these 32 entries. Colours are grouped into
// named ramps so terrain zones can be re-lit by swapping ramp offsets rather
// than having separate sprite sheets per zone.
//
// This mirrors a real constraint the original worked under: the Apple II build
// shipped only two terrain backgrounds because of 64K RAM. Here we get 32
// colours, and we spend them on *lighting zones* instead of *biomes* -- the
// sunlit rim and the permanently shadowed region genuinely do look different,
// because they genuinely are.
//
// Index layout:
//   0- 5  regolith ramp   (bright -> shadowed)
//   6-11  void ramp       (cold blue-black, for shadowed regions)
//  12-16  metal ramp
//  17-19  hazard amber
//  20-22  oxygen cyan
//  23-25  life green
//  26-28  danger red
//  29-31  UI neutrals
#pragma once

#include <SDL.h>
#include <cstdint>

namespace lt {

enum Palette : uint8_t {
    // Regolith
    kHiWhite    = 0,   // #FFFFFF specular highlights, sunlit ice
    kRegBright  = 1,   // #D8D2C8
    kRegMid     = 2,   // #B0AAA0
    kRegDark    = 3,   // #8A857E
    kRegDeep    = 4,   // #605C57
    kRegShadow  = 5,   // #38352F

    // Void / permanently shadowed region
    kVoidRim    = 6,   // #6E86A8
    kVoidMid    = 7,   // #4A5F80
    kVoidDeep   = 8,   // #32415C
    kVoidDark   = 9,   // #1E2839
    kVoidNear   = 10,  // #111726
    kVoidBlack  = 11,  // #070A12

    // Metal
    kMetalHi    = 12,  // #E0E6EC
    kMetalMid   = 13,  // #A8B4C0
    kMetalDark  = 14,  // #7A8894
    kMetalDeep  = 15,  // #55606B
    kMetalBlack = 16,  // #333A42

    // Hazard amber
    kAmberHi    = 17,  // #FFC43D
    kAmber      = 18,  // #E08A1E
    kAmberDeep  = 19,  // #A85C10

    // Oxygen / power cyan
    kCyanHi     = 20,  // #7FE8FF
    kCyan       = 21,  // #35B4D8
    kCyanDeep   = 22,  // #1A6E88

    // Life / nominal green
    kGreenHi    = 23,  // #9BE86B
    kGreen      = 24,  // #4FA83C
    kGreenDeep  = 25,  // #2A6B26

    // Danger red
    kRedHi      = 26,  // #FF6B6B
    kRed        = 27,  // #D62D3C
    kRedDeep    = 28,  // #7E1622

    // UI neutrals
    kUiWhite    = 29,  // #F2F2F2
    kUiGrey     = 30,  // #9AA0A6
    kUiBlack    = 31,  // #101216

    kPaletteSize = 32
};

// The canonical RGB values, in index order.
const uint8_t* paletteRgb();

// Converts a palette index to an SDL_Color.
inline SDL_Color paletteColor(uint8_t index) {
    const uint8_t* rgb = paletteRgb();
    SDL_Color c;
    c.r = rgb[index * 3 + 0];
    c.g = rgb[index * 3 + 1];
    c.b = rgb[index * 3 + 2];
    c.a = 255;
    return c;
}

// Base index of each ramp, for zone lighting adjustments.
constexpr uint8_t kRegolithRampBase = kRegBright;
constexpr uint8_t kVoidRampBase     = kVoidRim;
constexpr uint8_t kMetalRampBase    = kMetalMid;
constexpr uint8_t kRampLength       = 5;

}  // namespace lt
