# Baseline Agent Comparison Spec

**Document type:** Design spec for baseline agent comparison  
**Status:** Draft for review before training baseline comparison  
**Authority:** `Docs/project-context.md` remains the highest-level project reference. This document is a supporting implementation spec for the comparison stage.

---

## 1. Purpose

This specification defines the minimum implementation required for a baseline agent that does not use the current memory and affective-state pipeline. It exists to support a direct comparison against the proposed agent under the same configured simulation environment.

The baseline agent should be deliberately simpler, but it must still be a valid learning agent under the current architecture. It must be able to select actions from a Q-policy, update the Q-table from outcome data, and operate in the same evaluation harness as the proposed agent.

---

## 2. Research intent

The baseline model acts as the control condition for the project.

It should answer the question:

- How much does the proposed memory + affect model improve or change behaviour when compared with a simpler environmental-learning-only agent?

This is not a performance benchmark against a production-scale system. It is a controlled experiment to test whether the additional state representation changes the behaviour in systematic and observable ways.

---

## 3. Common comparison contract

The baseline agent must use the same environment contract as the proposed agent.

### 3.1 Shared observation

```cpp
struct EnvironmentObservation {
    int goal_state;
    int target_proximity;
    int user_status;
    int task_status;
    float signal_strength;
};
```

The baseline agent may process this directly and should not require additional memory or affect subsystem inputs to participate in a comparison run.

### 3.2 Shared action contract

The baseline agent must use the same action space as the proposed agent.

```cpp
static const std::vector<std::string> kDefaultActions = {
    "FETCH",
    "WANDER",
    "BARK",
    "ACT_CONFUSED",
    "SLEEP_LAY",
    "NUZZLE",
    "INSIST",
    "GREETING"
};
```

This ensures the same simulation environment can compare action choices consistently.

### 3.3 Shared reward and outcome contract

The environment should return the same reward semantics to both agents.

```cpp
struct ActionOutcome {
    int action_id;
    float reward;
    bool terminal;
    std::string outcome_tag;
};
```

This allows the baseline model to update Q-values without introducing hidden differences in reward design.

---

## 4. Baseline agent class design

### 4.1 Class name

`BaselineAgent`

### 4.2 Responsibilities

The baseline agent is responsible for:

- receiving observation data from the simulation,
- converting the observation into a minimal `state`,
- selecting actions by Q-learning,
- updating the Q-table after each action,
- tracking the session state needed for continuity,
- logging action and reward history for comparison output.

### 4.3 Core state members

```cpp
class BaselineAgent {
public:
    BaselineAgent(const EnvironmentConfig& config);

    void reset();
    std::string selectAction(const EnvironmentObservation& observation);
    void applyOutcome(const EnvironmentObservation& observation,
                     const std::string& action,
                     const ActionOutcome& outcome);

    void loadPersistentState(const std::string& path);
    void savePersistentState(const std::string& path) const;

private:
    EnvironmentConfig config;
    QLearning q_policy;
    SessionContext session_context;
    std::vector<InteractionEvent> interaction_history;
    state current_state;
};
```

### 4.4 Required logic

The baseline agent should:

1. convert the environment observation into a `state` using the existing state builder and environment distance/level mappings,
2. choose the action from `QLearning::chooseAction(...)`,
3. update the Q-table using the observed reward + next state,
4. persist a minimal Q-table and session summary if the run requires continuity,
5. maintain a lightweight event log but not memory or affective-state structures.

### 4.5 Explicit non-goals for the baseline

The baseline model must intentionally exclude:

- semantic memory records,
- memory retrieval / ranking,
- affective-state packages,
- aggregated rest-state fact generation,
- memory-derived state biasing,
- user-specific memory persistence as a decision factor.

The baseline should remain a learning system that depends primarily on environmental state, not internal semantic augmentation.

### 4.6 State construction for the baseline

The baseline agent should use the existing state representation, but must omit the memory and affect fields from the state-building process.

Example pattern:

```cpp
state s = makeState(
    session_context.current_intent,
    proximityFromValue(session_context.target_proximity),
    levelFromValue(session_context.energy_level),
    levelFromValue(session_context.stress_level),
    Level::MEDIUM,
    Level::LOW,
    session_context.user_status,
    0.0f
);
```

This keeps the state compact and policy-driven, while avoiding the higher-order memory/affect additions that exist in the proposed model.

### 4.7 Acceptance criteria

A valid baseline implementation should satisfy the following:

- It can run in the same configured environment as the proposed agent.
- It uses the same action set and reward semantics.
- It produces a policy-based action from `QLearning`.
- It updates its Q-table from observed reward outcomes.
- It does not require or use memory records or affect signals.
- It can be trained and saved independently to establish its own baseline Q-table.

---

## 5. Baseline comparison harness

The comparison harness should instantiate both agents with the same configuration and run episodes in parallel or sequentially under the same conditions.

```cpp
struct AgentComparisonRun {
    EnvironmentConfig config;
    ProposedAgent proposed_agent;
    BaselineAgent baseline_agent;
    std::vector<SimulationTick> tick_log;
};
```

The harness should capture per-episode metrics like:

- reward totals,
- chosen actions,
- terminal outcome,
- state encodings,
- final Q-table snapshot,
- resulting behaviour distribution.

The objective is to compare the policy decisions and learning dynamics, not to claim that one model is generally superior without the same environment and training conditions.

---

## 6. Required training sequence

Before training the implemented infrastructure, do the following:

1. Create the baseline agent class and configuration contract.
2. Create the proposed agent class and configuration contract.
3. Stand up a common environment runner for both agents.
4. Verify both agents can accept identical observations.
5. Verify both agents can select valid actions from the same action list.
6. Verify both agents can update their Q-table from the same reward outcomes.
7. Save the baseline Q-table and proposed Q-table separately for comparison.

This creates a true before-training baseline rather than a difficult-to-compare ad hoc implementation.

---

## 7. Implementation notes

- The baseline agent should be intentionally minimal and explainable.
- The baseline should not inherit the proposed agent or mix in the memory/affect mechanisms accidentally.
- The shared environment must remain the same; otherwise comparison results are not meaningful.
- Logging should capture both the action and the state encoding so differences can be interpreted after training.

---

## 8. Out-of-scope for this spec

- User-facing believability scoring.
- Semantic memory inference beyond the already-approved bounded project scope.
- A broad training regime beyond the initial baseline Q-table generation.
- Any changes to the environment that make the baseline and proposed agent non-comparable.

---

## 9. Review checkpoint

This specification should be reviewed before any training starts. It defines the cleanest path to a fair experimental comparison: two agent classes, one shared environment, and one minimal but reproducible evaluation harness.
