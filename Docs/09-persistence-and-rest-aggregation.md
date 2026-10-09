# Persistence Layer and Rest-State Data Aggregation

Status: implemented and validated in the current working tree.

## Objective

Define the persistence layer for the project as a bounded storage feature that covers:

- Q-table state
- memory records
- affective-state snapshot
- session context
- interaction history
- low-confidence fact aggregation produced during rest states

This feature is intended to preserve agent state across process restarts and provide a structured mechanism for consolidating repeated observations into memory-level facts without changing the current Q-learning policy semantics.

## Scope and boundary

This feature includes:

- serialising learned Q-table values
- serialising memory records and their metadata
- serialising affective-state data and recent session context
- storing interaction logs for later review and summarisation
- aggregating repeated low-confidence observations into a fact-like memory record during rest states

This feature does not include:

- adding new action types
- directly modifying Q-table learning during persistence
- altering the current action-selection policy
- unbounded memory growth
- full memory retrieval logic (already covered in the retrieval stage)

## High-level architecture

The persistence layer should remain a separate data service layer that sits below the memory and state subsystems.

Recommended logical flow:

1. runtime interaction produces events
2. interaction history is appended to the session log
3. session context snapshot is updated
4. affective state is updated from outcomes and feedback
5. when the agent enters a rest state, low-confidence observations are aggregated
6. aggregated facts are stored as low-confidence memory records
7. Q-table, memory records, affective state, and session context are persisted to disk

## Data types to persist

### 1. Q-table

Persist the existing tabular Q-learning values.

Required fields:

- state key
- action name
- q-value

Storage format:

- plain text or JSON format is acceptable
- text format is already supported in the current `QTable::save()`/`QTable::load()` implementation

### 2. Memory records

Persist memory records in a stable JSON structure.

Required fields:

- subject
- relation
- object
- confidence
- generalization_factor
- scope
- source
- observation_count
- last_updated
- relevance
- recency
- relevant_users

This should be the canonical record representation used by the memory subsystem.

### 3. Affective state

Persist the current affective-state values.

Required fields:

- valence
- energy
- arousal
- engagement
- stress
- affinity

This allows the system to resume with continuity across restarts.

### 4. Session context

Persist recent context information that is useful for continuing a session or re-establishing situational grounding.

Required fields:

- current intent or most recent interaction type
- target proximity
- user status
- last known energy and stress levels
- previous action summary
- last interaction timestamp

This is a short-lived but important continuity layer for the agent's working state.

### 5. Interaction history

Persist a bounded interaction log containing recent events.

Required fields:

- timestamp
- action taken
- user interaction summary
- reward or feedback
- observed state summary
- affective context summary

The interaction log should be bounded in size so it remains practical for long-running sessions.

## Rest-state aggregation feature

This is a specific feature requested for the project and should be treated as an explicit part of the persistence layer.

### Purpose

During rest states, the agent should aggregate repeated interaction logs into low-confidence facts that can later become memory entries if they are reinforced.

This is not a high-confidence memory insertion process. It is a summarisation step that produces candidate memory records with intentionally low confidence.

### Trigger condition

Aggregation occurs only in a rest state.

A rest state is defined as a period where:

- there is no active user-driven interaction requiring immediate action
- the system is in a quiet or idle phase
- recent interaction logs are stable enough to summarise without acting on them immediately

This should be modelled as an explicit state flag or mode shift, not as an ad hoc condition.

### Aggregation rule

The process should:

1. collect interaction history over a bounded window
2. extract repeated observations or patterns
3. group similar facts by subject/relation/object similarity
4. compute a low-confidence aggregate score
5. create a candidate memory record with low confidence
6. store it with a low initial confidence and a pending reinforcement tag if required

The aggregate record should be intentionally low confidence until later evidence raises it.

Recommended aggregation formula:

```
aggregate_confidence =
    average of interaction-specific confidence values
    weighted by recency and frequency
```

with a cap such as:

```
aggregate_confidence <= 0.35f
```

This keeps aggregated facts in the low-confidence range until they are reinforced by subsequent evidence.

### Reinforcement rule

A low-confidence aggregated fact should remain available for later reinforcement.

If a fact is later supported by repeated interaction evidence, then:

- confidence increases
- relevance may increase
- the fact can be promoted into a higher-priority memory record

This is explicitly a later stage of the memory lifecycle, not a direct action-policy update.

## Persistence file structure

A practical structure is a single directory or bundle containing sub-files for each domain.

Suggested layout:

```text
Data/
  qtable.json
  memory_records.json
  affect_state.json
  session_context.json
  interaction_history.json
  aggregated_facts.json
```

This is simpler than a single monolithic JSON file and keeps each subsystem easier to inspect and validate.

## Save and load semantics

### Save phase

The save operation should checkpoint all runtime state that may affect continuity:

- current Q-table
- current memory records
- current affective state
- current session context
- recent interaction history
- low-confidence aggregate facts generated during rest states

### Load phase

The load operation should restore state in a safe order:

1. Q-table
2. memory records
3. affective state
4. session context
5. interaction history
6. aggregate facts

This order minimises invalid state when the system resumes.

## Validation criteria

The persistence feature is considered complete when all of the following pass:

1. Q-table values persist and reload without drift
2. memory records persist with all required metadata
3. affective state persists and restores the same bounded values
4. session context restores recent state continuity
5. interaction history reloads in correct chronological order
6. low-confidence aggregate facts are created only during rest states
7. aggregated facts remain low-confidence until reinforced
8. regenerating the state after load does not break the baseline Q-learning validation

## Implementation boundary

This feature should be implemented as a storage and summarisation layer only.

It should not:

- add new policy decisions
- alter reward semantics
- activate direct memory-to-policy behavior
- force action selection to depend on persisted memory records during the same cycle

The persistence layer must preserve agent continuity while leaving policy logic under the current controlled staged plan.

## Recommended next stage

Once this spec is approved, the implementation path should be:

1. add save/load helpers for the Q-table
2. add save/load helpers for memory records and affective state
3. add session context and interaction history persistence
4. add rest-state aggregation logic for low-confidence facts
5. validate the persisted state against the baseline learning loop
