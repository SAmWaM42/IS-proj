# Runtime State Lifecycle and Observation Normalisation

Status: Draft specification for approval

## Purpose

This stage defines the runtime orchestration layer that sits between live interaction data and the already-validated baseline state model. The goal is to create a single, bounded, testable path for turning a current interaction snapshot into a state representation that can optionally include memory and affective inputs without changing the baseline policy semantics.

This stage does not implement the actual simulation environment. It defines the pre-simulation runtime contract that the environment adapter will eventually consume.

## Authority and scope

This specification is subordinate to [Docs/project-context.md](Docs/project-context.md) and builds on the prior contracts in:

- [Docs/06-memory-affect-state-integration.md](Docs/06-memory-affect-state-integration.md)
- [Docs/08-live-memory-retrieval-and-ranking.md](Docs/08-live-memory-retrieval-and-ranking.md)
- [Docs/09-persistence-and-rest-aggregation.md](Docs/09-persistence-and-rest-aggregation.md)

## Goals

1. Define the runtime data flow from interaction input to state construction.
2. Capture a bounded set of temporary runtime values that are safe to summarise before state encoding.
3. Preserve the baseline state encoding when memory or affective inputs are absent or zero.
4. Keep the state builder deterministic, bounded, and easy to test.
5. Prepare the runtime contract for the next stages without committing to a full simulation implementation.

## Non-goals

- implementing the actual simulation environment
- changing the Q-learning update rule itself
- introducing unbounded memory summaries or free-form state dimensions
- wiring raw NLP output directly into policy logic

## Runtime inputs

The runtime loop should accept a compact set of structured inputs:

- interaction event: user-visible action, message, or observation tag
- last selected action
- current local environment snapshot
- current session context
- current affective summary
- latest ranked memory summary
- current rest-state or idle-state status

These inputs should be normalised before state construction, but they do not need to be the final simulation contract.

## Proposed runtime record types

### InteractionEvent

```cpp
struct InteractionEvent {
    std::string event_id;
    std::string timestamp;
    std::string channel;
    std::string user_signal;
    std::string environment_signal;
    float urgency = 0.0f;
    bool is_rest_state = false;
};
```

### RuntimeContext

```cpp
struct RuntimeContext {
    InteractionEvent latest_event;
    SessionContext session_context;
    AffectiveState affective_state;
    MemorySummary memory_summary;
    std::vector<std::string> recent_actions;
    bool rest_state = false;
};
```

### NormalisedObservation

```cpp
struct NormalisedObservation {
    int user_status = 0;
    int target_proximity = 0;
    int task_state = 0;
    int interaction_intent = 0;
    float memory_bias = 0.0f;
    float affect_trend = 0.0f;
};
```

## Runtime state lifecycle

The runtime lifecycle should follow this order:

1. Receive interaction or observation event.
2. Normalise event into categorical observation values.
3. Load current session context and affective state.
4. Retrieve and rank candidate memory records.
5. Condense ranked records into a bounded memory summary.
6. Derive a bounded affective trend from current affective state.
7. Build state using the existing baseline state encoding plus bounded memory and affective features.
8. Select an action using the baseline Q-policy.
9. Record action and outcome for persistence or future rest-state aggregation.

## State construction rules

The state-building path should remain the same as the current validated baseline unless the memory or affective feature set is present.

Required rules:

- if the memory summary is empty, memory_bias must remain at zero
- if affective state is neutral, affect_trend must remain at zero
- if runtime inputs are absent, the state key matches the legacy baseline state key
- all runtime inputs remain bounded within the existing mixed-radix design
- no direct policy branching is introduced in this stage

## Acceptance criteria

This stage is considered complete when all of the following are true:

- all runtime data is converted into bounded normalised values before state construction
- baseline state identity is unchanged when memory and affective signals are empty
- memory and affective inputs change state identity only when their bounded values are non-zero
- the state builder remains deterministic and repeatable across the same runtime input
- no Q-learning rules or reward logic are modified in this stage

## Open design note

The runtime state layer should be intentionally narrow. It exists to transform live interaction data into a stable state representation for later policy or simulation use. It is not yet a behavioural policy layer.

---

## Dependency and hand-off

The implementation of this stage should be followed by the runtime memory activation and affective orchestration stage, after which the project moves into the simulation boundary contract.
