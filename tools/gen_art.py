#!/usr/bin/env python3
"""
Lunar Trail art generator.

Every sprite the game loads is produced here, from a palette and shape code,
into indexed PNGs that match the game's 32-colour palette exactly. Nothing is
traced, sampled, or copied: the art is drawn pixel by pixel from the same ASCII
that the font uses, so it is diff-able, reviewable, and reproducible.

Two source styles:

  ASCII   Multi-line ASCII art, one character per pixel. Used for anything
          hand-tuned (the rover, the crew, the outpost modules).
  PROC    Procedural. Used for terrain, which is better generated than drawn:
          noise, ramps, and dither patterns beat hand-placing 320x200 pixels.

Run:  python3 tools/gen_art.py
"""

import math
import os
import random
import sys

try:
    from PIL import Image, ImageDraw
except ImportError:
    print("error: Pillow is required (pip install pillow)", file=sys.stderr)
    sys.exit(1)

# ---------------------------------------------------------------------------
# The palette, mirrored from src/core/palette.h. Kept in sync by
# tools/check_palette.py, which parses the header and compares.
# ---------------------------------------------------------------------------

PAL = {
    'W': (0xFF, 0xFF, 0xFF),  # 0  hi white
    '1': (0xD8, 0xD2, 0xC8),  # 1  regolith bright
    '2': (0xB0, 0xAA, 0xA0),  # 2  regolith mid
    '3': (0x8A, 0x85, 0x7E),  # 3  regolith dark
    '4': (0x60, 0x5C, 0x57),  # 4  regolith deep
    '5': (0x38, 0x35, 0x2F),  # 5  regolith shadow

    # NOTE: 'v' was originally used for two different entries (void deep and
    # danger red). Python dicts silently keep the last one, which meant the void
    # ramp was being drawn in hazard red and every shadowed-region strip came
    # out wrong. These keys are now unique and asserted below.
    'A': (0x6E, 0x86, 0xA8),  # 6  void rim
    'B': (0x4A, 0x5F, 0x80),  # 7  void mid
    'C': (0x32, 0x41, 0x5C),  # 8  void deep
    'D': (0x1E, 0x28, 0x39),  # 9  void dark
    'E': (0x11, 0x17, 0x26),  # 10 void near
    'F': (0x07, 0x0A, 0x12),  # 11 void black

    'H': (0xE0, 0xE6, 0xEC),  # 12 metal hi
    'M': (0xA8, 0xB4, 0xC0),  # 13 metal mid
    'N': (0x7A, 0x88, 0x94),  # 14 metal dark
    'K': (0x55, 0x60, 0x6B),  # 15 metal deep
    'X': (0x33, 0x3A, 0x42),  # 16 metal black

    'y': (0xFF, 0xC4, 0x3D),  # 17 amber hi
    'o': (0xE0, 0x8A, 0x1E),  # 18 amber
    'r': (0xA8, 0x5C, 0x10),  # 19 amber deep

    'I': (0x7F, 0xE8, 0xFF),  # 20 cyan hi
    'c': (0x35, 0xB4, 0xD8),  # 21 cyan mid
    'j': (0x1A, 0x6E, 0x88),  # 22 cyan deep

    'G': (0x9B, 0xE8, 0x6B),  # 23 green hi
    'g': (0x4F, 0xA8, 0x3C),  # 24 green
    'l': (0x2A, 0x6B, 0x26),  # 25 green deep

    'R': (0xFF, 0x6B, 0x6B),  # 26 red hi
    'v': (0xD6, 0x2D, 0x3C),  # 27 red
    's': (0x7E, 0x16, 0x22),  # 28 red deep

    'w': (0xF2, 0xF2, 0xF2),  # 29 ui white
    'u': (0x9A, 0xA0, 0xA6),  # 30 ui grey
    'z': (0x10, 0x12, 0x16),  # 31 ui black
}

# Every key must be unique and every value must match exactly one palette
# entry, so a duplicate key can never again silently shadow another entry.
assert len(set(PAL.keys())) == len(PAL), "duplicate key in PAL"
assert len(set(PAL.values())) == len(PAL), "duplicate colour in PAL"
assert len(PAL) == 32, "palette must have exactly 32 entries, got %d" % len(PAL)

TRANSPARENT = (0, 0, 0, 0)


def from_ascii(rows, palette=None):
    """ASCII art -> RGBA Image. '.' and ' ' are transparent."""
    pal = palette or PAL
    w = max(len(r) for r in rows)
    h = len(rows)
    img = Image.new('RGBA', (w, h), TRANSPARENT)
    px = img.load()
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch in '. ':
                continue
            if ch not in pal:
                raise SystemExit("unknown palette char %r at %d,%d" % (ch, x, y))
            px[x, y] = pal[ch] + (255,)
    return img


def from_index_map(rows, index_of):
    """ASCII art where each char maps to a palette INDEX (or None)."""
    w = max(len(r) for r in rows)
    h = len(rows)
    img = Image.new('RGBA', (w, h), TRANSPARENT)
    px = img.load()
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch in '. ':
                continue
            idx = index_of(ch)
            if idx is None:
                continue
            r, g, b = PAL[idx]
            px[x, y] = (r, g, b, 255)
    return img


# ---------------------------------------------------------------------------
# Hand-drawn sprites
# ---------------------------------------------------------------------------

# The convoy: a pressurised crew cab on six traction wheels, three drive
# bogies, and an array mast. Drawn at 3x the on-screen size so the renderer
# can show it either at 1:1 (trail strip) or blown up (landscape view).
ROVER = [
    "..........HHHHHH..........",
    ".........HMMMMMMH.........",
    "........HMMwwwwMMMH........",
    "........HMWWWWWWMH........",
    "........HMWWWWWWMH........",
    "........HMIIIII IHM........".replace(" ", "C"),
    ".........HMMMMMMH.........",
    "......HHHHHHHHHHHHHH......",
    ".....HMMMMMMMMMMMMMMH.....",
    "....HHMMMMMMMMMMMMMMHH....",
    "....HMMMMMMMMMMMMMMMMH....",
    "....HMMKKKKKKKKKKKKMMH....",
    "....HMjrrrrrrrrrrrrjMH....",
    "....HMMMMMMMMMMMMMMMMH....",
    ".....HMMMMMMMMMMMMMMH.....",
    "......HHMMHHHHHHMMHH......",
    ".......HXX.HHHHH.XXH......",
    "......KXXK..HH..KXXK......",
    "......KXXK......KXXK......",
    "....XXXKXX......KXXKXX....",
    "..KXXKXXXK......KXXXKXXK..",
]

# Crew member in a suit, 8 wide x 12 tall, walking pose A.
CREW_A = [
    "..HHHH..",
    ".HMMMMH.",
    ".HWWWWW.",
    ".HWCWCW.",
    ".HWWWWW.",
    "..HHHH..",
    ".HMMMMH.",
    "HMMMMMMH",
    "HMMMMMMH",
    ".HMMMMH.",
    "..H..H..",
    "..K..K..",
]

CREW_B = [
    "..HHHH..",
    ".HMMMMH.",
    ".HWWWWW.",
    ".HWCWCW.",
    ".HWWWWW.",
    "..HHHH..",
    "..HMMH..",
    ".HMMMMH.",
    "..HMMH..",
    "..HMMH..",
    "..H..H..",
    ".K....K.",
]

# Relay outpost: a pressurised module on stilts with a comms mast.
OUTPOST = [
    "...........H.............",
    "...........H.............",
    "..........oH.............",
    "..........oH.............",
    "..HHHooooooHooooooHHH....",
    ".HMMMMMMMMMMMMMMMMMMMMH...",
    "HMHWWWWWWWWWWWWWWWWWHMH..",
    "HMHWCWCWCWCWCWCWCWCWHMH..",
    "HMHWWWWWWWWWWWWWWWWWHMH..",
    "HMHMMMMMMMMMMMMMMMMMMHM..",
    ".HMMMMMMMMMMMMMMMMMMMMH...",
    "..HHHHHHHHHHHHHHHHHHHH....",
    ".....N.........N.........",
    ".....N.........N.........",
    "....NNN.......NNN........",
    "...NNNNN.....NNNNN.......",
    "...KKKKK.....KKKKK.......",
]

# Colony dome: what you are driving toward.
DOME = [
    "........HHH........",
    "......HHMMMHH......",
    "....HHMMMMMMMHH....",
    "...HMMMMMMMMMMMH...",
    "..HMMHHMMMMHHMMMH..",
    ".HMMMHHMMMMHHMMMMH.",
    ".HMMMWWMMMMWWMMMMH.",
    "HMMMWWWWMMMMWWWMMMH",
    "HMMMWWCWMMMMWCWMMMH",
    "HMMMMMMMMMMMMMMMMMH",
    "HHMMMMMMMMMMMMMMMMHH",
    ".HHHMMMMMMMMMMMHHH.",
]

# Ice seam in cross-section: the thing you are drilling toward.
ICE_SEAM = [
    "4444444444444444444444",
    "4433333333333333333344",
    "4333333333333333333334",
    "4332222222222222223334",
    "432111CCCCCCCCC1122234",
    "43211CCWWWWWWWCC1122234",
    "43211CWCCCCCCWCC1122234",
    "43211CWCCCCCCWCC1122234",
    "432111CCCCCCCCC1112234",
    "4322222222222222222234",
    "4333333333333333333334",
    "4444444444444444444444",
]

# Survey stake, used as a route marker.
STAKE = [
    "..y..",
    "..y..",
    "..y..",
    "CWCyC",
    "CWCyC",
    "..y..",
    "..y..",
    "..y..",
]


# ---------------------------------------------------------------------------
# Procedural terrain
# ---------------------------------------------------------------------------

def value_noise_1d(seed, length):
    """Smooth 1D value noise. Coarse control points, smoothstep interpolation."""
    rng = random.Random(seed)
    control = [rng.uniform(0.0, 1.0) for _ in range(max(2, length // 8 + 2))]
    out = []
    for i in range(length):
        p = i / 8.0
        i0 = int(p)
        frac = p - i0
        a = control[min(i0, len(control) - 1)]
        b = control[min(i0 + 1, len(control) - 1)]
        # smoothstep
        t = frac * frac * (3.0 - 2.0 * frac)
        out.append(a + (b - a) * t)
    return out


def fbm_1d(seed, length, octaves=4, lacunarity=2.0, gain=0.5, base_period=32.0):
    """
    Fractal sum of value noise, with each octave sampled at its own frequency
    and interpolated rather than point-looked-up. Point lookup produces the
    hard one-pixel spikes that make terrain look like a seismograph.
    """
    out = [0.0] * length
    total_amp = 0.0
    amp = 1.0
    period = float(length) / base_period
    for o in range(octaves):
        n = value_noise_1d(seed + o * 7919, length)
        for i in range(length):
            p = (i / period) if period >= 1.0 else (i * (1.0 / period))
            i0 = int(math.floor(p)) % length
            i1 = (i0 + 1) % length
            frac = p - math.floor(p)
            t = frac * frac * (3.0 - 2.0 * frac)
            out[i] += (n[i0] * (1.0 - t) + n[i1] * t) * amp
        total_amp += amp
        amp *= gain
        period *= lacunarity
    return [v / total_amp for v in out]


def terrain_strip(path, w, h, zone, seed):
    """
    A horizontally tileable terrain strip for one lighting zone.

    Tileable in x by making the noise wrap: the control points at the two ends
    are forced equal, which removes the visible seam without needing a second
    buffer.
    """
    rng = random.Random(seed)
    height = fbm_1d(seed, w, octaves=4, base_period=26.0)
    # Wrap the ends.
    blend = w // 12
    for i in range(blend):
        t = i / blend
        s = (t * t * (3 - 2 * t))
        avg = (height[i] + height[w - 1 - i]) * 0.5
        height[i] = height[i] * (1 - s) + avg * s
        height[w - 1 - i] = height[w - 1 - i] * (1 - s) + avg * s

    # Each zone gets its own sky pair, ground ramp, and the single character
    # used to draw the lit lip of a crater rim.
    #
    # The lit lip matters more than it looks: in a shadowed region there is no
    # sun to light a rim, so the lip is drawn in the void ramp's own top entry
    # rather than in regolith white. Drawing white there made the shadowed
    # terrain read as a glowing sawtooth.
    if zone == 'sunlit':
        # Near the terminator: a pale blue sky down to a bright rim.
        sky_ramp = ['A', 'A', 'B', 'C', 'C']
        ground   = ['1', '1', '2', '3', '4']
        crater_lit = '2'
        crater_shade = '4'
        rim_ch = 'W'
        stars = 0.0
    elif zone == 'transition':
        # The compromise band, where the sky is already going dark.
        sky_ramp = ['B', 'C', 'C', 'D', 'D']
        ground   = ['N', 'N', '3', '4', '5']
        crater_lit = '4'
        crater_shade = 'X'
        rim_ch = 'M'
        stars = 0.0005
    else:  # psr
        # No sun at all. The sky is the void ramp; the stars are the light
        # source. Ground is lit only by regolith bounce, so it stays cold.
        sky_ramp = ['D', 'E', 'E', 'F', 'F']
        ground   = ['D', 'D', 'E', 'F', 'z']
        crater_lit = 'D'
        crater_shade = 'F'
        rim_ch = 'A'
        stars = 0.0028

    img = Image.new('RGBA', (w, h), PAL['B'] + (255,))
    px = img.load()

    # Sky gradient, banded from the ramp.
    sky_h = int(h * 0.40)
    for y in range(sky_h):
        band = int((y / max(1, sky_h - 1)) * len(sky_ramp))
        ch = sky_ramp[min(band, len(sky_ramp) - 1)]
        for x in range(w):
            px[x, y] = PAL[ch] + (255,)

    # Stars, densest where it is darkest. A shadowed region has the best view of
    # them, which is the whole visual joke of the game.
    for _ in range(int(w * h * stars)):
        x = rng.randrange(w)
        y = rng.randrange(sky_h)
        roll = rng.random()
        ch = 'W' if roll < 0.15 else 'u'
        px[x, y] = PAL[ch] + (255,)

    # The horizon: one pixel of the rim character, no more. Any thicker and it
    # reads as a glowing wire rather than as ground.
    for x in range(w):
        px[x, sky_h] = PAL[rim_ch] + (255,)

    # Surface profile. Kept shallow: a lunar surface profile is mostly flat
    # with crater rims, not a mountain range.
    relief = h - sky_h - 1
    surfaces = []
    for x in range(w):
        surfaces.append(sky_h + 1 + int(height[x] * relief * 0.30))

    # Terrain body, filled from each column's surface down.
    for x in range(w):
        surface = surfaces[x]
        for y in range(surface, h):
            depth = y - surface
            band = int(depth / 6.0)
            ch = ground[min(band, len(ground) - 1)]
            px[x, y] = PAL[ch] + (255,)
        # The lit lip of the surface itself, one pixel, in the rim colour.
        px[x, surface] = PAL[rim_ch] + (255,)

    # Craters: ellipses with a lit upper-left rim and a shaded lower-right
    # interior. Drawn after the body so they occlude it, and wrapped in x so
    # the strip stays seamless.
    for _ in range(int(w / 46)):
        cx = rng.randrange(w)
        rw = rng.randrange(7, 19)
        rh = max(2, rw // 3)
        cy = surfaces[cx] + rh + rng.randrange(2, max(3, int(relief * 0.45)))
        for dy in range(-rh, rh + 1):
            for dx in range(-rw, rw + 1):
                if (dx * dx) / (rw * rw) + (dy * dy) / (rh * rh) > 1.0:
                    continue
                x = (cx + dx) % w
                y = cy + dy
                if not (0 <= x < w and sky_h < y < h):
                    continue
                # Sun is always up and to the left in this projection.
                if dx + dy < 0:
                    px[x, y] = PAL[crater_lit] + (255,)
                elif dx - dy > rw // 3:
                    px[x, y] = PAL[crater_shade] + (255,)
                else:
                    px[x, y] = PAL[ground[1]] + (255,)

    # Boulders sitting on the surface.
    for _ in range(int(w / 30)):
        x = rng.randrange(w)
        surface = surfaces[x]
        if surface - 2 < 1:
            continue
        bw = rng.randrange(2, 5)
        bh = rng.randrange(1, 3)
        for dy in range(-bh, bh + 1):
            for dx in range(0, bw):
                y = surface - 1 + dy
                if 0 <= y < h:
                    px[(x + dx) % w, y] = PAL[ground[min(2, len(ground) - 1)]] + (255,)

    img.save(path)
    return img


def starfield(path, w, h, seed):
    """A tileable deep-space backdrop."""
    rng = random.Random(seed)
    img = Image.new('RGBA', (w, h), PAL['E'] + (255,))
    px = img.load()
    for _ in range(int(w * h * 0.002)):
        x = rng.randrange(w)
        y = rng.randrange(h)
        roll = rng.random()
        px[x, y] = PAL['W' if roll < 0.12 else ('u' if roll < 0.4 else 'B')] + (255,)
    img.save(path)
    return img


def dither_panel(path, w, h, top, bottom):
    """A vertical dither ramp, used for large flat UI areas."""
    img = Image.new('RGBA', (w, h), PAL['B'] + (255,))
    px = img.load()
    for y in range(h):
        t = y / max(1, h - 1)
        for x in range(w):
            if t < 0.5:
                px[x, y] = PAL[top] + (255,)
            else:
                # 50% checkerboard dither between the two, so the transition
                # does not band.
                px[x, y] = PAL[top if ((x + y) % 2 == 0) else bottom] + (255,)
    img.save(path)
    return img


# ---------------------------------------------------------------------------

SPRITES = {
    'rover': lambda p: from_ascii(ROVER).save(p),
    'crew_a': lambda p: from_ascii(CREW_A).save(p),
    'crew_b': lambda p: from_ascii(CREW_B).save(p),
    'outpost': lambda p: from_ascii(OUTPOST).save(p),
    'dome': lambda p: from_ascii(DOME).save(p),
    'ice_seam': lambda p: from_ascii(ICE_SEAM).save(p),
    'stake': lambda p: from_ascii(STAKE).save(p),
}


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    sprites = os.path.join(root, 'assets', 'sprites')
    os.makedirs(sprites, exist_ok=True)

    for name, fn in SPRITES.items():
        path = os.path.join(sprites, name + '.png')
        fn(path)
        print('  sprites/%s.png' % name)

    for zone, seed in (('sunlit', 1001), ('transition', 2002), ('psr', 3003)):
        path = os.path.join(sprites, 'terrain_%s.png' % zone)
        terrain_strip(path, 320, 200, zone, seed)
        print('  sprites/terrain_%s.png' % zone)

    starfield(os.path.join(sprites, 'starfield.png'), 256, 256, 4242)
    print('  sprites/starfield.png')

    dither_panel(os.path.join(sprites, 'panel_dark.png'), 64, 64, 'E', 'z')
    dither_panel(os.path.join(sprites, 'panel_lit.png'), 64, 64, 'C', 'D')
    print('  sprites/panel_dark.png, panel_lit.png')

    # A contact sheet for review.
    names = sorted(os.listdir(sprites))
    thumbs = []
    for n in names:
        if not n.endswith('.png') or n == 'contact_sheet.png':
            continue
        im = Image.open(os.path.join(sprites, n)).convert('RGBA')
        # Composite over a checkerboard so transparency is visible.
        bg = Image.new('RGBA', im.size, (24, 26, 32, 255))
        for yy in range(0, im.height, 8):
            for xx in range(0, im.width, 8):
                if ((xx // 8) + (yy // 8)) % 2 == 0:
                    for y2 in range(yy, min(yy + 8, im.height)):
                        for x2 in range(xx, min(xx + 8, im.width)):
                            bg.putpixel((x2, y2), (40, 44, 54, 255))
        thumbs.append((n, Image.alpha_composite(bg, im)))

    cols = 4
    cw = max(t[1].width for t in thumbs) + 8
    ch = max(t[1].height for t in thumbs) + 8
    rows = (len(thumbs) + cols - 1) // cols
    sheet = Image.new('RGBA', (cols * cw, rows * ch), (16, 18, 22, 255))
    for i, (n, im) in enumerate(thumbs):
        sheet.paste(im, ((i % cols) * cw + 4, (i // cols) * ch + 4), im)
    sheet.save(os.path.join(sprites, 'contact_sheet.png'))
    print('  sprites/contact_sheet.png  (review this)')

    print('\nDone. %d sprites in assets/sprites/' % len(thumbs))
    return 0


if __name__ == '__main__':
    sys.exit(main())
