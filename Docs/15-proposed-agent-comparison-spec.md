# Proposed Agent Comparison Spec

**Document type:** Design spec for training comparison setup  
**Status:** Draft for review before training baseline comparison  
**Authority:** `Docs/project-context.md` remains the highest-level project reference. This document is a supporting implementation spec for the research comparison stage.

---

## 1. Purpose

This specification defines the minimum class design needed to create a proposed agent that includes memory and affective-state support, so it can be run side-by-side with a baseline agent in the same configured simulation environment.

The goal is not to implement the full final behaviour yet. The goal is to define a class and contract that can:

- observe the same environment as the baseline agent,
- produce a valid action from the agent policy,
- update internal state after each step,
- persist and reload training-relevant state,
- allow fair comparison against a baseline Q-learning agent.

---

## 2. Research intent

The proposed agent is the implementation under study: it should include the memory and affect mechanisms already established in the project architecture.

It must differ from the baseline in the state representation used by the policy, not in the simulation harness itself. Both agents should be trained and evaluated under the same:

- environment configuration,
- action space,
- reward framing,
- reset conditions,
- training length,
- evaluation conditions.

---

## 3. Shared comparison contract

Before either agent is trained, both must implement the same common interface for environmental interaction.

### 3.1 Shared environment config

The environment should expose a common runtime configuration object, for example:

```cpp
struct EnvironmentConfig {
    int width;
    int height;
    int max_steps;
    int seed;
    std::vector<std::string> actions;
    float reward_positive;
    float reward_negative;
    float reward_neutral;
    bool allow_user_stimulus;
};
```

This object should be used by both agents and should be identical for both training runs.

### 3.2 Shared observation contract

Each agent receives an observation that is deliberately compatible with both implementations.

```cpp
struct EnvironmentObservation {
    int goal_state;
    int target_proximity;
    int user_status;
    int task_status;
    float signal_strength;
};
```

This should be the common external observation source. The agent may enrich it internally, but it must not require a different environment contract.

### 3.3 Shared outcome contract

After selecting an action, the environment should return a standard outcome.

```cpp
struct ActionOutcome {
    int action_id;
    float reward;
    bool terminal;
    std::string outcome_tag;
};
```

This allows both agent classes to update their training state using the same reward semantics.

---

## 4. Proposed agent class design

### 4.1 Class name

`ProposedAgent`

### 4.2 Responsibilities

The proposed agent is responsible for:

- maintaining the Q-table,
- maintaining persistent memory records,
- maintaining affective state,
- building the runtime state from observation + memory + affect,
- selecting an action from the learned Q-policy,
- updating memory and affect after an outcome,
- persisting and reloading its runtime state and memory store.

### 4.3 Core state members

```cpp
class ProposedAgent {
public:
    ProposedAgent(const EnvironmentConfig& config);

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
    AffectiveState affective_state;
    std::vector<record> memory_records;
    std::vector<InteractionEvent> interaction_history;
    RuntimeContext runtime_context;
    state current_state;
};
```

### 4.4 Internal logic

The proposed agent must parse the same environment observation, then:

1. build a runtime context from the latest event,
2. retrieve to-ranked relevant memory records,
3. summarise the memory signal,
4. combine memory, affect, and environment values into a state,
5. choose an action using its Q-learning policy,
6. apply the reward and update affect + memory,
7. persist the Q-table and memory state if required by the run configuration.

### 4.5 Required subcomponents

The proposed agent should use the existing project abstractions as much as possible:

- `QLearning` for action selection and value updates,
- `state` and `makeRuntimeState(...)` for the final encoded state,
- `MemorySummary` and memory scoring logic for retrieval and summarisation,
- `AffectiveState` and `summarizeAffectTrend(...)` for internal mood/energy tracking,
- `SessionContext` for continuity across steps,
- JSON persistence helpers already used in the project for memory, affect, interaction history, and aggregated facts.

### 4.6 Required behavioural differences from the baseline

The proposed agent should include at least these additional elements that are intentionally absent in the baseline:

- memory retrieval and ranking,
- weighted memory summary contribution,
- affective-state updates on rewards and event polarity,
- memory-based state shifts,
- persistence of interaction and memory state across runs,
- rest-state aggregation of low-confidence facts.

### 4.7 Acceptance criteria

A valid proposed-agent implementation should satisfy the following:

- It can run in the same configured environment as the baseline agent.
- It produces an action from `QLearning` and not from a hardcoded heuristic.
- It incorporates memory and affect into the state-building path.
- It records user interactions and session context in a persistent form.
- It can be compared against the baseline by logging reward, chosen action, and final state for equal episodes.

---

## 5. Proposed comparison harness

The training harness should instantiate both agents against the same shared configuration object and environment instance.

```cpp
struct AgentComparisonRun {
    EnvironmentConfig config;
    ProposedAgent proposed_agent;
    BaselineAgent baseline_agent;
    std::vector<SimulationTick> tick_log;
};
```

The harness should:

- reset both agents,
- run the same number of steps,
- apply the same environment observations to both agents,
- record the same reward stream or equivalent comparable metrics,
- save per-agent logs for later analysis.

This ensures the comparison is about the agent design, not about environment drift.

---

## 6. Implementation notes

- Do not implement the full memory and affect architecture in the first run if it would delay baseline comparison. Use the existing bounded state design already in the project.
- Keep the proposed agent and baseline agent API as close as possible to ensure fairness.
- Log both action choice and state fingerprints so that the difference between the models is measurable and visible.
- Keep the reward function equivalent for both models so the comparison isolates the agent internals.

---

## 7. Out-of-scope for this spec

- Full multi-agent interaction.
- Non-tabular deep RL.
- Believability scoring or subjective user evaluation.
- Broad memory retrieval optimisation beyond the bounded design already approved.

---

## 8. Review checkpoint

This spec is intended to be approved before the baseline training sequence begins. It should define the minimum class and harness contract for the training comparison, without yet expanding into a broader full-agent architecture.
