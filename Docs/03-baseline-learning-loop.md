# Baseline Learning Loop Specification

## Status

Draft specification for the first minimal working baseline prototype.

This document translates the shared-state and action contracts into a concrete implementation plan for the smallest useful learning loop.

---

## 1. Purpose

This specification defines the baseline learning loop for the first working prototype of the companion agent.

The purpose is not to implement the full proposed architecture. The purpose is to establish a minimal, testable reinforcement-learning loop that can validate:

- state construction from observations
- action selection from a bounded action space
- reward processing
- Q-table updates
- terminal-state and unseen-state handling

The baseline loop is the foundation that the proposed memory-augmented agent will later build on.

---

## 2. Scope and constraints

### In scope

- bounded state encoding
- fixed action space
- epsilon-greedy action selection
- Q-table representation
- reward handling
- Q-learning update logic
- deterministic tests for the learning loop

### Out of scope

- memory integration
- affective-state integration
- full simulation rendering
- persistent user interaction modelling
- semantic NLP-driven memory updates

---

## 3. Baseline architecture

The baseline prototype will consist of the following minimal flow:

```text
Observation
    ↓
State encoding
    ↓
Action selection
    ↓
Environment response
    ↓
Reward + next state
    ↓
Q-table update
```

This keeps the implementation narrow and testable while matching the overall project architecture described in the hand-off document.

---

## 4. Minimal state representation

The baseline state will use only the bounded observation features defined in the shared-state specification.

### 4.1 Required observation fields

- interaction intent
- target availability
- target proximity
- user interaction status

These fields must be discretised into stable categories.

### 4.2 Encoding requirement

Each observation should be converted into a single finite state key before it is used in the Q-table.

The state key must:

- be deterministic
- be stable across runtime sessions
- remain compatible with Q-table persistence
- be derived from bounded discrete categories rather than raw continuous values

#### Example representation

```text
(intent, target_available, target_proximity, user_status)
```

This tuple may then be mapped to a mixed-radix integer or another stable identifier.

---

## 5. Minimal action space

The baseline prototype will use the fixed action set defined in the shared-state specification:

- `FETCH`
- `WANDER`
- `BARK`
- `ACT_CONFUSED`
- `SLEEP_LAY`
- `NUZZLE`
- `INSIST`

These actions must remain fixed during the initial learning-loop validation.

No additional action should be introduced unless the existing specification for the baseline loop is updated deliberately.

---

## 6. Action selection policy

The baseline agent will use a simple epsilon-greedy policy.

### 6.1 Policy definition

- With probability `epsilon`: choose a random valid action.
- Otherwise: choose the action with the highest Q-value for the current state.

### 6.2 Initial values

- `epsilon` should start at a value high enough to allow exploration.
- A simple default is a value between `0.1` and `0.3` for initial validation runs.
- The final value should be documented and remain consistent across tests.

### 6.3 Constraints

- The action selector must return only valid actions from the fixed action set.
- A state with no known values should still behave predictably.
- Random selection must be reproducible or at least controlled in tests.

---

## 7. Q-table definition

The baseline prototype requires a Q-table that maps state identifiers to action values.

### 7.1 Required behaviour

The Q-table must support:

- lookup by state and action
- insertion of new state-action pairs
- update of existing values
- saving and loading values if persistence is used in the prototype

### 7.2 Value representation

A conventional floating-point Q-value is sufficient for the first prototype.

### 7.3 Policy for unseen states

For unseen state-action pairs, the Q-table should use a documented default value, such as `0.0`, before learning updates occur.

This ensures the prototype can still select actions in previously unseen states without undefined behaviour.

---

## 8. Q-learning update rule

The baseline loop should use the standard Q-learning update:

```text
Q(s, a) = Q(s, a) + alpha * (reward + gamma * max(Q(s', a')) - Q(s, a))
```

Where:

- `alpha` is the learning rate
- `gamma` is the discount factor
- `s` is the current state
- `a` is the chosen action
- `s'` is the next state
- `reward` is the reward from the environment response

### 8.1 Initial parameter values

The first draft should use simple, documented values such as:

- `alpha = 0.1`
- `gamma = 0.9`
- `epsilon = 0.2`

These values are initial starting points and not final empirical settings.

### 8.2 Terminal states

The implementation must define how terminal transitions are handled.

If a terminal condition occurs:

- the next state should be treated as a valid terminal representation
- the update rule should still be applied consistently
- the terminal transition should be documented in tests

---

## 9. Reward integration

The baseline learning loop will use the initial reward model defined in the shared-state specification.

### 9.1 Required reward categories

The environment should emit reward values associated with:

- successful task completion
- explicit positive feedback
- explicit negative feedback
- repeated unsuccessful attempts
- neutral/no-response outcomes

### 9.2 Reward requirements

The reward must be:

- scalar
- deterministic for a given outcome
- applied to the action that produced the observed transition
- representable in tests without hidden side effects

### 9.3 Example reward mapping

```text
successful task completion -> +1.0
positive explicit feedback -> +1.0
small engagement increase -> +0.25
no response -> 0.0
repeated unsuccessful attempts -> -0.25
explicit negative feedback -> -2.0
```

These values are provisional and should be treated as initial test values.

---

## 10. Environment interaction model

The environment must supply, for each action and state, a next state and reward.

### 10.1 Required environment contract

For the initial prototype, the environment should provide:

- current observation
- selected action
- next observation
- reward
- terminal flag (if applicable)

### 10.2 Minimal environment behaviour

To keep the first implementation testable, the environment should be deterministic or semi-deterministic and easy to simulate.

The environment must not rely on the full memory or NLP subsystems at this stage.

---

## 11. Core algorithm flow

The baseline learning loop should proceed as follows:

1. Obtain current state from the environment.
2. Encode the current state to a stable state ID.
3. Select an action using epsilon-greedy exploration.
4. Execute the chosen action.
5. Receive the next observation, reward, and terminal flag.
6. Encode the next state.
7. Update the Q-table using the Q-learning rule.
8. Repeat until termination or the maximum step count is exceeded.

---

## 12. Required test cases

The baseline loop must be validated using deterministic examples before any memory or affective integration is attempted.

### 12.1 Test case 1: basic Q update

- known initial state
- chosen action with known reward
- next state known
- expected Q-value update is hand-calculable

### 12.2 Test case 2: unseen state handling

- state not previously seen
- action selection should not crash
- Q-value for missing state-action pair should be initialised predictably

### 12.3 Test case 3: epsilon-greedy behaviour

- verify that action selection returns a valid action from the action list
- confirm that exploration and exploitation follow the configured epsilon policy

### 12.4 Test case 4: terminal-state handling

- run one transition that ends the episode
- confirm that the Q-table update still occurs correctly and that terminal state handling is documented

### 12.5 Test case 5: persistence compatibility

- save Q-table values
- reload them
- confirm that the same state/action mappings resolve to the same values

---

## 13. Acceptance criteria

The baseline learning loop is considered sufficient for the next stage when all of the following are true:

1. states are discretised into a finite and documented encoding
2. the action space is fixed and valid
3. epsilon-greedy action selection returns valid actions
4. the Q-table updates using the standard Q-learning update rule
5. reward values are applied consistently
6. terminal transitions are handled explicitly
7. unseen states do not cause undefined behaviour
8. the loop is testable without memory or affective components

---

## 14. Implementation constraints for the next stage

- Keep the first implementation in a small, deterministic test harness.
- Do not introduce memory retrieval or affective features into the baseline loop.
- Do not expand the action set beyond the agreed seven actions.
- Do not modify the state contract without updating the associated documentation.
- Use the same state and action definitions for all baseline tests.

---

## 15. Next step

The next step is to convert this specification into a small implementation plan for the baseline prototype and to write the first deterministic tests before implementation begins.

This will preserve the project’s specification-driven workflow and keep the work aligned with the project hand-off.
