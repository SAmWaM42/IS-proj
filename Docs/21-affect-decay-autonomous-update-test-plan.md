# Test Branch: Affective Decay and Autonomous Environment Updates

**Branch:** `test/affect-decay-autonomous-updates`  
**Base:** `Dev`  
**Status:** Implementation candidate; local compile/runtime verification pending.

## Scope

This branch prototypes two missing behaviours:

1. **Time-driven affect decay** in the proposed agent.
2. **Autonomous action reconsideration** while the user is idle, using an ambient environment observation.

The changes are deliberately isolated from `Dev` so they can be built and reviewed before merge.

## Affective decay rules

`AffectiveState::decay(elapsedSeconds)` relaxes transient affect dimensions toward explicit resting values using exponential relaxation:

`x(t + dt) = x(t) + (baseline - x(t)) * (1 - exp(-rate * dt))`

Initial, uncalibrated engineering defaults:

| Variable | Resting value | Rate | Interpretation |
|---|---:|---:|---|
| Arousal | 0.25 | 0.12 | Fastest relaxation |
| Stress | 0.15 | 0.04 | Slower relaxation |
| Engagement | 0.35 | 0.015 | Gradual reduction during inactivity |
| Affinity | 0.50 | 0.002 | Very slow drift; retains longer-term social state |
| Energy | Environment-owned | — | Not changed by affect decay; should respond to rest, exertion, and feeding |

Rates use seconds and are initial implementation parameters, not research-derived constants. The method clamps state to [0, 1], ignores non-positive/non-finite elapsed time, and limits a single update to 60 seconds to avoid large time jumps. Valence becomes neutral after arousal and stress fall below their settling thresholds. Semantic memory records are not changed by decay.

## Autonomous environment updates

The simulation advances proposed-agent affect every UI frame using elapsed simulation time. When the visible behaviour finishes and no user interaction is queued, the UI waits five seconds and builds an ambient observation with no active task or user feedback. It updates target proximity and visible food/play affordances, asks both policies to select an action from that same observation, and renders the selected A/B condition.

Autonomous decisions do **not** fabricate a user reward, interaction-history entry, or semantic-memory record. The other policy is evaluated on the same ambient observation for a paired comparison. The existing environment/action contract still determines which actions are valid.

## Verification checklist

Run from a fresh checkout of this branch:

```sh
cmake -S . -B build
cmake --build build -j2
./build/IS-2 --self-test
ctest --test-dir build --output-on-failure
./build/IS-2 --train --episodes 1000 --steps 40 --seed 42 --out artifacts/training-test-branch
./build/IS-2 --validate --episodes 200 --steps 40 --seed 43 --out artifacts/training-test-branch
./build/IS-2 --compare --episodes 200 --steps 40 --seed 44 --out artifacts/training-test-branch
```

Manual UI verification:

- Leave the simulation idle for at least ten seconds; confirm an autonomous action occurs approximately every five seconds after the previous animation ends.
- Confirm researcher mode logs autonomous decisions and participant-facing mode does not show researcher-only action labels.
- Confirm user feedback is not logged as having occurred during an autonomous action.
- Confirm arousal/stress move toward their resting values, engagement changes more slowly, and affinity changes very slowly.
- Confirm memory records are unchanged by elapsed-time updates.
- Confirm energy does not change merely because affect decays; environment action outcomes must own energy changes.

## Known limitations / review gates

- **Not locally verified in this editing session.** GitHub edits do not establish that the branch compiles or that tests pass.
- The five-second interval and decay rates need to be reviewed against the intended simulation timescale and, if appropriate, calibrated before participant evaluation.
- The UI currently derives ambient proximity from the companion's distance to the ball. Review whether this is the correct observable target for all ambient situations.
- Autonomous actions are rendered, but this prototype does not yet apply a complete, shared environment-transition model to every autonomous action. In particular, energy expenditure/recovery must be handled by the environment contract rather than affect decay.
- The baseline and proposed agent receive the same ambient observation, but only the proposed agent has time-decaying affect. This is intentional for the treatment comparison.
- Do not use participant data or describe the prototype as research-validated before required ethics/consent approval and a verified build.
