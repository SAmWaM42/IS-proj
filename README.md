# Companion Agent Project

## Overview

This project investigates a companion agent that learns from repeated interaction and can later incorporate persistent memory and affective state into its decision-making.

The current work is intentionally staged:

1. Establish a minimal, validated baseline learning loop.
2. Formalise the shared state and action contracts.
3. Add minimal memory and affect scaffolding.
4. Integrate these components only after the baseline is stable and testable.

The project is not intended to be a general-purpose chatbot or a large-scale LLM application. It is a bounded research prototype focused on agent behaviour, learning, and perceived believability under controlled conditions.

---

## Project scope

The system is designed around the comparison between two experimental agents:

- Baseline agent: learns from environmental observations and a fixed action space without advanced memory or affect integration.
- Proposed agent: augments the baseline with semantic memory, affective-state variables, and memory-derived features.

Both agents share the same environment and action definitions during the first experiment so that behavioural differences can be attributed to the memory/affect augmentation rather than to different underlying mechanics.

---

## Current status

Development status: Baseline + headless synthetic experiment CLI + participant study prototype

The project is currently in a stable staged prototype phase. The original baseline learning loop remains validated, and the implementation has progressed beyond the purely tabular baseline to include bounded memory/affect integration, persistence, and a minimal SFML-backed runtime environment.

Verified milestones:

- A minimal C++ baseline Q-learning loop has been implemented and validated.
- Shared state/action contracts are formalised and documented.
- The 14-action vocabulary, observable-context action mask, food/play affordances, and shared animation mapping are documented in [Docs/20-stage-minus-one-action-architecture.md](Docs/20-stage-minus-one-action-architecture.md).
- Memory and affect scaffolding was added as an explicit staged extension point.
- Bounded memory/affect state integration has been implemented without destabilising the baseline policy.
- Live memory retrieval, ranking, and state summarisation are in place.
- Persistence and rest-state aggregation are implemented for Q-table, memory records, affective state, session context, interaction history, and low-confidence aggregated facts.
- The headless `--compare` workflow compares the trained baseline and proposed policies using shared scenario inputs.
- A headless synthetic training/evaluation runner is available through `--train`, `--validate`, and `--compare`; it writes mode-specific per-step JSONL logs and episode summaries.
- A keyboard-driven participant-study prototype is available through `--study`, with randomized A/B condition order, fixed shared scenarios, participant feedback, and a separate questionnaire.
- Terminal transitions no longer bootstrap from next-state Q-values, and policy exploration uses a per-agent seed derived from the experiment configuration.
- A machine-readable validation report is stored in [Docs/test-results.json](Docs/test-results.json).

The current implementation deliberately keeps the policy layer stable and explainable while the runtime, memory, and affect pipeline is exercised under controlled conditions. This keeps the project testable while still enabling comparison and simulation-level observation.

---

## Headless experiment commands

Build the project with the existing CMake setup, then run:

```sh
./build/IS-2 --self-test
./build/IS-2 --train --episodes 1000 --steps 40 --seed 42 --out artifacts/training
./build/IS-2 --validate --episodes 200 --steps 40 --seed 43 --out artifacts/training
./build/IS-2 --compare --episodes 200 --steps 40 --seed 44 --out artifacts/training
```

Training saves `baseline.qtable` and `proposed.qtable`. Validation and comparison load those checkpoints, disable exploration and policy updates, and use an evaluation scenario seed offset from the supplied seed. Step logs are written to `<mode>_baseline_steps.jsonl` and `<mode>_proposed_steps.jsonl`, preventing later runs from overwriting earlier mode logs; a mode-specific JSON summary is also written.

The policy state includes task availability, discretized signal strength, and explicit food/play affordances. Both agents use the same 14-action vocabulary and observable-context mask; invalid actions are excluded from selection and Q-value bootstrapping. The synthetic reward is also intent-conditioned (social greeting, explicit request, or disturbance/rest).

Important: these state-encoding changes make previously saved Q-table keys incompatible with the current policy. **Retrain both agents before validation or running the participant study**. The current synthetic observations are independently sampled rather than a causal action-driven environment, so each training update deliberately avoids bootstrapping from a randomly sampled next observation. Treat this as contextual reward learning, not evidence of long-horizon planning.

These are synthetic engineering diagnostics only. The reward model does not measure believability. Build and test the branch before relying on generated results.

### Interactive SFML simulation UI

Launch the replacement three-screen simulation interface:

```sh
./build/IS-2 --sim
```

The UI follows the Chapter 3.5.3 wireframes: **Main Simulation** provides the environment, preset interaction controls and text input; **Agent State** exposes proposed-agent affect and recent stored memory records; **Comparison** presents both agents' responses to the same interaction stream.

The text input is routed through the persistent spaCy NLP service when available, with a C++ keyword fallback. It recognises fetch/request, praise, disturbance, rest, negative feedback, feeding, and play contexts. Preset controls use fixed development reward signals, so this UI is for simulation and debugging, not a substitute for the controlled participant-study protocol. The interface saves a session summary and session Q-table copies under `artifacts/simulation/`.

### NLP-enabled simulation and dog sprites

The SFML `--sim` interface sends typed messages to `Components/NLP/nlp_intent_service.py`. The adapter uses spaCy's `en_core_web_sm` lemmatization when installed and reports the selected method in the simulation view; a keyword fallback is used if Python, spaCy, or the model is unavailable.

Set up the optional NLP dependency in a virtual environment:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install spacy
python -m spacy download en_core_web_sm
```

The UI currently launches `python3` by name, so ensure the desired Python environment is active before starting the application. The renderer maps both agents' selected actions to the same available animation groups. The included dog sprite sheet is loaded from `Data/Assets/DogMegaPackFree/DogMegaPackFree/Dogs.png`; verify the source sheet's exact frame layout visually and adjust the 4-by-4 frame mapping if its cells do not correspond to animation frames. If the image cannot be loaded, it falls back to the vector placeholder. The larger window and Hide Controls button provide more room for the yard view.

This is an initial sprite-sheet integration: verify the source sheet's exact frame layout visually and adjust the 4-by-4 frame mapping if its cells do not correspond to animation frames. The NLP intent mapper still emits a small set of discrete intents and fixed development reward signals; it is not a general dialogue model or a validated participant-feedback mechanism.

### Participant study prototype

After training the policies, launch the SFML study interface:

```sh
./build/IS-2 --study --policy-dir artifacts/training --study-out artifacts/study
```

The interface presents two blinded conditions (A/B) in random order, uses the same five scenarios for both, records positive/neutral/negative/no-response feedback, and asks four 1–5 ratings after each condition. Policy learning is disabled during participant sessions; feedback can update the proposed agent's affect and memory state, while the questionnaire ratings remain separate from the agent's reward signal.

Participant data is written as an anonymous session JSON file under `artifacts/study/`, which is git-ignored. The file includes the condition key for later analysis, so treat it as restricted research data. This prototype does not replace supervisor/institutional ethics approval, an approved consent process, or a piloted questionnaire. Do not collect real participant data until those requirements are met.

## Architecture

The project separates responsibilities into bounded components:

```text
User / Interaction
      |
      v
NLP Client (Python + spaCy)
      |
      v
Memory Parser / Memory Layer
      |
      v
Affective State
      |
      v
State Builder
      |
      v
Q-learning Policy
      |
      v
Simulation Environment / Reward Loop
```

The main architectural rule is that the learning loop is kept separate from NLP, memory, and affect details until those interfaces are formally validated.

---

## Core design principles

- Prefer working, testable increments over architectural complexity.
- Keep the Q-table state bounded and deterministic.
- Use discrete action and observation categories during the first prototype.
- Treat memory and affect as explicit, structured inputs rather than hidden system behaviour.
- Validate each stage before moving to the next.

---

## Component overview

### NLP boundary

The project uses a Python spaCy-based tokenizer process communicating with the C++ application through a lightweight stdin/stdout interface. This boundary was validated with a minimal C++ harness and is intended to supply structured language-processing results without making the decision layer directly dependent on Python internals.

### Environment and state

The environment exposes small, bounded observations such as object proximity, interaction status, and relevant task state. These are converted into discrete categories before entering the Q-table.

### Learning loop

The baseline uses tabular Q-learning with a fixed action space and a compact state representation. The Q-table updates based on observed rewards and the best value available in the next state.

### Memory subsystem

The memory layer stores structured facts, confidence, generalisation, source, scope, and relevance metadata. This allows a future retrieval process to select records that are helpful in the current context without overloading the decision system with raw historical data.

### Affective state

The affective layer tracks bounded variables such as valence, energy, arousal, engagement, stress, and affinity. These are designed to provide structured inputs for future state construction without yet directly changing the baseline policy.

---

## Minimal memory model

The project currently defines a minimal memory contract intended for future retrieval and state construction.

### `fact`

```cpp
typedef struct {
    std::string subject;
    std::string relation;
    std::string object;
} fact;
```

A `fact` captures the underlying semantic relationship, for example:

```text
user -> likes -> fetch
agent -> observed -> user_playing_fetch
```

### `record`

```cpp
struct record {
    fact raw_fact;
    float confidence = 0.0f;
    float generalization_factor = 0.0f;
    enum user_context { GENERAL, USER_SPECIFIC };
    user_context current_context = GENERAL;
    std::vector<std::string> relevant_users;
    enum source_type { USER_STATEMENT, OBSERVATION, ACTION_HISTORY, INFERENCE };
    source_type source = OBSERVATION;
    int observation_count = 0;
    std::string last_updated;
    float relevance = 0.0f;
};
```

A `record` augments the raw fact with metadata describing how reliable, general, and context-specific it is.

### `action_log`

```cpp
class action_log {
public:
    std::string action_taken;
    std::string time;
    float count = 0.0f;
};
```

Action logs provide a historical record of what the agent has done, which can later be used to update memory or influence behavioural patterns.

### `cache`

```cpp
class cache {
public:
    std::vector<ex_record> data_store;
};
```

The cache is a lightweight optimization layer for reusing already extracted memory information.

### `AffectiveState`

```cpp
struct AffectiveState {
    AffectValence valence = AffectValence::NEUTRAL;
    AffectEnergy energy = AffectEnergy::MODERATE;
    float arousal = 0.5f;
    float engagement = 0.5f;
    float stress = 0.5f;
    float affinity = 0.5f;
};
```

These values are intentionally small and bounded so they can be tested cleanly before being integrated into the state representation.

---

## Build and validation

The project uses CMake as the build entry point.

```bash
cmake -S . -B build
cmake --build build --target IS-2
./build/IS-2
```

The tokenizer boundary is also validated with a dedicated C++ harness:

```bash
cmake --build build --target tokenizer_probe
./build/tokenizer_probe
```

Current validated baseline checks include:

- basic Q update
- unseen state handling
- persistence
- terminal handling

The current baseline validation passes successfully.

---

## Current development focus

The repository has moved past the initial baseline-only stage and is now focused on the comparison and runtime validation layer.

The immediate work is:

1. Continue validating the shared environment contract for both agents.
2. Expand the baseline/proposed comparison harness into a more formal training and evaluation pass.
3. Keep the simulation environment deliberately minimal but behaviourally meaningful.
4. Preserve the baseline as the control condition while testing the memory/affect-aware variant under the same stimuli and reward semantics.
5. Maintain persistence continuity across runtime sessions and rest-state aggregation without broadening the Q-learning policy.

---

## Summary

This project is best understood as a staged research prototype for a learning-based companion agent. The current implementation is intentionally conservative: the baseline learning loop is validated first, the memory and affect contracts are defined next, and only then are those features integrated into the decision-making pipeline.

This staged approach keeps the project explainable, testable, and aligned with the project’s research objectives.
