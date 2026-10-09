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

Development status: Baseline

The project is currently in a validated baseline stage.

Verified milestones:

- A minimal C++ baseline learning loop has been implemented and validated.
- The shared state/action contracts are formalised and documented.
- The C++/Python tokenizer boundary has been tested successfully.
- Minimal memory and affect scaffolding has been added as an explicit staged extension point.

The current implementation does not yet combine memory and affect into the Q-learning policy. That integration remains deliberately deferred until the baseline remains stable and reproducible.

---

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

## Planned next stage

The next step is to integrate memory and affective state into the state-construction pipeline, while preserving the already-validated baseline learning loop and the shared action/state contracts.

The expected sequence is:

1. Extend the state builder with bounded memory-derived features.
2. Add affective-state inputs to the state representation.
3. Validate the updated state representation separately.
4. Re-run baseline checks and compare results against the earlier baseline.
5. Only then evaluate behavioural outcomes under the proposed agent configuration.

---

## Summary

This project is best understood as a staged research prototype for a learning-based companion agent. The current implementation is intentionally conservative: the baseline learning loop is validated first, the memory and affect contracts are defined next, and only then are those features integrated into the decision-making pipeline.

This staged approach keeps the project explainable, testable, and aligned with the project’s research objectives.
