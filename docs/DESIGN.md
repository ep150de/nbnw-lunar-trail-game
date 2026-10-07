# Lunar Trail — design

A structural homage to *The Oregon Trail* (MECC, 1971–1990), relocated to a
pressurised surface convoy crossing the lunar south pole. This document records
what the game is, why it is shaped the way it is, and which properties are
load-bearing.

---

## 1. What makes the original hard

The original was hard for five compounding reasons. All five are reproduced
deliberately, because they are structural rather than a difficulty curve — there
is no easy mode in *Lunar Trail* and no slider for "how punishing".

| # | The original's cruelty | The lunar equivalent |
|---|---|---|
| 1 | **Credits are the only non-renewable resource.** There is no way to earn them, ever. | Identical. No ISU credits, no income, no exceptions. |
| 2 | **Outpost prices inflate +25% per fort**, so every day's delay is a compounding cost. | Identical formula, verbatim: `price = base × (1 + 0.25 × index)`. |
| 3 | **The player physically cannot buy enough consumables** and must spend 30–80 days foraging. A Farmer at $400 cannot feed five people for 150 days. | Identical. Consumables for a typical run are 1,962 kg against a 2,100 kg bay, and that excludes propellant and cargo. The Payload Technician cannot afford a depot leg at markup. |
| 4 | **`already sick → dies`.** A second malady is instant death; recovery takes 10–30 days. | Preserved verbatim. A second malady is fatal. The Physician is the only exception, and only once. |
| 5 | **Winter is a hard wall.** Snow zeroes speed, snowbound halts you, and the 1971 ancestor simply killed you: *"YOUR FAMILY DIES IN THE FIRST BLIZZARD OF WINTER."* | **The shadow line.** Zero solar in a shadowed region, ~7 sols of battery, and past sol 110 the crew rotation window closes. |

And the one mercy valve, preserved: the original's *"An Indian helped you find
some food"* (+30 lbs, fires only at literally zero food, only between landmarks,
only while travelling). Here it is a salvage cache, and it is deliberately too
late to rely on.

**The governing rule:** do not balance consumption downward toward purchasable.
That removes the game. If a player can buy their way across, the game is a
shopping list.

---

## 2. The mechanic that makes it this game: water is the whole economy

The ISRU mass balance drives every number in Chapter Two. The real
stoichiometry:

```
Electrolysis   2 H₂O → 2 H₂ + O₂
               1 kg water → 0.111 kg O₂ + 0.889 kg H₂

Sabatier       CO₂ + 4H₂ → CH₄ + 2H₂O
               CO₂ comes from processed regolith and is free

LCH₄ : LOX     1 : 6 by mass
```

Two consequences are kept because they are *true*, not because they are fun:

**Fuel is cheap; oxidiser is not.** Making LCH₄ on the Moon costs about
0.375 kg of water per kg of fuel. Making LO₂ costs about 9 kg. And in-situ
propellant has far lower specific impulse than propellant made in a factory on
Earth, so making it costs payload as well as ice. The intended expert play is
therefore: **buy the oxidiser, make your own fuel, mine water for breathing.**

**Oxygen is three jobs.** Every kilogram in your tanks is either ~0.20 sols of
five crew's breathing, or propellant for a chemical burn, or neither. There is
no third option.

### The oxidiser mixture lock

LCH₄ and LOX burn in a 1:6 ratio. Fall out of it and you are carrying dead
weight: you cannot ignite anything, and the game charges a penalty on
propellant-per-kilometre so that a lopsided pair of tanks buys strictly less
distance than the same total mass in proportion. A full tank of one and an empty
tank of the other is the single most common way players lose to themselves, which
is why the UI shows both bars rather than a combined total.

---

## 3. Zones: energy and propellant are anti-correlated

This is the standout mechanic, and it is a real property of the lunar south pole.

| Zone | Solar | Ice | Character |
|---|---|---|---|
| **Sunlit Rim** | 220 kWh/sol | 0 kg/crew | Self-sustaining. All propellant carried in. |
| **Transition Slope** | 130 kWh/sol | 16 kg/crew | Break-even at Cruise, a cost at anything faster. |
| **Shadowed Region** | **0 kWh/sol** | 52 kg/crew | All power carried in. |

There is no zone that gives you both, so the map decides what you are permitted
to have and every route is a trade.

**The shadow line.** Batteries hold 900 kWh; a cruising convoy draws 123 kWh a
sol (95 for life support, 28 for the drive). That is about seven sols in the
dark, or three and a half on half a charge. Route geometry is constrained by
this: every depot-to-depot stretch through a shadowed region must fit inside one
battery, or the run is unwinnable regardless of skill. This is asserted by test.

---

## 4. Chapter One: the ascent

Chapter One is a **mass-budget puzzle**, structurally the store screen of the
original, which is where that game front-loaded its difficulty.

1. **Manifest** — three professions (difficulty tiers), five named crew, route
   seed.
2. **LEO refuelling** — three escalating windows. Everything you spend getting
   to the Moon is mass you do not land with.
3. **TLI** — the window opens sol 3 and closes sol 5.
4. **Cislunar cruise** — four days, nothing to decide, and your last chance to
   look at the mass readout.
5. **LLOI** — capture into 100×100 km near-polar.
6. **Powered descent** — the interactive minigame.

### On the descent minigame

The physics are real and thin, because at lunar gravity in vacuum there is
almost nothing to work with:

```
a  = T/m − g_moon
T  = throttle × maxThrust
dvx = −(T/m + g) · sin(tilt) · dt
dvz = (T/m · cos(tilt) − g) · dt
```

No drag, no atmosphere, and therefore no visual cue for how fast you are really
going. Apollo pilots flew these on landmarks and callouts. There is no cue here
except the numbers, which is the problem the player has to solve.

It is the hardest ninety seconds in the game, and it is where Chapter One is
actually spent.

**What kills you:** touchdown above 3 m/s vertical; more than 2 m/s
horizontal; on a slope over 15°; or out of propellant above the surface.

**The fairness affordances:** a HUD-projected predicted-intercept readout
(integrating to the surface at held throttle, which is the mental model a real
descent is flown on), and four landing sites with genuinely different problems —
flat, steep-but-aligned, flat-but-ninety-metres-long, and in shadow.

Without the intercept readout the descent would be unfair rather than hard. With
it, a player who understands the arithmetic can fly it. That distinction is the
whole difference between difficult and merely punishing.

---

## 5. Chapter Two: the trail

The classic nested cycle, ported faithfully:

- **Outer loop** — landmark to landmark (16 landmarks, 20 segments)
- **Inner loop** — one sol at a time

The trail screen autopilots between landmarks at roughly one sol every half
second. The player may pause at any time, and the game auto-pauses on anything
major.

### Order of operations per sol

This ordering is not cosmetic. It follows the original's inner loop and it
matters:

1. Collect solar
2. Spend power on life support and drive
3. Roll for malady
4. Consume food, water, oxygen
5. Roll a hazard
6. Compute distance
7. Advance the calendar

Illness is rolled **before** rations are consumed and before events fire, which
means a sol where you are about to run dry can still kill somebody first, and it
means the day's distance is computed from the resources you actually had rather
than the ones you were about to lose.

### The nine actions

The original's travel-screen menu, in its original order, with its original
command names preserved where the meaning carries over:

| # | Command | Notes |
|---|---|---|
| 1 | Continue on trail | context-sensitive: cross a chasm, or depart |
| 2 | Check supplies | writes the manifest to the log |
| 3 | Look at route map | includes the hazard forecast, if you have a Comms Officer |
| 4 | Change pace | Cruise / Sprint / Full Burn |
| 5 | Change rations | Full / Reduced / Emergency |
| 6 | Stop to rest | the only reliable healing |
| 7 | Attempt to barter | costs a sol |
| 8 | Talk to the crew | five crew, advice that is actually true |
| 9 | Prospect for ice | costs a sol and a day's life support |
| 10 | Recharge at depot | *no original counterpart* — see below |

The tenth entry exists because the original had no batteries. Everything else
maps one-to-one.

---

## 6. Prospecting is hunting

The same loop, the same rules, the same famous constraint.

**A sortie costs one sol and returns at most 90 kg of ice.** The haul cap is the
most important rule in the prospecting system and it is preserved deliberately:
the original imposed a 100 lb limit on hunting for exactly the same reason — so
that the player learns to think about wastefulness. You can mine far more than
you can carry.

A haul splits 55% as drinking water and 45% through electrolysis. In a shadowed
region a full sortie yields roughly 50 L of water, 4.5 kg of oxygen, and 9 kg of
LCH₄.

Prospecting is also not free: it spends that sol's food, water and oxygen. It is
a day of life support, not a button.

Because the payload bay cannot hold a route's worth of consumables, roughly four
to eight sols of a seventy-two sol run go to an ice face. That is the same
proportion of the journey the original's hunting occupied.

---

## 7. Chasm crossings are river crossings

The original had four rivers and a fixed algorithm:

```
depth <  2.5       low risk
2.5 .. 3.0         swamped: no losses, but a day lost drying out
>  3.0             loss probability rises LINEARLY with depth
```

with three crossing methods plus a guide. That structure is reproduced exactly,
with depth reinterpreted as chasma depth in metres, and a fourth method added
because a rover has an engine.

| Method | Cost | Character |
|---|---|---|
| **Ramp Traverse** | free, instant | the ford. Shallow is nearly safe; marginal bogs down and costs a sol; deeper, risk climbs linearly. |
| **Grapple And Winch** | 1 sol, 40 kWh, 2 charges | the float. Risk scales with span. |
| **Hop Assist** | propellant | *new.* Burns across. Fast, and it does not care about your wheels. |
| **Guided Crossing** | 240 credits | the ferry. Lowest risk, may wait up to six sols. |
| **Local Guide** | 3 suit sets | the Indian guide. −80% risk, and −80% losses. |

**Two structural details preserved from the original:**

- A guided crossing is available at only **two of five** chasms, so at least
  three are always a gamble or a purchase. The original had ferries at only two
  of four rivers, so at least two were always a gamble.
- The guide **picks the line and the timing for you, never waits, and will not
  warn you about the current.** This was the original's guide, and it was
  infuriating there too. It is kept as an homage.

---

## 8. Hazards are weighted by geography

The original's manual lists 23 events and states that "the probability of these
events occurring is not fixed but depends upon the current circumstances."
Bill Heinemann's contribution was tying event likelihood to place. Reproduced
here: every event carries a weight function evaluated against live state.

**Solar particle events** are six times likelier on the sunlit rim than in a
shadowed region — no sun means no particle flux reaching you.
**Battery thermal runaway** is 2.6× likelier on a chemical burn and 1.5× likelier
on a low charge. **Boulder fields and impassable slopes** scale with how hard you
are pushing. **Scavenger raids** are more likely early, where there are people to
raid.

**What is absent is load-bearing: there is no combat.** The 1985 design removed
the original's hostile-riders concept. Nothing here can be fought, chased, or
outrun. Every hazard is something you could only have prepared for. The only
counterplay is what you loaded at the depot.

### The hard rules

**Second malady is fatal.** Instant, no warning, no roll. Recovery is 10 sols for
ailments and 30 for injuries, so the natural instinct to push through is exactly
the wrong instinct. The Physician survives one such event — they cannot save the
same person twice.

**Hazards do not fire while resting.** Preserved. This is a real and known
exploit, and resting is the only reliable healing in the game.

**The three-way repair branch**, verbatim: a spare replaces the part instantly
and costs nothing; without a spare, a repair costs a sol and some charge;
without either, the unit is lost and the convoy limps on what it has left. That
last case is a *money* problem rather than an ending, which mirrors the original —
the trade option existed precisely so that a break was survivable if you could
afford a new part. Losing every traction unit, or every drive bogie, is fatal:
*"You have no more oxen."*

---

## 9. Crew and specialities

Five named crew, each carrying one speciality. Specialities **change rules**;
they are not stat bonuses.

| Speciality | Effect |
|---|---|
| **Physician** | one second malady is survivable. Once. |
| **Geologist** | prospecting yields +40% |
| **Engineer** | repairs almost always succeed |
| **Mechanic** | hardware failures half as likely |
| **Comms Officer** | reveals the route seed and an eight-sol hazard forecast |

A speciality is lost the moment its holder dies. The Geologist is the most
valuable perk in the game because prospecting is where runs are decided, and the
Comms Officer is the difference between playing a route and gambling on one.

The default manifest is one of each. You may swap, but the difficulty is
supposed to come from the economy, not from drawing a weak crew.

---

## 10. Determinism, and being honest about it

Research finding worth stating plainly: the 1971/78 BASIC original was fully
deterministic — the source contains no `RANDOMIZE` statement, verified. Whether
the 1985 Apple II remake was deterministic is **undocumented and unresolved**,
and this project does not claim it either way.

The design choice here is an **explicit, displayed, typed-back-in PCG32 seed**.
Every random roll for a run — every hazard, every repair, every crossing outcome
— is fixed by it. Replaying a seed reproduces the route exactly.

This is deliberate. The property that made the original feel *fair* rather than
arbitrary was that a death was attributable to a decision and the route was
learnable. Random entropy you cannot see defeats both. A visible seed makes the
game a puzzle you can solve by playing it again.

The forecast panel reveals the next eight sols — but only while a Comms Officer
is alive. Without one you are gambling every sol, which is the risk that buys the
perk its value.

---

## 11. Scoring, and the economy-wide balance

```
per surviving crew   500 Nominal / 400 Degraded / 300 Stressed / 200 Critical
colony cargo         1 point per 10 kg
spare traction unit  60 points      (300 credits)
spare drive bogie   120 points      (600 credits)
spare seal kit       48 points      (240 credits)
MMOD suit set        12 points       (60 credits)
credits held          1 point per 5
```

Spares and suit sets are not assigned a flat value. They go through the same
`price × pointsPerCredit` rule as everything else, because a flat per-spare figure
would make carrying a 600-credit bogie worth the same as a 240-credit seal kit
and would quietly reward the wrong repair part.

then multiplied by the profession: ×1, ×2, ×3.

Ranks: 500+ Trainee · 1500+ Expeditionary · 3000+ Trailblazer · 5000+ Colony
Founder.

The original balanced its entire economy so that **one point equals five
dollars**, whether you spent the money or held it, and every good's score value
was set so that converting five dollars into anything was worth exactly one
point. That property is preserved and, importantly, **derived rather than
hard-coded**: `goodPoints()` is always `price × pointsPerCredit`, with no
per-good special cases, so the invariant cannot be broken by adding a good or
changing a price.

This is what makes "restock early and often" a genuinely *correct* decision
rather than an obvious one — the markup is the only thing that scales, and it
compounds at every outpost you dawdle past.

---

## 12. Wrecks, and the Top Ten

Every dead convoy writes a record to the save file: crew name, distance reached,
sol, profession, seed, and an epitaph if you write one. Records persist across
every future run and appear on the route map as crosses.

**And they are scavengeable.** Somebody else's dead convoy is a resupply point.
Your failures become the next player's supplies. This was the original's
most-loved feature and it is load-bearing here — it is the only reason a repeated
attempt is emotionally different from the last one.

The Top Ten is seeded with the explorers of the heroic age of Antarctic polar
travel, spanning the plausible score range, so a mediocre run has somewhere to
sit. The original did the same thing with real emigrants.

---

## 13. What was deliberately left out

- **No difficulty settings.** The three professions *are* the difficulty setting,
  exactly as in the original, where they were chosen to create three tiers and
  nothing else.
- **No combat.** See above; its absence is the point.
- **No fast travel or undo.** The original had no undo and neither does this.
- **No way to skip a hazard once it fires.** Some things are simply the run
  ending, and knowing that in advance is part of the planning.
