# Pre-Simulation Environment Contract

Status: Draft specification for approval

## Purpose

This stage defines the contract that must exist before the project begins implementing the actual simulation environment. The goal is to formalise the boundary between the agent runtime and the eventual environment loop so that the simulation can be implemented without changing the core state or memory contract.

This is the final pre-simulation specification stage. It does not implement the environment itself.

## Authority and scope

This specification should be read as the hand-off document immediately before the environmental runtime implementation. It is subordinate to:

- [Docs/project-context.md](Docs/project-context.md)
- [Docs/03-baseline-learning-loop.md](Docs/03-baseline-learning-loop.md)
- [Docs/10-runtime-state-lifecycle.md](Docs/10-runtime-state-lifecycle.md)
- [Docs/11-live-memory-activation-and-ranking.md](Docs/11-live-memory-activation-and-ranking.md)
- [Docs/12-runtime-affect-and-rest-state-orchestration.md](Docs/12-runtime-affect-and-rest-state-orchestration.md)
- [Docs/13-runtime-persistence-and-continuity-contract.md](Docs/13-runtime-persistence-and-continuity-contract.md)

## Required environment boundary

The eventual simulation environment must provide a stable contract with the following responsibilities:

- produce current environmental observations
- accept the chosen action from the agent
- return reward or penalty information
- track terminal conditions or reset conditions
- expose a time-step or turn abstraction

## Proposed types

### EnvironmentObservation

```cpp
struct EnvironmentObservation {
    int goal_state = 0;
    int target_proximity = 0;
    int user_status = 0;
    int task_status = 0;
    float signal_strength = 0.0f;
};
```

### ActionResult

```cpp
struct ActionResult {
    int action_id = 0;
    float reward = 0.0f;
    bool terminal = false;
    std::string outcome_tag;
};
```

### SimulationTick

```cpp
struct SimulationTick {
    EnvironmentObservation observation;
    ActionResult action_result;
    std::string timestamp;
};
```

## Environment contract rules

1. The environment must not directly modify the Q-table.
2. The environment must return observation data in the same bounded categories used by the agent state contract.
3. The agent should remain the sole component responsible for learning updates.
4. Reward and terminal logic must be structured and testable.
5. The environment interface should be small enough to support deterministic tests without requiring a full visual or game runtime.

## Pre-implementation requirements

Before actual simulation code is written, the project should satisfy the following:

- the runtime state builder is approved and stable
- memory retrieval and affective summary generation are approved and deterministic
- persistence is approved and continuity-safe
- the environmental observation categories are consistent with the state schema

## Acceptance criteria

This stage is complete when:

- the environment boundary is clearly separated from the learning logic
- action execution and observation generation are defined in small, bounded structures
- reward and terminal conditions are explicitly modelled but not yet implemented as a larger simulation engine
- the next implementation step is clearly limited to simulation runtime development rather than redesigning the state or memory model

## Final gate before simulation implementation

Once these documents are approved, the project is ready to move from runtime contract work into the actual simulation environment implementation stage. The design should still remain bounded and testable rather than broad or feature-heavy.
