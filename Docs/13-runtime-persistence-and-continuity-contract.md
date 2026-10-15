# Runtime Persistence and Continuity Contract

Status: Draft specification for approval

## Purpose

This stage defines the runtime contract for persisting the agent’s long-lived state between sessions and reinitialisations. The design must cover the persistence of baseline learning state, memory records, affective state, session context, interaction history, and low-confidence rest-state facts.

This stage is a runtime continuity layer. It does not implement the simulation environment itself and should remain separated from the decision policy.

## Authority and scope

This specification should be read alongside:

- [Docs/09-persistence-and-rest-aggregation.md](Docs/09-persistence-and-rest-aggregation.md)
- [Docs/10-runtime-state-lifecycle.md](Docs/10-runtime-state-lifecycle.md)
- [Docs/11-live-memory-activation-and-ranking.md](Docs/11-live-memory-activation-and-ranking.md)
- [Docs/project-context.md](Docs/project-context.md)

## Persistence domains

The system should persist the following domains:

1. Q-table / learning state
2. memory records
3. affective state
4. session context
5. interaction history
6. aggregated low-confidence candidate facts

## Save/load lifecycle

The runtime save sequence should be deterministic and ordered:

1. save Q-table state
2. save memory records
3. save affective state
4. save session context
5. save interaction history
6. save low-confidence aggregated facts

The load sequence should reverse this order and validate compatibility before continuing.

Required rules:

- the application must fail safely if a persisted file is corrupted
- all persisted records must be versioned or tagged with a schema version
- baseline Q-learning should continue to work after a restart using the same state contract
- memory records and session context should load without undefined defaults when absent

## Persistence schema expectations

The persisted data should be structured in a stable JSON or equivalent format with explicit domain names and versions.

```json
{
  "schema_version": 1,
  "q_table": {},
  "memory_records": [],
  "affective_state": {},
  "session_context": {},
  "interaction_history": [],
  "aggregated_facts": []
}
```

This is a compact example shape; the final schema should remain explicit and testable.

## Runtime continuity rules

- restore affective state before building the next runtime state
- restore session context before the memory retrieval step
- regenerate the runtime state after loading from persisted state without breaking the baseline state contract
- keep aggregated low-confidence facts separate from verified memory facts until a reinforcement step is explicitly executed

## Approval gate

This stage is complete when:

- persisted domains are serialised atomically or in a clearly ordered way
- the system can reload them without corrupting baseline state identity
- the runtime state rebuild remains deterministic after a restart
- low-confidence aggregated facts are not treated as high-confidence memory without explicit reinforcement

## Boundary

This stage still precedes the creation of the actual simulation environment. It is a continuity and runtime readiness layer that ensures the agent can recover its internal state across sessions.
