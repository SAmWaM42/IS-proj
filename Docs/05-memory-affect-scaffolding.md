# Memory and Affect Scaffolding

## Status

This document defines the minimal memory and affect scaffolding for the next staged integration step. It is intentionally smaller than a full implementation and is designed to extend the validated baseline loop without disturbing the current Q-learning contract.

## Purpose

The baseline learning loop is validated and stable. The next step is to add the minimal contract objects needed for memory-derived state and affective-state signals, without yet wiring them into the decision policy.

## Scope

This stage includes:

- a minimal memory record format
- a minimal affective-state structure
- explicit source and scope fields for memory entries
- a place to store affective updates from interaction outcomes
- a documented separation between memory/affect and the Q-learning policy

This stage does not include:

- full memory retrieval logic
- full affective learning rules
- full simulation integration
- Q-table changes based on memory or affective features

## Minimal contract

### Memory record

A memory record should capture:

- subject
- relation
- object
- confidence
- generalization factor
- scope (`GENERAL` or `USER_SPECIFIC`)
- source (`USER_STATEMENT`, `OBSERVATION`, `ACTION_HISTORY`, `INFERENCE`)
- observation count
- last updated timestamp or label
- relevance score

This makes the memory layer explicit and allows future retrieval logic to rank records without depending on ad hoc structures.

### Affective state

The affective state should expose a small set of bounded variables:

- valence (`NEUTRAL`, `POSITIVE`, `NEGATIVE`)
- energy (`LOW`, `MODERATE`, `HIGH`)
- arousal
- engagement
- stress
- affinity

These variables are intentionally bounded and simple so they can be stored and tested without introducing broad behavioral complexity.

## Integration rule

Memory and affect must remain separate from the baseline Q-learning update path until they have been validated independently.

In this stage:

- the memory structure exists
- the affect structure exists
- the data model is documented
- the baseline Q-learning loop remains unchanged and still passes validation

This preserves a clean boundary: memory and affect are available as structured inputs for future state construction, but they are not yet used to alter the main decision policy.

## Acceptance criteria

The next stage is considered complete when:

1. Memory records can represent fact-like observations in a bounded form.
2. Affective state has explicit, bounded variables.
3. The memory/affect contract remains independent from the baseline Q-table logic.
4. The project still builds and the baseline validation still passes.

## Notes

This is intentionally a contract-only expansion, not a full behavior change. The aim is to prepare for the next integration step in a controlled, explainable way.
