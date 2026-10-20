# Research-validity implementation progress

**Branch:** `Dev`  
**Specification:** [08-research-validity-memory-affect-and-logging-spec.md](08-research-validity-memory-affect-and-logging-spec.md)  
**Status:** Implementation in progress; compilation and runtime validation still required.

## Implemented in this pass

- Proposed-agent memory and affect are saved beside the proposed Q-table:
  - `proposed.qtable.memory.json`
  - `proposed.qtable.affect.json`
- Proposed-agent `reset()` now resets transient session/history state without discarding memory or affect.
- The UI starts with an A/B selection screen. A/B-to-model assignment is randomised per application run and saved in a run-specific manifest for later analysis.
- The participant-facing simulation shows only the selected condition's action and dog animation. The model key is stored in the manifest, not printed on the selection screen.
- A selection-screen control clears the proposed agent's persisted memory file and in-memory records. It does not clear the learned Q-table or affect sidecar.
- Interactive simulation disables online Q-table learning and sets exploration to zero.
- Each interactive run writes its own JSONL interaction log and manifest. Records include timestamps, scenario observations, both actions/rewards, before/after state encodings, proposed affect before/after, memory candidates, and memory counts.
- A Python validator was added at `scripts/validate_interaction_log.py`.
- Training now uses a repeated, structured sequence of contextual scenarios rather than sampling every observation independently. Training logs include encoded state IDs and per-episode unique-state coverage.
- Agent outcome APIs accept an explicit next observation. Training uses that next observation for Q-learning's next-state update and passes the terminal flag rather than forcing every transition to terminal.
- Memory confidence now estimates the observed proportion of positive outcomes for an action record. The latest outcome is kept on that record, so a later positive result can update an earlier negative result instead of leaving a stale rejection marker forever.
- Negative feedback now updates arousal and energy as well as valence, stress, engagement and affinity.
- Regression tests were added for memory confidence, memory persistence across reset/save/load, negative-feedback affect transitions, and retaining an action rejection across intervening interactions.

## Important interpretation

The interactive UI still sends the same external observation and reward signal to both agents. Since the current UI does not simulate action-dependent world transitions, this is an equivalent-input paired comparison, not yet a dynamic cloned-world simulation. The training runner also replays the same observation sequence for both agents, with each agent receiving its own action-dependent reward.

Basic semantic preference memory is now supported for explicit text patterns such as “I prefer fetch/ball”, “I dislike barking/noise”, and “I prefer rest/sleep”. The UI carries detected preferences in the FIFO queue, records them only when that interaction reaches execution, persists them, and consults them during action selection. This is deliberately a narrow rule-based first pass, not general semantic understanding; the NLP service does not yet return structured preference facts itself.

## Validation status

No claim is made that this revision compiles or passes tests: the changes were pushed through the repository integration, but a build/test run has not been performed in this environment. Before relying on the changes:

1. Build the `Dev` branch and run `./IS-2 --self-test`.
2. Run `./IS-2 --train --episodes 1000 --steps 36 --seed 42 --out artifacts/training`.
3. Run `./IS-2 --validate --episodes 200 --steps 36 --seed 142 --out artifacts/training`.
4. Validate a produced simulation log, for example `python3 scripts/validate_interaction_log.py artifacts/simulation/interaction-events-<run-id>.jsonl`.
5. Inspect `artifacts/training/train_summary.json`, `validate_summary.json`, the step logs, and the new run manifest/log. Compare state coverage and contextual action rates; do not infer believability from synthetic reward alone.

## Remaining work before the study is considered research-ready

- Expand the initial rule-based preference extraction into a tested structured NLP result, including negation/ambiguity cases and a wider range of preferences.
- Log the exact memory record(s) actually used by action selection, not just candidate records.
- Add tests for queue FIFO/exactly-once processing, paired scenario invariance, deterministic evaluation traces, and log failure handling.
- Decide how persistent memory is isolated by participant/user identity in study mode. A single global memory file must not silently mix different participants' histories.
- Add held-out scenario coverage and more complete policy/checkpoint metadata to training/validation manifests.
- Run a real build, unit tests, validator tests and a short manual UI session before declaring the stage complete.
