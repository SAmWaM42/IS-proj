# Phase 3 — Transition Semantics and Experiment Trace Identity

**Status:** Local CMake build, full self-test suite, and 5-episode smoke training run verified by the developer. Generated trace/summary field inspection is still pending.  
**Parent plan:** [Docs/19-training-and-evaluation-redesign-spec.md](19-training-and-evaluation-redesign-spec.md), Stage 0.

## Scope of this increment

This is the first small Stage 0 correctness increment. It does not claim to complete the full training redesign or produce research-grade simulation results.

### Transition end contract

`ActionResult` now distinguishes:

- `terminated`: the environment reached a true terminal state. Q-learning must not bootstrap from a next-state value.
- `truncated`: the episode ended because of a time or step limit. If a final observation exists, Q-learning should bootstrap from it.

A transition cannot be marked both terminated and truncated. The current training runner does not yet model a true terminal world state; reaching the configured maximum number of steps is therefore recorded as truncation.

### Stable action identity and logs

The canonical action vocabulary now has explicit stable numeric IDs. IDs no longer depend on an action's position in the vector, so adding or reordering entries does not silently redefine the meaning of a logged ID.

Training step records include `schema_version`, `step_index`, `action_id`, `terminated`, and `truncated`. The old `step` field remains as a backwards-compatible alias for `step_index`; it is not an action identifier. Run summaries record the current state/action version labels and the learning/exploration settings.

## Verification added

- A Q-learning regression check seeds a known next-state value and verifies that a truncated transition bootstraps from it.
- An action-ID regression check verifies uniqueness across the canonical action set and the unknown-action sentinel.

## Local verification completed; trace inspection pending

From the project root on this branch:

```sh
cmake --build build -j"$(nproc)"
./build/IS-2 --self-test
./build/IS-2 --train --episodes 5 --steps 7 --seed 42 --out /tmp/is2-phase3-smoke
```

The developer confirmed that the build succeeded, all listed self-tests passed, and the smoke training command completed. Still inspect the generated JSONL to confirm each episode's final step has `truncated: true`, `terminated: false`, a stable `action_id`, and a distinct `step_index`; inspect `train_summary.json` for schema/version labels and hyperparameters.

## Intentionally deferred

The broader Stage 0 exit gate is not met yet. Typed observation/scenario builders, invariant validation, an independent reward-table oracle, complete state/action visit counts, full checkpoint compatibility checks, and the far-fetch reward/completion audit remain for subsequent increments. The stochastic hidden-user model, held-out scenario split, and multi-seed evaluation also remain later stages.

The training reward and alignment metrics remain engineering diagnostics; they are not measures of perceived believability.
