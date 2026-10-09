# Baseline Learning Test Specification

## Status

Draft specification for deterministic validation of the baseline learning loop.

This document defines the smallest set of tests needed to validate the baseline implementation before memory and affective-state components are introduced.

---

## 1. Purpose

The baseline learning loop must be validated against deterministic, hand-checkable scenarios before it is extended.

These tests are meant to confirm that the system correctly:

- encodes states from environmental observations
- selects valid actions
- updates Q-values according to the Q-learning rule
- handles terminal transitions
- handles unseen states without undefined behaviour
- persists Q-table values consistently

---

## 2. Required test boundaries

The tests below intentionally avoid semantic memory, NLP, and affective processing.

They validate only the rules described in the baseline learning loop specification.

---

## 3. Test 1: Basic Q-value update

### Objective

Verify that a single Q-learning update produces the expected value for a known state-action pair.

### Setup

- initial Q-value: `Q(s, a) = 0.0`
- learning rate: `alpha = 0.1`
- discount factor: `gamma = 0.9`
- reward: `r = 1.0`
- next state `s'` has `max(Q(s', a')) = 0.5`

### Expected calculation

```text
Q(s, a) = 0.0 + 0.1 * (1.0 + 0.9 * 0.5 - 0.0)
        = 0.1 * (1.0 + 0.45)
        = 0.145
```

### Acceptance criteria

- the updated value is approximately `0.145`
- the exact floating-point tolerance should be documented in the unit test and kept small
- the update is applied only to the selected action for the current state

---

## 4. Test 2: Unseen state handling

### Objective

Verify that a previously unseen state behaves predictably and does not crash or produce undefined behaviour.

### Setup

- create a state not already present in the Q-table
- choose any valid action from the fixed action list
- call the action selector and update routine

### Expected behaviour

- the Q-table inserts the state/action entry with a defined default value
- the action selection returns a valid action from the allowed set
- no uninitialised memory access occurs

### Acceptance criteria

- no crash occurs
- returned action is always valid
- missing entries are initialised consistently

---

## 5. Test 3: Epsilon-greedy policy validation

### Objective

Verify that action selection follows the epsilon-greedy rule.

### Setup

- configure `epsilon = 0.0`
- create a state with a known highest-value action
- confirm that the agent picks the highest-value action

Then:

- configure `epsilon = 1.0`
- verify that the action selector may choose any valid action

### Acceptance criteria

- when `epsilon = 0.0`, exploitation is selected deterministically for the known best action
- when `epsilon = 1.0`, the system explores and returns a valid action from the action set
- no invalid action is returned

---

## 6. Test 4: Terminal-state update handling

### Objective

Check that terminal transitions are handled cleanly and consistently.

### Setup

- define a transition to a terminal state where the next state is marked terminal
- apply the Q-learning update using the terminal state

### Expected behaviour

- the update routine still computes a valid Q-value
- the terminal state is treated as a valid next-state representation
- no infinite recursion or bad-state handling occurs

### Acceptance criteria

- terminal transitions are processed without invalid state access
- Q-value update still follows the standard rule
- the terminal flag is documented and interpreted consistently

---

## 7. Test 5: Q-table persistence compatibility

### Objective

Verify that the Q-table can be saved and reloaded without changing the state-action mapping.

### Setup

- initialise a small Q-table with a few state-action values
- save to a temporary file
- reload the same file
- compare loaded values against the original values

### Acceptance criteria

- the same state/action identifiers resolve to the same numeric values after reload
- file format is consistent and documented
- malformed or missing files should be handled explicitly but not silently ignored

---

## 8. Test 6: reward consistency in the minimal environment

### Objective

Ensure the reward model behaves consistently for a known environment transition.

### Setup

- construct a minimal environment transition with a known action
- record the resulting reward value
- confirm it matches the documented mapping table

### Acceptance criteria

- the environment and the reward contract agree on the outcome
- no reward is assigned twice for a single event
- explicit negative feedback dominates over neutral engagement when both occur in the same transition

---

## 9. Test harness constraints

These tests must be implemented in a deterministic and isolated manner.

### Required constraints

- no semantic memory components should be involved
- no NLP process should be required
- no affective-state model should be involved
- no rendering or simulation framework is required for these tests
- the environment can be a minimal stub or in-memory state machine

The goal is to validate the learning loop and not to build the whole experimental system yet.

---

## 10. Acceptance criteria for the test suite

The baseline learning-loop test suite is sufficient when all of the following are true:

1. basic Q-learning updates match documented hand calculations
2. unseen states are handled predictably
3. epsilon-greedy behaviour chooses valid actions only
4. terminal transitions are handled explicitly
5. Q-table persistence is stable and compatible
6. reward model mapping is deterministic and documented
7. all tests can be run without involving memory or affective-state systems

---

## 11. Implementation order

The tests should be implemented in the following order:

1. Basic Q-value update
2. Unseen state handling
3. Epsilon-greedy validation
4. Terminal-state handling
5. Q-table persistence validation
6. Reward consistency validation

This sequence narrows the problem and confirms the most fundamental learning logic before any broader integration begins.

---

## 12. Next step

After this test specification is accepted, the next concrete implementation step is to create the minimal baseline Q-learning harness and implement the deterministic tests described here.

This remains a strict baseline-only scope and does not yet include memory, affect, or full simulation integration.
