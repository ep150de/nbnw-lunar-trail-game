# Lunar Trail

**The Oregon Trail, reflight.** An open-source survival-logistics game about
driving one hundred and twenty kilometres across the lunar south pole, from a
landing site to a colony that is waiting to find out whether you arrived.

C++17 · SDL2 · 320×200 pixel art · Apache-2.0

---

## What it is

You are the flight leader of a five-person surface convoy. One hundred and
twenty kilometres of broken ground lie between you and a site where two hundred
people are waiting.

You have water, oxygen, power and propellant. You do not have enough of any of
them. That is not a bug; it is the job.

The game is a structural homage to *The Oregon Trail* (MECC, 1971–1990). It
keeps the original's architecture — a credit budget that never refills, outposts
whose prices inflate at every stop, a hunting minigame you cannot avoid, a
three-way repair decision, and nine ways to die — and relocates the whole thing
to a pressurised rover on the lunar south pole.

Most convoys do not arrive. The ones that do are not lucky. They planned.

---

## Building

Requires **SDL2**, **SDL2_image**, **SDL2_ttf**, **SDL2_mixer**, and a C++17
compiler. No other dependencies, and nothing is fetched at build time.

```sh
# Debian / Ubuntu
sudo apt install build-essential cmake libsdl2-dev libsdl2-image-dev \
                     libsdl2-ttf-dev libsdl2-mixer-dev

# Fedora
sudo dnf install gcc-c++ cmake SDL2-devel SDL2_image-devel \
                 SDL2_ttf-devel SDL2_mixer-devel

# Arch
sudo pacman -S base-devel cmake sdl2 sdl2_image sdl2_ttf sdl2_mixer

# macOS
brew install cmake sdl2 sdl2_image sdl2_ttf sdl2_mixer

# Windows (vcpkg)
vcpkg install sdl2 sdl2_image sdl2_ttf sdl2_mixer
```

Then:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
./build/lunar_trail
```

### Regenerating the art

All artwork is generated from source in `tools/`, and the generated files are
committed so a plain clone builds and runs.

```sh
python3 tools/gen_font.py     # 5x7 bitmap font + PNG proof sheet
python3 tools/gen_art.py      # sprites, terrain strips, contact sheet
```

Both need Pillow (`pip install pillow`). After running `gen_art.py`, look at
`assets/sprites/contact_sheet.png` to review everything that was drawn.

### Tests

```sh
cmake -S . -B build -DLUNAR_TRAIL_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The suite covers the load-bearing invariants of the design rather than incidental
behaviour — the health model's update order, the second-malady rule, the 25%
outpost markup, the five-credits-per-point economy property, the ISRU
stoichiometry, route geometry, chasm availability, and determinism.

### Balance harness

The economy was tuned from measurements, not from reasoning.

```sh
cmake --build build --target balance_harness
./build/balance_harness
```

It plays hundreds of full runs with scripted policies and reports win rates per
profession. If you change a number in `src/sim/balance.h`, re-run this and check
the gradient still descends. A change that lifts all three tiers to a high win
rate has removed the difficulty rather than balanced it.

---

## Playing

Controls are listed in full in the in-game **Field Manual** (main menu, option
2), which is the intended way to learn the game.

Short version:

| | |
|---|---|
| Arrows / WASD | Navigate menus |
| Mouse | Click menus |
| Enter / Space | Confirm |
| Escape | Back one level |
| F11 | Fullscreen |
| M | Toggle sound |

### The shape of a run

**Chapter One — Ascent.** Build the stack in low Earth orbit, catch a
trans-lunar injection window, insert into lunar orbit, and fly a powered descent
onto the south pole rim. What you spend getting there is what you land with.
Most missions end here.

**Chapter Two — The Trail.** One hundred and twenty kilometres. Solve water,
oxygen, power and propellant every sol. Arrive with enough cargo to seed a
colony.

### Three things that will decide your run

**Water is the whole economy.** Ice is water. Water is drinking, water is
life-support oxygen, water is rocket oxidiser. 5 crew drinking 3.5 L a sol over a
72-sol route is 1,260 kg of it, against a 2,100 kg payload bay that also has to
carry 630 kg of food, 200 kg of oxygen, 95 kWh of batteries and the propellant to
get there. Water is not the largest number; it is the one with no substitute at
all, which is why every run still ends up spending sols at an ice face.

**Energy and propellant are anti-correlated by geography.** The sunlit rim
produces 220 kWh a sol and has no ice. The shadowed regions have the ice and
produce exactly zero. No zone gives you both, so you are permanently trading
power against propellant and the map decides which you're allowed to have.

**The shadow line is a wall, not a slope.** Batteries hold 900 kWh, about seven
sols in the dark. Past sol 110 the crew rotation window closes and nobody is
coming.

---

## Design

`docs/DESIGN.md` records the full design, including the mapping from every
original mechanic to its counterpart and the five properties that make the game
hard. `docs/BALANCE.md` is the numeric reference, with every tunable in one
place and the reasoning for the values that are not obvious.

Some choices worth flagging up front:

- **The route seed is displayed, and typed back in, and does the same thing.**
  Every random roll for a run is fixed by it. This is deliberate: the 1971
  Oregon Trail was fully deterministic (its source contains no `RANDOMIZE`), and
  the property that made it feel *fair* was that a death was attributable to a
  decision and the route was learnable.
- **A second malady is fatal.** Preserved verbatim from the original, and the
  hardest rule in the game. The instinct to push through is exactly wrong.
- **Hazards do not fire while you are resting.** Also preserved, and also a real
  known exploit.
- **Your wrecks persist.** Every dead convoy writes a record — crew name,
  distance, sol, epitaph — that appears on the route of every future run, where
  it is also a resupply point. Your failures become the next player's supplies.
  This was the original's most-loved feature.

---

## Assets

**Every asset is original or CC0.** Nothing is traced, ripped, or sampled from a
game asset pack. All artwork is generated from ASCII-art definitions and
procedural code in `tools/`, so it is reproducible and diff-able.

`assets/sfx/` is intentionally empty — the game loads CC0 audio if you put files
there and runs silently otherwise.

**No NASA imagery is included.** NASA content was consulted as an external
design reference (the Lunar South Pole Visualizations gallery and LROC imagery of
Shackleton crater, for the palette and the placement of shadows) but none of it
is committed. NASA material is not CC0 and not a public-domain dedication; it is
uncopyrightable as a U.S. government work, which is a different thing, and the
insignia and mission patches are restricted regardless. Excluding it means this
repository can say truthfully that every asset in it is CC0.

Full audit: [`assets/NOTICE-ASSETS.md`](assets/NOTICE-ASSETS.md).

---

## Licence

Source code is **Apache-2.0** — see [`LICENSE`](LICENSE). Note that the licence
covers code only; art and fonts are CC0 and are documented separately in
[`assets/`](assets/).

The game is a homage to *The Oregon Trail*, originally designed by Don Rawitsch,
Bill Heinemann and Paul Dillenburger at MECC. No MECC code, art, text or audio is
included here. This project is unaffiliated with MECC and with the current
rights-holders of the Oregon Trail name and marks.

---

## Repository layout

```
src/core/      renderer, input, RNG, JSON, persistence, audio
src/sim/       the simulation: no SDL, fully headless and testable
src/game/      scenes and UI
tools/         asset generation, balance harness, UI capture test
tests/         the invariant suite
content/       balance overrides
assets/        generated sprites, font, notices
docs/          design and balance reference
```

The `src/sim/` layer contains every rule and depends on nothing but the standard
library. That is what makes the game testable and what makes the balance harness
possible — a run can be simulated end to end without a window.
