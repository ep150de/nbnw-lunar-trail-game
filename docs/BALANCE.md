# Balance reference

Every tunable number in the game, and the reasoning for the ones that are not
obvious.

The authoritative source is `src/sim/balance.h`. This document explains it.
Nothing here needs to be kept in sync by hand — it is a design record.

---

## How this was tuned

Not by reasoning. `tools/balance_harness.cpp` plays hundreds of complete runs
with scripted policies and reports win rates per profession. Every value below
was chosen from a measurement that harness produced.

```sh
cmake --build build --target balance_harness
./build/balance_harness
```

Current output:

| Profession | Credits | Win rate | Avg sols | Avg km |
|---|---|---|---|---|
| Flight Director | 16,000 | ~20% | 86 | 101 |
| Mission Specialist | 8,800 | ~13% | 94 | 99 |
| Payload Technician | 5,800 | ~4% | 95 | 98 |

That is the intended shape: hard, every tier theoretically winnable, hardest tier
requiring the most correct play.

**If you change a number here, re-run the harness and check the gradient still
descends.** A change that lifts all three tiers to a high win rate has removed
the difficulty rather than balanced it.

---

## Convoy

| | Value | Why |
|---|---|---|
| Dry mass | 2,650 kg | crew cab + chassis + wheels + batteries + RCS |
| **Payload cap** | **2,100 kg** | the number the whole economy pivots on — see below |
| Traction units | 6 | below six costs speed; sick units count as half |
| Drive bogies | 3 | each failure roughly halves the drive |
| Spares per type | 3 | the original's three wheels / three axles / three tongues, preserved |

### Why 2,100 kg

Two invariants constrain it, and both are asserted by the test suite:

1. **Consumables for a typical run must nearly fill the bay.** At 27.25 kg/sol
   over 72 sols that is 1,962 kg — 93% of the cap. Consoles fit alone.
2. **A survivable load must not fit.** Adding propellant and the 50 kg minimum
   colony cargo pushes past 2,100 kg.

The ~140 kg gap is the mining requirement: about four to six sorties per run, a
fifth of the journey. That is the ratio the original's hunting occupied.

Cap 1,700 was tried and rejected — it forced ~28 sorties, more prospecting than
driving. Cap 2,600 was tried and rejected — it let a run carry the route without
mining at all, which removes the game's core tension.

---

## Daily consumption

Per crew member, per sol:

| | Rate | ×5 crew |
|---|---|---|
| Food | 1.4 kg | 7.0 kg |
| Water | 3.5 L | 17.5 kg |
| Oxygen | 0.55 kg | 2.75 kg |
| Energy | 19.0 kWh | 95 kWh |
| MMOD suit wear | 0.03 set | 0.15 set |

**Total: 27.25 kg and 95 kWh per sol.**

Water was originally 4.5 L/crew. At 45% of a 60 kg haul being drinkable, covering
a route needed ~100 prospecting sols — more than the route itself. The figure
above, with a 90 kg cap and a 55% drink fraction, puts prospecting at five
sorties.

Oxygen was 0.85 and is 0.55, for a specific reason: at 0.85 a five-person crew
consumed 4.25 kg/sol while the best possible sortie yielded 3.3 kg of O₂. The
electrolysis loop was decorative — the convoy could only ever run out. At 0.55
mining for oxygen is worth doing, which is the design intent.

---

## Pace

| Pace | Factor | km/sol | Energy | Character |
|---|---|---|---|---|
| **Cruise** | 1.0 | 3.0 | 28 kWh | electric, sustainable, gentle on the rig |
| **Sprint** | 2.0 | 6.0 | 52 kWh | electric, heavy; wears drive gear |
| **Full Burn** | 4.667 | 14.0 | 85 kWh | chemical; eats propellant and battery both |

Full Burn is the only pace that reliably beats the shadow line, and it is also
the fastest route to Critical health (7.0/sol vs 0.0 for Cruise). That trade is
the game.

A sol never overshoots the next landmark — the convoy stops there, which is why
a fast pace into a nearby landmark legitimately yields less than a nominal day.
The HUD shows both the capability and the actual, so pace choice is a
calculation rather than a guess.

---

## Rations

| Rations | Multiplier | Health cost/sol |
|---|---|---|
| **Full** | 1.00 | 0 |
| **Reduced** | 0.60 | 3.0 |
| **Emergency** | 0.25 | 11.0 |

---

## Zones

| Zone | Solar | Ice/crew | Terrain |
|---|---|---|---|
| Sunlit Rim | 220 kWh/sol | 0 kg | ×1.00 |
| Transition Slope | 130 kWh/sol | 16 kg | ×0.85 |
| Shadowed Region | **0 kWh/sol** | 52 kg | ×0.75 |

Solar values were originally 140 / 60 / 0. That was incoherent: a cruising
five-person convoy draws 123 kWh/sol, so the sunlit rim had 17 kWh of headroom
(never enough to actually recharge) and the transition zone was a guaranteed
63 kWh/sol net drain — an unwinnable slide into the dark with no counterplay.

Current values make the gradient correct: sunlit comfortably self-sustaining,
transition break-even at Cruise, PSR entirely on the battery.

---

## Energy

| | Value |
|---|---|
| **Battery capacity** | **900 kWh** |
| Depot recharge | 450 kWh/sol, costs 2 sols |

A cruising convoy draws 123 kWh/sol (95 life support + 28 drive), so a full
battery is **7.3 sols in the dark** — or 3.6 on half a charge.

Battery capacity was 220 kWh, which gave 1.8 sols and made the manual's own claim
of "about eight sols" false. 900 kWh makes the claim true and makes the sunlit
zone genuinely worth routing through.

Depot recharge is free but costs sols, and sols are the only resource the south
pole does not manufacture. That is what makes a depot stop a decision rather
than a shop visit.

### Route geometry is constrained by this

Every depot-to-depot stretch through a shadowed region must fit inside one
battery. The current route's longest dark stretch is 10 km ≈ 547 kWh, inside the
900 kWh budget with margin.

An earlier version had three stretches needing 1,039 and 1,093 kWh. Those were
geometrically impossible at Cruise with no reachable recharge, and every run died
at kilometre 76–80 regardless of skill. Fixed by re-zoning rather than by
inflating the battery, so the shadow line stays a real constraint.

---

## Propellant

| | Value |
|---|---|
| Consumption | 3.0 kg/km at Full Burn |
| Mixture | LCH₄ : LOX = 1 : 6 |
| Ice required | ~8 kg per kg of usable propellant |

### The mixture lock

`propellantPerKm()` charges a penalty proportional to how far the tanks are from
the correct 1:6 ratio, so the same total propellant mass buys strictly less
distance when lopsided. `hasProportions()` gates Full Burn entirely: fuel with no
oxidiser ignites nothing.

This is the most common way players lose to themselves, which is why the UI shows
both bars rather than a combined total.

---

## Prospecting

| | Value |
|---|---|
| **Haul cap** | **90 kg per sortie** |
| Sortie cost | 1 sol + that sol's food, water, oxygen, 1 cutting charge |
| Split | 55% drinking water / 45% electrolyzed |
| Crew bonus | Geologist +40% |

The haul cap is the most important rule in the system and is preserved
deliberately from the original's 100 lb hunting limit: it teaches that you can
extract far more than you can carry, which is the whole of wastefulness
planning.

A shadowed-region sortie yields roughly 50 L water, 4.5 kg O₂, 9 kg LCH₄.

---

## Chasm crossings

Breakpoints are the original's river depths reinterpreted as metres:

```
depth <  2.5       low risk
2.5 .. 3.0         bog down: no losses, one sol lost
>  3.0             risk rises linearly with depth
```

| Method | Risk formula | Cost |
|---|---|---|
| Ramp Traverse | 0.03–0.06 shallow; up to 0.55 at depth | free |
| Grapple And Winch | `0.06 + span × 0.0030`, cap 0.45 | 1 sol, 40 kWh, 2 charges |
| Hop Assist | `0.05 + span × 0.0025`, cap 0.38 | propellant |
| Guided Crossing | 0.06 flat | 240 credits, may wait ≤6 sols |
| Local Guide | `(0.15 + depth × 0.020) × 0.20` | 3 suit sets |

Ramp risk was originally reaching 0.80 at 14 m, with a 35% catastrophic-failure
rate on top. Combined, the *free* option killed a quarter of all runs outright.
Rebuilt so the ramp is genuinely cheap on shallow crossings and merely dangerous
on deep ones — which is what makes the paid methods worth buying, and puts the
risk in *choosing the wrong method* rather than in rolling a coin.

Catastrophe is now 12% of failures above severity 0.55. Rare and dramatic, not
routine.

**Only two of five chasms have a guided crossing**, so three are always a
gamble or a purchase — the original's ferries existed at only two of four rivers.

---

## Health

The original's model, verbatim:

```
health = health × 0.90  +  Σ(modifiers)
```

0 is perfect, 140 is death. Bands: 0–34 Nominal · 35–69 Degraded · 70–104
Stressed · 105–139 Critical · 140 Failing.

**The ordering is load-bearing.** Recovery is applied *before* modifiers, which is
why a party at 139 lands at 125 rather than dying instantly — the original
manual's "they die within a few days" window. This is asserted by test.

| Modifier | Value |
|---|---|
| Pace (Sprint / Full Burn) | +2.5 / +7.0 |
| Rations (Reduced / Emergency) | +3.0 / +11.0 |
| Thermal (PSR / transition / sunlit) | +4.0 / +1.5 / +0.5 |
| Per maladied crew member | +1.5 |
| Oxygen at zero | +30.0 |
| Water at zero | +14.0 |
| Food at zero | +9.0 |
| Batteries flat | +9.0 |
| Batteries below 75% of a day's draw | +3.0 |

Illness odds run 0% at perfect health to 40% at worst. Ailments and diseases
recover in 10 sols; injuries in 30. **A second malady is fatal.**

The flat-battery penalty was 22/sol, which was a cliff: entering a shadowed region
low on charge became an unwinnable slide with no counterplay. At 9/sol it is a
slow squeeze, and the interesting failure becomes running the batteries flat with
a hundred kilometres still to cover.

Health reaching 140 kills the party. Without that rule a convoy could sit pinned
at 140 indefinitely, which is both wrong and an unintended way to survive.

---

## Economy

Prices are all multiples of 5 credits, because 5 credits = 1 point and the
economy is only balanced if that holds exactly for every good.

| Good | Depot price | Points |
|---|---|---|
| Water | 1.2 cr/L | 0.24 |
| Food | 3.0 cr/kg | 0.6 |
| Oxygen | 12.0 cr/kg | 2.4 |
| Fuel (LCH₄) | 10.0 cr/kg | 2.0 |
| Oxidiser (LOX) | 5.0 cr/kg | 1.0 |
| MMOD suit set | 60 cr | 12 |
| Cutting charge | 25 cr | 5 |
| Spare traction unit | 300 cr | 60 |
| Spare drive bogie | 600 cr | 120 |
| Spare seal kit | 240 cr | 48 |
| Colony cargo | 0.5 cr/kg | 0.1 |

Spares and suit sets go through `goodPoints()` like every other good, so they
score at 60, 120, 48 and 12 points respectively against prices of 300, 600, 240
and 60 credits. They previously carried separate constants (50 and 8) that no
longer matched those prices.

`goodPoints()` is **always** `price × pointsPerCredit`, with no per-good
special cases, so the invariant cannot be broken by adding a good or changing a
price.

### Markup

`price = base × (1 + 0.25 × outpostIndex)` — exactly the original's linear 25%:

| Outpost | Index | Multiplier |
|---|---|---|
| Shackleton Rim Depot | 0 | ×1.00 |
| Relay Post Krill | 1 | ×1.25 |
| Relay Post Shoemaker | 2 | ×1.50 |
| Relay Post Amundsen | 3 | ×1.75 |
| Relay Post Gerlache | 4 | ×2.00 |

Because 5 credits is 1 point whether spent or held, the markup is a pure
opportunity cost in days rather than in points. Restocking early is correct.

### Budgets

| Profession | Credits | ×score | Legs affordable |
|---|---|---|---|
| Flight Director | 16,000 | ×1 | covers the route unaided |
| Mission Specialist | 8,800 | ×2 | ~3 of 4 legs |
| Payload Technician | 5,800 | ×3 | ~1.8 of 4 legs — must mine |

The tiers are a **budget gradient**, exactly as in the original where the
professions were chosen to create three difficulty levels and nothing else.

A depot leg at markup costs ~2,060 credits including a full set of spares. The
Payload Technician cannot cover the route without mining, which is the design.

Budgets were originally 4,200 / 2,100 / 900, which no profession could use: a
convoy that spent its budget at the first depot arrived at the second broke, and
the markup means the water it needs is 25% dearer than what it just paid.

---

## Journey

| | Value |
|---|---|
| Route length | 120 km, 16 landmarks |
| Minimum sols | 45 |
| Typical sols | 72 |
| **Hard sol limit** | **110** |

The hard limit is a wall, not a slope: past sol 110 the crew rotation window
closes and nobody is coming. It is the lunar answer to the original's blizzard
in the mountains, and it sits above a typical run so it is a real deadline
rather than the expected outcome.

### The route

| km | Landmark | Zone | Store |
|---|---|---|---|
| 0 | Shackleton Rim Depot | Sunlit | ×1.00 |
| 7 | Argo Chasma | Sunlit | — |
| 14 | Ilus Field | Sunlit | — |
| 22 | Relay Post Krill | Transition | ×1.25 |
| 31 | De Gerlache Rift *(guided)* | Transition | — |
| 39 | Faustini Rim | Transition | — |
| 48 | Relay Post Shoemaker | Transition | ×1.50 |
| 57 | Haworth Break | Transition | — |
| 66 | Levi Massif | **Shadowed** | — |
| 76 | Relay Post Amundsen | **Shadowed** | ×1.75 |
| 86 | Nobile Sill | Transition | — |
| 90 | Amundsen Chasma *(guided)* | Transition | — |
| 96 | Upper Route | Transition | — |
| 104 | Relay Post Gerlache | Transition | ×2.00 |
| 112 | Columbia Chasma | Transition | — |
| 120 | Horizons Colony Site | Sunlit | — |

Nobile Sill is the branch point: the Upper Route adds distance and skips the
Amundsen Chasma, mirroring South Pass → Fort Bridger (long) or Green River
(short). The dark stretch is 66–76 km, at the shadow lip, where you can see the
dark before you are in it.

---

## Hazards

23 events, weighted by geography. Base chance of any event per sol of travel:
**13%**.

Representative frequencies on a healthy run, per 100 sols of travel:

| Event | Rate | Notes |
|---|---|---|
| Dust Storm | 9.9 | ×1.4 at Sprint |
| Boulder Field | 8.1 | ×1.5 at Full Burn |
| Derelict Cache | 7.7 | a *gain*; the positive counterpart to the original's abandoned wagon |
| Crew Injury | 6.9 | ×1.8 on Emergency rations |
| Scavenger Raid | 6.8 | ×1.7 in the first 30 km |
| Micrometeorite Impact | 6.0 | costs O₂ and charge |
| Traction Unit Failure | 5.9 | ×0.5 with a Mechanic |
| O2 Loop Contamination | 5.6 | — |
| Prior Convoy Wreck | 5.3 | a wreck marker on the map |
| Seal Breach | 5.3 | ×2 at a chasm |
| Drive Bogie Failure | 3.3 | ×0.5 with a Mechanic |
| Bay Fire | 3.3 | destroys food and suits |
| Impassable Slope | 2.9 | costs 2 sols, charge, cargo |
| Wrong Trail | 2.5 | large distance loss |
| Solar Particle Event | 1.2 | **×0.15 in a PSR** |
| Gyro Drift | 0.5 | ×0.15 with Comms |
| Nav Beacon Lost | 0.5 | ×0.15 with Comms |

---

## Descent

| | Value |
|---|---|
| Start | 15,000 m altitude, −1,700 m/s vertical, +40 m/s horizontal |
| Lander dry mass | 1,000 kg |
| Descent propellant | 1,600 kg |
| Thrust / Isp | 45 kN / 311 s |
| Max tilt | 20° |

Limits: 3 m/s vertical, 2 m/s horizontal, 15° slope.

Descent starts from a near-circular 15 km orbit. Ideal Δv from the rocket
equation is `311 × 9.80665 × ln(2600/1000)` ≈ 2,354 m/s, so 1,600 kg of
propellant covers it with margin — which is the point: arriving without it is
the classic lunar death, and it is entirely arithmetic.

---

## Overrides

`content/balance.json` overlays any field in `Balance`. Absent keys keep their
defaults, so the file can be partial. It ships with commented examples and no
active overrides.
