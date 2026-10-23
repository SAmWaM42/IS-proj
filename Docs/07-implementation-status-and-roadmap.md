# Implementation Status and Roadmap

**Last reviewed:** 2026-10-23  
**Branch reviewed:** `Dev`  
**Purpose:** Working status register for implemented features, outstanding work, refinements, tests, and their source/specification paths.

This is an implementation tracker, not evidence that every current source edit compiles or that the research design is validated. For architecture and design decisions, see [Docs/project-context.md](project-context.md). Feature-specific design contracts are linked below.

## Status key

- **Implemented — locally confirmed:** feature is in the source and has been confirmed working in the local application.
- **Implemented — tests recorded:** implementation and tests are documented, but the checked-in results may predate newer edits.
- **Implemented — verification required:** source changes exist, but a fresh build/test is still required.
- **Pending:** not yet completed or not yet demonstrated end-to-end.
- **Research gate:** software exists, but research validity, ethics, or participant-data collection is not established by implementation alone.

## 1. Current project snapshot

The project is a simulated companion-animal research prototype comparing a baseline tabular Q-learning agent with a proposed agent that incorporates semantic memory and affective state. The interactive SFML interface is a **working sprint-demo MVP**; it must not be described as proof that participants perceive the proposed agent as more believable.

### Confirmed at the latest review

- The owner confirmed the local SFML UI builds and launches for the sprint demonstration.
- `./build/IS-2 --sim` opens the participant-facing simulation mode.
- `./build/IS-2 --sim-researcher` opens researcher/debug mode.
- The UI includes A/B companion selection, a backyard scene, companion animation, preset interactions, typed interaction input, and session/interaction logging.
- Typed input is routed through the persistent Python NLP service when available, with keyword fallback.
- Session summaries and Q-table copies are written under `artifacts/simulation/`.
- The participant-facing mode hides internal action labels and memory/affect diagnostics; researcher mode exposes them.
- A questionnaire URL can be configured through `ANIMAL_AGENT_STUDY_FORM_URL`; the URL should not be hard-coded into source control.
- The current shared-action architecture and action-dependent environment-transition edits are committed on `Dev`, but those latest edits still require a fresh build and test run.

### Important boundaries

- NLP is bounded intent classification, not general dialogue generation.
- Preset rewards are development signals, not validated measures of user feedback or believability.
- Synthetic training/comparison scores are engineering diagnostics, not participant believability results.
- The `--study` workflow is a prototype. Do not collect real participant data until the supervisor/institutional ethics process, approved consent, and questionnaire/protocol requirements are satisfied.
- `Docs/test-results.json` records 30/30 PASS at **2026-10-22T08:06:21Z**. That report predates the latest Stage −1 edits and must not be treated as a fresh pass for the current revision.

## 2. Implemented features and source paths

### 2.1 Baseline tabular Q-learning

**Status:** Implemented; baseline tests are recorded.

Capabilities:
- Tabular Q-value lookup/update and handling of unseen states.
- Q-table persistence and terminal-state handling.
- Baseline remains the control condition and excludes memory/affect inputs.
- Exploration can be seeded for reproducible runs; evaluation mode disables exploration and policy updates.

Paths:
- Implementation: [Components/State-Action/state-action.h](../Components/State-Action/state-action.h), [Components/State-Action/state-action.cpp](../Components/State-Action/state-action.cpp)
- Design: [Docs/03-baseline-learning-loop.md](03-baseline-learning-loop.md)
- Tests/spec: [Docs/04-baseline-learning-tests.md](04-baseline-learning-tests.md)

### 2.2 Memory and affect state

**Status:** Implemented in the agent/state pipeline; end-to-end runtime behavior still merits focused verification.

Capabilities:
- Structured semantic memory records and affect-state contracts.
- Bounded `memory_bias` and `affect_trend` inputs for state construction.
- Memory records can be ranked and summarised before being represented as a bounded state signal.
- Proposed-agent memory and affect persistence/rest aggregation are implemented in the persistence layer.
- Memory can preserve explicit semantic preferences and prior interaction outcomes; negative feedback can influence subsequent proposed-agent behavior.

Paths:
- Implementation: [Components/Memory/memory.h](../Components/Memory/memory.h), [Components/Memory/memory.cpp](../Components/Memory/memory.cpp)
- State encoding: [Components/State-Action/state-action.h](../Components/State-Action/state-action.h), [Components/State-Action/state-action.cpp](../Components/State-Action/state-action.cpp)
- Specs: [Docs/05-memory-affect-scaffolding.md](05-memory-affect-scaffolding.md), [Docs/06-memory-affect-state-integration.md](06-memory-affect-state-integration.md), [Docs/08-live-memory-retrieval-and-ranking.md](08-live-memory-retrieval-and-ranking.md), [Docs/09-persistence-and-rest-aggregation.md](09-persistence-and-rest-aggregation.md)
- Runtime contracts to review: [Docs/10-runtime-state-lifecycle.md](10-runtime-state-lifecycle.md), [Docs/11-live-memory-activation-and-ranking.md](11-live-memory-activation-and-ranking.md), [Docs/12-runtime-affect-and-rest-state-orchestration.md](12-runtime-affect-and-rest-state-orchestration.md), [Docs/13-runtime-persistence-and-continuity-contract.md](13-runtime-persistence-and-continuity-contract.md)

### 2.3 Shared actions, observations, and environment transitions

**Status:** Implemented in source; **fresh build and regression verification required**.

Capabilities added in the latest Stage −1 work:
- One canonical 14-action vocabulary shared by baseline/proposed agents, scenarios, and UI action labels/animation grouping.
- Shared observable-context validity rules and invalid-action masking during action selection and next-state Q-value bootstrapping.
- Food/play affordances included in observations and policy state.
- Training and study scenarios include food, play, and negative-feedback contexts.
- Initial action-dependent environment transitions: approach changes proximity; fetch effort/success depend on distance and environment energy; rest/eating restore energy; far-fetch follow-up scenario added.
- Removed disconnected/unused legacy simulation and scaffolding identified in the Stage −1 specification.

Paths:
- Action contract: [Components/State-Action/action-space.h](../Components/State-Action/action-space.h)
- State and learning implementation: [Components/State-Action/state-action.h](../Components/State-Action/state-action.h), [Components/State-Action/state-action.cpp](../Components/State-Action/state-action.cpp)
- Tracking spec: [Docs/20-stage-minus-one-action-architecture.md](20-stage-minus-one-action-architecture.md)
- Environment/comparison contracts: [Docs/14-pre-simulation-environment-contract.md](14-pre-simulation-environment-contract.md), [Docs/15-proposed-agent-comparison-spec.md](15-proposed-agent-comparison-spec.md), [Docs/16-baseline-agent-comparison-spec.md](16-baseline-agent-comparison-spec.md)

**Fairness issue to resolve:** the latest implementation gives the proposed policy access to environment energy while the baseline does not. Decide whether energy is an explicitly intended treatment difference or make the observation available consistently to both agents. Document the decision and test it before interpreting comparison results.

### 2.4 Headless training, validation, and comparison

**Status:** Implemented; run again after the latest action/environment changes.

Commands:
```sh
./build/IS-2 --self-test
./build/IS-2 --train --episodes 1000 --steps 40 --seed 42 --out artifacts/training
./build/IS-2 --validate --episodes 200 --steps 40 --seed 43 --out artifacts/training
./build/IS-2 --compare --episodes 200 --steps 40 --seed 44 --out artifacts/training
```

The runner saves baseline/proposed Q-table checkpoints and mode-specific step logs/episode summaries. Retrain both policies after state/action encoding changes before validation or comparison. The current synthetic observations/reward setup is an engineering test harness; it is not evidence of long-horizon planning or perceived believability.

Paths:
- Entrypoint/options and self-tests: `main.c++`
- Build setup: `CMakeLists.txt`
- Comparison design: [Docs/15-proposed-agent-comparison-spec.md](15-proposed-agent-comparison-spec.md), [Docs/16-baseline-agent-comparison-spec.md](16-baseline-agent-comparison-spec.md), [Docs/16-paired-agent-scenario-tests.md](16-paired-agent-scenario-tests.md)
- Training/evaluation notes: [Docs/17-training-validation-implementation.md](17-training-validation-implementation.md)
- Historical test report: [Docs/test-results.json](test-results.json)

### 2.5 SFML interactive simulation MVP

**Status:** Implemented and locally confirmed by the project owner for sprint demonstration. Run regression checks after pulling the latest `Dev`.

Launch:
```sh
./build/IS-2 --sim
./build/IS-2 --sim-researcher
```

Features:
- Participant-facing mode and separate researcher/debug mode.
- A/B companion selection, backyard scene, companion sprite animation, preset interactions, scene-object hover descriptions, and typed text interaction.
- Persistent NLP service with spaCy/model support and labelled keyword fallback.
- Session/interaction logging and session Q-table copies under `artifacts/simulation/`.
- Optional questionnaire button driven by `ANIMAL_AGENT_STUDY_FORM_URL`.

Paths:
- UI: [Components/Simulation/companion-ui.h](../Components/Simulation/companion-ui.h), [Components/Simulation/companion-ui.cpp](../Components/Simulation/companion-ui.cpp)
- NLP adapter: [Components/NLP/nlp_intent_service.py](../Components/NLP/nlp_intent_service.py)
- Build: [CMakeLists.txt](../CMakeLists.txt)
- Usage/limitations: [README.md](../README.md), [Docs/18-sfml-companion-ui.md](18-sfml-companion-ui.md)

### 2.6 Persistence and generated artifacts

**Status:** Persistence features implemented; restart continuity and generated files should be rechecked after runtime changes.

The persistence subsystem covers Q-tables, memory records, affective state, session context, interaction history, and low-confidence aggregated facts. The interactive UI writes session summaries and Q-table copies to `artifacts/simulation/`; do not assume this is equivalent to restoring the complete episodic memory and affect state in the next UI launch.

Paths:
- Memory persistence: [Components/Memory/memory.h](../Components/Memory/memory.h), [Components/Memory/memory.cpp](../Components/Memory/memory.cpp)
- Persistence design: [Docs/09-persistence-and-rest-aggregation.md](09-persistence-and-rest-aggregation.md), [Docs/13-runtime-persistence-and-continuity-contract.md](13-runtime-persistence-and-continuity-contract.md)
- Example state files: `Docs/affect_state.json`, `Docs/aggregated_facts.json`, `Docs/interaction_history.json`, `Docs/memory_records.json`, `Docs/session_context.json`
- Runtime output: `artifacts/simulation/`, `artifacts/training/`, `artifacts/study/` (study output may contain restricted research data)

## 3. Pending work and refinements

Prioritised order reflects the next useful engineering gates, not a claim that the items are already implemented.

### P0 — Build and regression-check the latest `Dev` revision

- [ ] Pull latest `Dev` and configure/build with CMake.
- [ ] Run `./build/IS-2 --self-test`.
- [ ] Run the configured CTest suite, if present in the build.
- [ ] Run a short seeded training job and inspect its logs.
- [ ] Run a fresh NLP smoke test with the intended virtual environment.
- [ ] Re-open `--sim` and `--sim-researcher` and check the interaction/logging path.

### P1 — Validate Stage −1 environment and action changes

- [ ] Verify every canonical action is accepted/rejected in the intended contexts.
- [ ] Verify impossible actions are not selected and do not contribute to next-state Q-value bootstrapping.
- [ ] Verify approach/far-fetch changes distance, and fetch logs show successful and failed outcomes under the intended distance/energy conditions.
- [ ] Verify rest/eating restore energy and do not bypass task/action constraints.
- [ ] Verify food/play availability is encoded consistently in observations and state.
- [ ] Resolve and document baseline/proposed energy-observation fairness.
- [ ] Confirm terminal transitions do not bootstrap from next-state Q-values.

Tracking: [Docs/20-stage-minus-one-action-architecture.md](20-stage-minus-one-action-architecture.md).

### P1 — Audit memory retrieval and action-decision traces

- [ ] Check whether query relevance changes the final memory summary, not merely record ordering.
- [ ] Keep retrieval score, baseline record weight, query relevance, and summary weight conceptually distinct.
- [ ] Confirm the active runtime path uses the intended retrieval/persistence methods rather than relying on stubs or unused helpers.
- [ ] Log the reason/source for each final action: policy selection, contextual fallback, memory preference, rejection override, or invalid-action recovery.
- [ ] Measure fallback frequency so fallback behavior does not conceal poor policy performance.
- [ ] Test valence transitions and confirm the affect trend has the intended behavioral semantics.

Relevant paths: [Components/Memory/memory.cpp](../Components/Memory/memory.cpp), [Components/State-Action/state-action.cpp](../Components/State-Action/state-action.cpp), [Docs/08-live-memory-retrieval-and-ranking.md](08-live-memory-retrieval-and-ranking.md), [Docs/11-live-memory-activation-and-ranking.md](11-live-memory-activation-and-ranking.md).

### P2 — Runtime continuity and persistence

- [ ] Verify a full save/load round trip for Q-table, memory, affect, session context, interaction history, and aggregated facts.
- [ ] Verify a restarted runtime can retrieve prior semantic preferences and does not accidentally promote low-confidence candidates to confirmed facts.
- [ ] Separate UI session-summary persistence from complete agent-state restoration.
- [ ] Test malformed/missing persistence files and ensure safe defaults/recoverable errors.

Relevant specs: [Docs/09-persistence-and-rest-aggregation.md](09-persistence-and-rest-aggregation.md), [Docs/10-runtime-state-lifecycle.md](10-runtime-state-lifecycle.md), [Docs/13-runtime-persistence-and-continuity-contract.md](13-runtime-persistence-and-continuity-contract.md).

### P2 — NLP/UI robustness

- [ ] Confirm the NLP view reports spaCy/model processing when the configured environment has `spacy` and `en_core_web_sm`.
- [ ] Temporarily disable the model/service and confirm the fallback is labelled and still usable.
- [ ] Verify the service lifecycle, failure handling, and typed interaction latency.
- [ ] Verify sprite-sheet/frame assumptions and action-to-animation mapping against the actual bundled assets.
- [ ] Check all controls at the current window size and verify session files are written on normal close.
- [ ] Keep participant-facing UI free of internal action/memory/affect diagnostics.

Relevant paths: [Components/Simulation/companion-ui.cpp](../Components/Simulation/companion-ui.cpp), [Components/NLP/nlp_intent_service.py](../Components/NLP/nlp_intent_service.py), [Docs/18-sfml-companion-ui.md](18-sfml-companion-ui.md).

### P3 — Research protocol and believability evaluation

- [ ] Define the believability construct and the questionnaire items/scale before data collection.
- [ ] Pilot the interaction scenarios and verify both conditions receive matched scenarios and equivalent instructions.
- [ ] Ensure the only intended experimental differences are documented; avoid accidentally giving one agent extra environment information.
- [ ] Validate logs and analysis fields, including missing/no-response handling and condition-key protection.
- [ ] Obtain required supervisor/institutional ethics approval, approved consent, and questionnaire/protocol review before recruiting participants or collecting real data.
- [ ] Report perceived believability as an empirical outcome; do not infer it from synthetic reward or action counts.

Relevant paths: [Docs/08-research-validity-memory-affect-and-logging-spec.md](08-research-validity-memory-affect-and-logging-spec.md), [Docs/09-research-validity-implementation-progress.md](09-research-validity-implementation-progress.md), [Docs/16-paired-agent-scenario-tests.md](16-paired-agent-scenario-tests.md), [README.md](../README.md).

## 4. Test and verification register

| Check | Command / procedure | What it covers | Status at this review |
| --- | --- | --- | --- |
| Build | `cmake -S . -B build && cmake --build build --target IS-2` | Compile current sources and link the executable | **Required after latest Stage −1 edits** |
| Self-test | `./build/IS-2 --self-test` | Q-learning, state/action, memory/affect, persistence, runtime contracts | Historical report says PASS; rerun on current revision |
| CTest | `ctest --test-dir build --output-on-failure` | Registered CMake tests, if configured | Required to confirm current result |
| Training smoke test | `./build/IS-2 --train --episodes 10 --steps 20 --seed 42 --out artifacts/training-smoke` | Runtime startup, scenario loop, logs, action/environment transitions | Required |
| Full training | Command in §2.4 | Generate checkpoints for later evaluation | Rerun after state/action encoding changes |
| Validation | `./build/IS-2 --validate --episodes 200 --steps 40 --seed 43 --out artifacts/training` | Evaluate trained checkpoints without policy updates/exploration | Run only after retraining |
| Comparison | `./build/IS-2 --compare --episodes 200 --steps 40 --seed 44 --out artifacts/training` | Paired synthetic policy diagnostics | Run only after retraining; not a believability measure |
| NLP smoke test | Activate `.venv`, then submit text in `--sim` and inspect method label/log | spaCy/model path and fallback path | Required after current revision |
| Participant UI | `./build/IS-2 --sim` | Participant-facing interaction and logging | Locally confirmed by owner; regression check recommended |
| Researcher UI | `./build/IS-2 --sim-researcher` | Debug screens and comparison diagnostics | Regression check recommended |
| Study prototype | `./build/IS-2 --study --policy-dir artifacts/training --study-out artifacts/study` | Study flow and data writing | Software prototype only; ethics/research gate remains open |

Historical machine-readable report: [Docs/test-results.json](test-results.json), generated 2026-10-22. Do not edit its timestamp or treat it as current; regenerate it only through the project's test/report workflow after running the current build and tests.

## 5. Implementation map

| Area | Main source paths | Spec / status record |
| --- | --- | --- |
| Q-learning and state encoding | `Components/State-Action/state-action.h/.cpp` | [03](03-baseline-learning-loop.md), [04](04-baseline-learning-tests.md), [06](06-memory-affect-state-integration.md) |
| Canonical action set | `Components/State-Action/action-space.h` | [20](20-stage-minus-one-action-architecture.md) |
| Memory and affect | `Components/Memory/memory.h/.cpp` | [05](05-memory-affect-scaffolding.md), [08](08-live-memory-retrieval-and-ranking.md), [09](09-persistence-and-rest-aggregation.md) |
| Runtime contracts | state/action and memory components | [10](10-runtime-state-lifecycle.md)–[14](14-pre-simulation-environment-contract.md) |
| Proposed/baseline comparison | `main.c++` and shared state/action components | [15](15-proposed-agent-comparison-spec.md), [16](16-baseline-agent-comparison-spec.md), [16 paired tests](16-paired-agent-scenario-tests.md) |
| Training and evaluation | `main.c++`, `CMakeLists.txt` | [17](17-training-validation-implementation.md), [19](19-training-and-evaluation-redesign-spec.md) |
| SFML UI | `Components/Simulation/companion-ui.h/.cpp` | [18](18-sfml-companion-ui.md) |
| NLP | `Components/NLP/nlp_intent_service.py` | [README](../README.md), [18](18-sfml-companion-ui.md) |
| Research logging and validity | UI/study output and agent interaction logging | [08 research validity](08-research-validity-memory-affect-and-logging-spec.md), [09 progress](09-research-validity-implementation-progress.md) |
| Project-wide architecture/context | project architecture and implementation | [Docs/project-context.md](project-context.md) |

## 6. Working rules

1. Mark work as complete only when source paths are identified and the relevant check has passed on the current revision.
2. Keep historical test reports immutable; create a fresh report for a fresh test run.
3. Keep baseline exclusions explicit so it remains a genuine control.
4. Keep implementation behavior separate from research claims: a working UI is not a believability result.
5. Prefer small, independently testable changes and update this file when implementation status changes.
