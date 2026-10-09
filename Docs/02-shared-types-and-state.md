# Shared Types and State Specification

## Status

Draft specification for the initial shared contracts used by the companion-agent prototype.

This document is intended to complement the project hand-off in `project-context.md` and to define the minimum viable contracts needed before implementation. It does not replace the project hand-off; it narrows the design to the first executable prototype.

---

## 1. Purpose

This specification defines the minimal shared contracts required for the first working prototype of the companion agent.

The goal is to establish a bounded, testable, and comparable state and action model that can be used by both the baseline and proposed agent implementations without introducing unbounded complexity or hidden assumptions.

This document covers the following elements:

- environmental observation features
- minimal discrete action space
- stable state encoding strategy
- reward model for the initial prototype
- staged integration strategy for memory and affective state

---

## 2. Scope and non-goals

### In scope

- A bounded environmental observation model
- A discrete, finite action space
- A stable Q-learning state key
- A minimal reward system
- Clear separation between baseline and proposed-agent state construction

### Out of scope

- Full semantic memory implementation
- Full affective-state learning rules
- Complex simulation rendering or world-state management
- Large-scale experiment design
- User-believability metric design

---

## 3. Shared design principles

1. The first prototype must be bounded and reproducible.
2. The state space must be finite and deterministic.
3. The action space must be fixed for the first experiment.
4. Reward signals must be explicit and testable.
5. Memory and affective features must be introduced only after the base learning loop works.
6. The baseline and proposed agents must share the same core action and environment definitions unless a difference is explicitly justified and documented.

---

## 4. Environmental observation model

The environment observation will contain a bounded set of discrete features. The environment should filter inputs according to relevance to the current interaction instead of exposing every possible world variable.

### 4.1 Required initial features

The first implementation includes the following features:

- Interaction intent
  - The inferred purpose of the user's current input.
  - Examples: `FETCH`, `ATTENTION`, `GENERAL_INTERACTION`, `INVALID`

- Target availability
  - Whether the object or target relevant to the current intent exists in the current environment.
  - Values: `AVAILABLE`, `UNAVAILABLE`

- Target proximity
  - Distance between the agent and the relevant object.
  - Categories: `NEAR`, `MEDIUM`, `FAR`, `UNAVAILABLE`

- User presence / interaction status
  - Whether the user is available and whether a recent interaction has occurred.
  - Categories should be fixed and documented.

### 4.2 Relevance filtering

Environmental variables should be filtered according to the current interaction.

Example:

- If the current intent is `FETCH`, the feature relevant to the state is the distance to the target object.
- Distance to unrelated objects should not be included in the same state representation.

This prevents the state from expanding unnecessarily and keeps the Q-table bounded.

### 4.3 Constraints

- Raw continuous coordinates must not be used directly in the Q-table key.
- Fixed thresholds must be documented and used consistently.
- The state representation must be reproducible across runs.
- Relevance must be determined by the current interaction intent, not by arbitrary ad hoc rules.

---

## 5. Minimal action space

The initial action space will contain seven actions.

| Action name | Intended behaviour |
|---|---|
| `FETCH` | Attempt to retrieve the relevant object. |
| `WANDER` | Move around the environment without a specific target. |
| `BARK` | Vocalise to initiate or respond to an interaction. |
| `ACT_CONFUSED` | Express confusion when the input cannot be interpreted or acted upon. |
| `SLEEP_LAY` | Lie down or perform a resting behaviour. |
| `NUZZLE` | Seek attention through close interaction. |
| `INSIST` | Persist in an interaction or repeat an attempt when the intended response has not occurred. |

### 5.1 Behavioural notes

- `INSIST` is not a generic “repeat previous action” rule.
- It represents a distinct, explicit behavioural choice.
- Its usefulness depends on the context, previous outcome, and current interaction state.
- It should be learned through reward consequences rather than hard-coded as a universal response.

### 5.2 Experimental comparability

Both the baseline and proposed agents should use the same action definitions in the first experiment.

This ensures the comparison is not confounded by mismatched action spaces.

The proposed agent may learn to choose `INSIST` differently because its state includes memory and affective features, but the action definitions themselves remain common.

---

## 6. State encoding strategy

The state must be reduced to a stable finite key before it is used in the Q-table.

### 6.1 Encoding model

Each observation feature will be discretised into a bounded, documented category set.

The initial feature set may include:

- interaction intent: 4 categories
- target proximity: 4 categories
- target availability: 2 categories
- user interaction status: 3 categories

This gives a rough upper bound of:

4 x 4 x 2 x 3 = 96 possible state combinations

This is small enough to be manageable for the first working prototype.

### 6.2 Preferred encoding method

A mixed-radix or tuple-based encoding is preferred over ad hoc weighting.

A simple tuple representation is recommended initially:

```text
(state_intent, state_target_proximity, state_target_available, state_user_status)
```

This tuple can then be converted into a stable integer state ID using a documented mapping, such as a mixed-radix encoding.

### 6.3 Constraints

- The same encoding must be used for both baseline and proposed agents during comparable trials.
- The encoding must remain fixed across runs unless the specification is deliberately updated.
- A change to the feature set or encoding rule must be treated as a new state definition and should be documented as such.
- Unseen states must have a documented, stable handling rule.

### 6.4 Notes on state growth

Memory-derived and affective features should not be added to the initial Q-table state until the basic environmental model is working.

This keeps the state space small and prevents memory or affective noise from obscuring the baseline learning loop.

---

## 7. Reward specification

The reward model should be explicit, bounded, and testable. It must distinguish successful outcomes from positive engagement and from explicit negative feedback.

### 7.1 Reward principles

1. Successful task completion receives a positive reward.
2. Positive user feedback receives a positive reward.
3. Measurable engagement increase receives a smaller positive reward.
4. Silence or no response is treated as neutral unless defined otherwise.
5. Explicit negative feedback receives a strong negative reward.
6. Repeated unsuccessful attempts receive a small negative penalty.
7. Reward signals must be cumulative and derived from observable outcomes.

### 7.2 Provisional reward table

| Outcome | Illustrative reward |
|---|---:|
| Successful task completion | +1.0 |
| Positive explicit user feedback | +1.0 |
| Small measurable engagement increase | +0.25 |
| No response | 0.0 |
| Repeated unsuccessful attempts | -0.25 |
| Explicit negative user feedback | -2.0 |

These values are provisional and should be treated as initial design values rather than final empirical tuning values.

### 7.3 Engagement term

A simple engagement measure may be represented as:

```text
r_engagement = E(t+1) - E(t)
```

where `E` is a measurable engagement signal defined by the environment or interaction log.

This signal should be combined with explicit positive/negative feedback and task outcome rewards in a documented way.

### 7.4 Important constraint

Reward signals must not be counted repeatedly for the same event.

For example:

- a single user response should not simultaneously count as both positive engagement and explicit feedback if the same interaction has already been recorded as the same event.
- repeated negative feedback from the same interaction should not be double-counted.

### 7.5 Relation to `INSIST`

The `INSIST` action is intended to allow the agent to learn when persistence is beneficial and when it is counterproductive.

This should be learned through the reward signal rather than by hard-coded suppression.

Example:

- bark gets a positive response
- repeated bark without response may cause a negative reward
- eventually the agent learns to avoid repeated insistence in the same circumstances

---

## 8. Baseline vs proposed prototype strategy

The first working prototype should be a baseline-driven environment and learning loop before memory and affective-state augmentation are introduced.

### 8.1 Goal of the initial prototype

The initial prototype should establish:

- finite state encoding
- bounded action space
- well-defined rewards
- Q-learning update rule
- action selection with exploration
- stable persistence of Q-table values

### 8.2 Memory and affect integration order

Memory and affective-state variables should be added only after the base prototype is working and validated.

The proposed integration order is:

1. baseline environment + Q-learning loop
2. memory component validated independently
3. affective state validated independently
4. state builder combines environment + memory + affective features
5. proposed agent comparison runs

### 8.3 Why this order matters

This reduces debugging complexity and ensures that differences in behaviour can be attributed to intended design additions rather than to unstable state definitions or reward functions.

---

## 9. Acceptance criteria for this specification

The Task 2 contract is considered sufficient when the following are true:

1. The observation features are finite and documented.
2. The action space is fixed and bounded.
3. The state-key scheme is deterministic and reproducible.
4. The reward model is explicit and testable.
5. The prototype can be implemented without hidden assumptions.
6. The extension path for memory and affect is explicit but staged.
7. The baseline and proposed agent remain comparable under the same environment and action definitions.

---

## 10. Open decisions that remain external to this specification

The following items still need explicit decisions when their dependent subsystem is implemented:

- precise environmental thresholds for `NEAR`, `MEDIUM`, `FAR`
- exact user interaction-status categories
- whether engagement is environment-driven or interaction-driven
- exact reward tuning values after the first working prototype is tested
- how memory-derived features are encoded once memory is added
- how affective values are discretised when those features are introduced

These decisions are intentionally deferred until the minimal baseline loop is validated.

---

## 11. Implementation guidance

The first implementation should not attempt to solve all of these at once.

The recommended sequence is:

1. define the bounded observation model
2. choose the fixed action set
3. define the stable state encoding
4. define reward outcomes and edge cases
5. implement Q-learning updates for the base prototype
6. validate the prototype in a small deterministic environment
7. add memory and affect separately

This is the smallest complete, testable, and scientifically defensible progression for the prototype.
