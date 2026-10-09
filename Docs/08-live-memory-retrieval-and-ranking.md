# Live Memory Retrieval and Ranking

Status: completed and validated in the current working tree.

## Objective

Implement a bounded retrieval-and-ranking stage that selects the most relevant memory records, computes a weighted summary, and feeds that summary into the existing `memory_bias` state signal without altering the baseline Q-learning policy.

The stage is intentionally constrained to the memory layer and state-construction layer. The policy still selects actions using the same Q-learning mechanism and the same action set.

## Design rule

The retrieval stage must not make the policy directly dependent on memory in a way that changes action selection semantics.

Instead, memory is used to update the state summary in a bounded way:

- retrieve relevant records
- rank them by importance
- compute a weighted memory signal
- clamp the signal to a bounded range
- fold the result into the state encoding via `memory_bias`

This preserves the clean staged architecture while still allowing memory evidence to influence state identity.

## Memory record weighting

Each record is assigned a weight that reflects the evidence strength of that memory.

```
record_weight =
    0.45 * confidence +
    0.30 * relevance +
    0.15 * recency +
    0.10 * scope_bonus
```

where:

- confidence is in [0, 1]
- relevance is in [0, 1]
- recency is in [0, 1]
- scope_bonus is 0.10 when `current_context == USER_SPECIFIC`, otherwise 0.0

The final weight is clamped to [0, 1].

## Record signal value

The per-record signal value is the contribution of that record toward the agent's memory salience.

```
record_signal =
    0.5 * confidence +
    0.5 * relevance
```

This keeps the signal bounded and consistent with the state summary contract.

## Retrieval and ranking

Relevant records are ranked by a score computed from the record weight plus a query match bonus.

```
retrieval_score = record_weight + query_bonus
```

with:

- query_bonus = 0.15 when a query string matches the record text
- otherwise query_bonus = 0.0

The ranked records are then ordered from strongest to weakest evidence.

## Weighted memory summary

The memory signal passed to state construction is a weighted average of the ranked candidate records.

```
weighted_memory_signal =
    sum(record_weight_i * record_signal_i)
    /
    sum(record_weight_i)
```

This produces a single bounded memory signal that reflects both memory strength and memory importance.

The result is clamped to [0, 1].

## State integration

The weighted summary is then converted into the memory bias bucket in the same manner as the bounded memory/affect integration stage.

```
if weighted_memory_signal < 0.33 -> memory_bias = 0
if weighted_memory_signal < 0.66 -> memory_bias = 1
otherwise -> memory_bias = 2
```

The retrieval stage therefore influences the state by changing the memory bias signal, not by directly altering the Q-learning update logic.

## Code changes implemented

### Memory layer

- added `recency` to the record model
- added `calculateMemoryRecordWeight(...)`
- added `calculateRecordSignal(...)`
- added `rankRelevantRecords(...)`
- added `summarizeWeightedMemorySignal(...)`
- updated `summarizeMemorySignal(...)` to delegate to the weighted summary logic

### State-construction layer

- updated `makeStateFromMemoryRecords(...)` to use the ranked and weighted summary instead of a simple average
- preserved the existing baseline `makeState(...)` behavior
- preserved the low-energy exponential penalty in the affective trend

## Why this remains bounded

This stage is deliberately narrow:

- retrieval is explicit and ranked
- weighting is explicit and bounded
- summarization is explicit and bounded
- the Q-learning policy is unchanged
- action selection remains unchanged

This makes the stage explainable, testable, and aligned with the staged project architecture.

## Validation requirements

The stage is considered complete when all of the following are true:

1. memory records can be ranked by strength and recency
2. weighted summary changes when higher-confidence or higher-relevance records are added
3. the resulting memory bias changes the state identity in a deterministic way
4. the baseline Q-learning validation still passes
5. no new action or policy semantics are introduced

## Acceptance criteria

The current implementation is accepted when:

- ranked memory records produce a stronger signal than weak or empty records
- the weighted summary reflects both evidence strength and relevance
- state construction uses the weighted summary for `memory_bias`
- baseline validation remains green
