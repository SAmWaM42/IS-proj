# WP1 — Training Instrumentation

**Branch:** `Dev`  
**Scope:** Diagnostics and provenance only. No reward, state, action-selection, exploration, or deployment behaviour is changed.

## Build and test

From the repository root on `Dev`, reconfigure so the executable embeds the current revision and build metadata, then build and run the existing test suite:

```bash
git pull origin Dev
cmake -S . -B build -DANIMAL_AGENT_BUILD_RESEARCH_TESTS=ON
cmake --build build -j"$(nproc)"
./build/AnimalAgent --self-test
ctest --test-dir build --output-on-failure
```

The instrumentation does not change reward values, state encoding, exploration, action selection, or Q-learning update rules. A successful local build and test run is still required before treating this commit as verified.

## Outputs

For each `--train`, `--resume-training`, `--validate`, or `--compare` run, the selected output directory receives:

- `<mode>_summary.json`: existing episode/run metrics plus full-run instrumentation aggregates.
- `<mode>_diagnostic_events.jsonl`: targeted events for INSIST eligibility/availability/selection/execution attempts and repetition penalties/resets/category changes.
- `<mode>_run_manifest.json`: arguments, seeds, build identity, executable path/size/FNV-1a-64 checksum, and policy-file checksums when present.
- `<mode>_baseline_steps.jsonl` and `<mode>_proposed_steps.jsonl`: detailed rows for the existing final-two-episode window; both now include valid-action Q estimates.

Use a new output directory for every run to preserve provenance and avoid artifact rotation. Example:

```bash
RUN_DIR="artifacts/stage-a1-instrumentation/train-$(date -u +%Y%m%dT%H%M%SZ)"
./build/AnimalAgent --train --episodes 200 --steps 40 --seed 42 --out "$RUN_DIR"
```

Then validate using the saved policies in that same directory, but use a distinct output directory because the current CLI expects policy files in the output directory. Copy the two Q-table files into a fresh validation directory before invoking validation:

```bash
VALIDATE_DIR="artifacts/stage-a1-instrumentation/validate-$(date -u +%Y%m%dT%H%M%SZ)"
mkdir -p "$VALIDATE_DIR"
cp "$RUN_DIR/baseline.qtable" "$RUN_DIR/proposed.qtable" "$VALIDATE_DIR/"
./build/AnimalAgent --validate --episodes 50 --steps 40 --seed 43 --out "$VALIDATE_DIR"
```

## Metric definitions

- **INSIST predicate eligible:** `isActionValidForObservation("INSIST", observation)` returns true.
- **INSIST available:** `INSIST` appears in `validActionsForObservation(observation)`. A mismatch between these counts is separately recorded.
- **Policy selection:** proposed agent's action before memory/preference overrides; baseline's selected policy action.
- **Final selection:** action after any proposed-agent overrides.
- **Execution attempt:** final `INSIST` action is passed into the environment transition function. This is not a claim that an insist was accepted or produced a meaningful response; the current environment transition does not expose a separate accepted/rejected INSIST result.
- **Penalty trigger:** repetition penalty is greater than zero on that decision.
- **Within-state Q spread:** maximum minus minimum Q estimate across at least two valid actions with explicitly stored Q-table entries. Default-zero values for unvisited actions are excluded.
- **Repeated-action Q gap:** best alternative Q estimate among explicitly stored valid actions minus the repeated action's Q estimate. Both the repeated action and at least one alternative must have stored entries. Positive means the alternative has a higher learned value.
- **All valid-action Q estimates:** the step/event records retain estimates for every valid action, including default-zero estimates where no Q entry exists. `stored_valid_action_names` identifies which estimates were actually learned/stored.

Run-wide distributions include sample count, mean, population standard deviation, minimum, p25, median, p75, p90 and maximum. The summary includes the total number of observed decisions, cumulative penalty, mean penalty per decision and mean penalty per penalty-triggering decision. Per-episode diagnostic aggregates are also embedded in each agent's episode entry.

## Interpretation boundaries

These are engineering diagnostics, not believability measures. Q gaps describe the stored action-value estimates at decision time; they do not by themselves establish the counterfactual effect of subtracting the penalty or prove that the penalty changed a later greedy decision. INSIST execution attempts do not imply acceptance. The event log is intentionally selective; the summary aggregates cover every decision in the run.

The executable checksum is FNV-1a-64 for artifact identity, not a cryptographic integrity guarantee. Q-value distribution summaries use explicitly stored entries only, avoiding treating an unvisited action's default zero as a learned estimate. The manifest records the Git revision and whether the working tree was dirty at CMake configure time. Reconfigure after changing source or branch to refresh these compile-time values.
