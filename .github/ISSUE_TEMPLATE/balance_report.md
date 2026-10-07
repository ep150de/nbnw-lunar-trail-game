---
name: Balance report
about: A number feels wrong
labels: balance
---

Balance is tuned from measurement, not opinion — `tools/balance_harness.cpp`
plays hundreds of runs and reports win rates. Before filing, run it:

```sh
cmake --build build --target balance_harness
./build/balance_harness
```

**What feels wrong**

**Which resource or mechanic**: water / oxygen / energy / propellant / pace /
health / hazards / chasms / outpost prices / budgets

**Evidence** — a run, a seed, or harness output.

**Proposed change** and what it would break.
