# Implementation Status and Roadmap

Status: current working record for the repository

## Authority and scope

This document records the implementation status of the current project state and maps each completed or pending work item to the relevant specification.

The authoritative project baseline remains [Docs/project-context.md](Docs/project-context.md). This document is a working record of implementation progress and not a replacement for the project-context document.

## Current in-progress work

### Stage −1 — Shared action architecture and complexity reduction

**Status:** Source edits committed; compile and test verification pending.

Implemented changes:
- centralised the 14 actions and shared observable-context validity rules
- masked invalid actions during selection and next-state Q-value bootstrapping for both agents
- added food/play affordances to observations and encoded them into policy state
- expanded synthetic training and participant-study contexts for food, play, and negative feedback
- centralised UI action labels and action-to-animation grouping
- removed the disconnected legacy backyard simulation, placeholder environment scaffolding, unused action hierarchy, duplicate comparison harness, unused Q-learning helpers, and no-op training-data loader

Tracking document: [Docs/20-stage-minus-one-action-architecture.md](20-stage-minus-one-action-architecture.md)

**Remaining gate:** Build the project, run `--self-test`, and verify that training never selects a masked action before beginning Stage 0.

## Completed work

### 1. Baseline learning loop validation

Completed and validated.

Summary:
- established a minimal tabular Q-learning baseline
- validated Q updates, unseen state handling, persistence, and terminal-state handling
- preserved the baseline as the stable control condition for later staged extensions

Relevant spec:
- [Docs/03-baseline-learning-loop.md](Docs/03-baseline-learning-loop.md)
- [Docs/04-baseline-learning-tests.md](Docs/04-baseline-learning-tests.md)

Implementation files:
- [Components/State-Action/state-action.h](Components/State-Action/state-action.h)
- [Components/State-Action/state-action.cpp](Components/State-Action/state-action.cpp)

### 2. Memory and affect scaffolding

Completed.

Summary:
- defined explicit memory and affect data contracts
- kept memory and affect separate from the baseline policy logic
- ensured the project architecture remained modular and reviewable

Relevant spec:
- [Docs/05-memory-affect-scaffolding.md](Docs/05-memory-affect-scaffolding.md)

Implementation files:
- [Components/Memory/memory.h](Components/Memory/memory.h)
- [Components/Memory/memory.cpp](Components/Memory/memory.cpp)

### 3. Bounded memory/affect state integration

Completed and validated.

Summary:
- added bounded `memory_bias` and `affect_trend` fields to the state model
- added `makeStateWithContext(...)` and `makeStateFromMemoryRecords(...)`
- extended the mixed-radix encoder with these bounded state signals
- included the low-energy exponential penalty in affective trend calculation
- validated that changing the live memory signal or affective trend changes state identity without changing the Q-learning policy

Relevant spec:
- [Docs/06-memory-affect-state-integration.md](Docs/06-memory-affect-state-integration.md)

Implementation files:
- [Components/State-Action/state-action.h](Components/State-Action/state-action.h)
- [Components/State-Action/state-action.cpp](Components/State-Action/state-action.cpp)
- [Components/Memory/memory.h](Components/Memory/memory.h)
- [Components/Memory/memory.cpp](Components/Memory/memory.cpp)

## Pending work

### 1. Live memory retrieval and ranking

Status: completed and validated.

Intent:
- move from abstract memory summaries to a retrieval layer that selects and ranks relevant records from stored memory
- keep the memory summary bounded and deterministic when passing into state construction

Expected work items:
- define retrieval ranking rules for confidence, relevance, recency, and scope
- add a dedicated retrieval or memory-selection function
- connect the selection result to the bounded memory summary function
- add acceptance tests demonstrating retrieval-driven state changes

Spec status:
- completed in [Docs/08-live-memory-retrieval-and-ranking.md](Docs/08-live-memory-retrieval-and-ranking.md)

### 2. Persistence layer and rest-state aggregation

Status: completed and validated.

Intent:
- define persistence for Q-table, memory records, affective-state data, session context, and interaction history
- support rest-state aggregation of interaction logs into low-confidence candidate memory facts
- preserve continuity across restarts without broadening the policy layer

Expected work items:
- define JSON or text structure for each persisted domain
- define save/load semantics and ordering
- define rest-state trigger and aggregation logic
- define how low-confidence aggregated facts are reinforced later
- validate persistence without disturbing the baseline Q-learning loop

Spec status:
- implemented in [Docs/09-persistence-and-rest-aggregation.md](Docs/09-persistence-and-rest-aggregation.md)

### 3. Runtime state lifecycle and observation normalisation

Status: pending specification review

Intent:
- define the runtime handoff from interaction and observation events into a consistent bounded state
- keep the state builder deterministic and compatible with the baseline state contract

Expected work items:
- normalise incoming interaction records into a runtime observation model
- preserve the baseline state key when memory/affect inputs are empty
- gate memory and affect features behind bounded runtime values

Spec status:
- draft in [Docs/10-runtime-state-lifecycle.md](Docs/10-runtime-state-lifecycle.md)

### 4. Live memory activation and ranking

Status: pending specification review

Intent:
- define the runtime retrieval layer that ranks prospective memory records before summarisation
- ensure the retrieved memory signal is compact and safe for state construction

Expected work items:
- score records using confidence, relevance, recency, and context match
- filter noise and low-support memory entries
- pass only a compact summary into the state builder

Spec status:
- draft in [Docs/11-live-memory-activation-and-ranking.md](Docs/11-live-memory-activation-and-ranking.md)

### 5. Runtime affect and rest-state orchestration

Status: pending specification review

Intent:
- connect affective trend updates and rest-state candidate generation to the runtime lifecycle
- keep affective and rest-state updates bounded and non-policy-breaking

Expected work items:
- update affective variables from runtime events and outcomes
- apply the low-energy exponential weighting rule
- aggregate repeated low-confidence observations only during rest states

Spec status:
- draft in [Docs/12-runtime-affect-and-rest-state-orchestration.md](Docs/12-runtime-affect-and-rest-state-orchestration.md)

### 6. Runtime persistence and continuity contract

Status: pending specification review

Intent:
- define a runtime save/load contract for the learning state, memory, affect, session, and low-confidence fact domains
- preserve continuity across restarts without broadening the learning policy

Expected work items:
- define the persisted schema and ordering for all domains
- validate restart-safe loading semantics
- keep aggregated low-confidence facts separate from confirmed memory entries

Spec status:
- draft in [Docs/13-runtime-persistence-and-continuity-contract.md](Docs/13-runtime-persistence-and-continuity-contract.md)

### 7. Pre-simulation environment contract

Status: pending specification review

Intent:
- define the environment boundary and observation/action types before the actual simulation engine is built
- prepare a controlled runtime contract for later simulation implementation

Expected work items:
- define the observation/action types and reward contract
- establish the agent-environment boundary
- confirm that state, memory, and affect contracts remain stable before the environment layer is built

Spec status:
- draft in [Docs/14-pre-simulation-environment-contract.md](Docs/14-pre-simulation-environment-contract.md)

### 8. Baseline and proposed agent comparison setup

Status: pending specification approval

Intent:
- define the minimum class and harness design needed to compare a baseline Q-learning agent against the proposed memory-and-affect agent in the same configured environment
- ensure both agents share the same simulation contract and reward semantics before training begins

Expected work items:
- define the common environment contract and reward structure
- define the proposed-agent class responsibilities and state members
- define the baseline-agent class responsibilities and exclusions
- define an evaluation harness that runs both agents under matching conditions
- collect comparable metrics for action choice, reward totals, and final state

Spec status:
- draft in [Docs/15-proposed-agent-comparison-spec.md](Docs/15-proposed-agent-comparison-spec.md)
- draft in [Docs/16-baseline-agent-comparison-spec.md](Docs/16-baseline-agent-comparison-spec.md)

### 9. Full affect-driven decision policy integration

Status: pending

Intent:
- extend the learning or decision policy from state summaries into policy-level selection or action preference adjustments
- only after the state-summary route is proven stable and reviewable

Expected work items:
- define when affective trend influences action preference
- decide whether effect is direct or indirect
- keep the policy bounded and testable

Spec status:
- no dedicated implementation spec yet; this should remain a later-phase specification

### 10. Environment and simulation integration

Status: pending

Intent:
- connect the agent state, memory, and affective signal to a simulation environment
- produce observable behavior and reward updates

Expected work items:
- environment observation model
- action execution flow
- reward generation
- state reset and terminal-event handling within the broader simulation context

Spec status:
- reference the higher-level system architecture in [Docs/project-context.md](Docs/project-context.md) and the baseline state contract in [Docs/03-baseline-learning-loop.md](Docs/03-baseline-learning-loop.md)

## Current implementation map

| Stage | Status | Specification |
| --- | --- | --- |
| Baseline Q-learning | Completed | [Docs/03-baseline-learning-loop.md](Docs/03-baseline-learning-loop.md) |
| Baseline validation | Completed | [Docs/04-baseline-learning-tests.md](Docs/04-baseline-learning-tests.md) |
| Memory/affect scaffolding | Completed | [Docs/05-memory-affect-scaffolding.md](Docs/05-memory-affect-scaffolding.md) |
| Bounded memory/affect state integration | Completed | [Docs/06-memory-affect-state-integration.md](Docs/06-memory-affect-state-integration.md) |
| Live memory-to-state wiring | Completed in the current working tree | [Docs/06-memory-affect-state-integration.md](Docs/06-memory-affect-state-integration.md) |
| Retrieval and ranking layer | Completed | [Docs/08-live-memory-retrieval-and-ranking.md](Docs/08-live-memory-retrieval-and-ranking.md) |
| Persistence layer + rest-state aggregation | Completed | [Docs/09-persistence-and-rest-aggregation.md](Docs/09-persistence-and-rest-aggregation.md) |
| Runtime state lifecycle and observation normalisation | Pending specification review | [Docs/10-runtime-state-lifecycle.md](Docs/10-runtime-state-lifecycle.md) |
| Live memory activation and ranking | Pending specification review | [Docs/11-live-memory-activation-and-ranking.md](Docs/11-live-memory-activation-and-ranking.md) |
| Runtime affect and rest-state orchestration | Pending specification review | [Docs/12-runtime-affect-and-rest-state-orchestration.md](Docs/12-runtime-affect-and-rest-state-orchestration.md) |
| Runtime persistence and continuity contract | Pending specification review | [Docs/13-runtime-persistence-and-continuity-contract.md](Docs/13-runtime-persistence-and-continuity-contract.md) |
| Pre-simulation environment contract | Pending specification review | [Docs/14-pre-simulation-environment-contract.md](Docs/14-pre-simulation-environment-contract.md) |
| Full affect-driven policy usage | Pending | not yet specified |
| Simulation integration | Pending | reference project-context |

## Working principle for future stages

Future work should remain in small, independently testable increments and should always be validated against the stable baseline before any broader feature extension is attempted.

This preserves the staged approach defined in [Docs/project-context.md](Docs/project-context.md), while the implementation record in this file keeps track of what is already proven and what remains intentionally deferred.
