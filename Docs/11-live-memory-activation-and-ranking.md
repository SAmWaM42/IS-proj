# Live Memory Activation and Ranking

Status: Draft specification for approval

## Purpose

This stage defines the runtime memory-selection layer that retrieves and ranks candidate records from stored memory before they are condensed into the bounded memory signal. The memory signal remains intentionally narrow and should influence state construction only through a bounded summary, not through direct action policy logic.

This stage is a runtime extension of the earlier retrieval and ranking work but is defined here as a distinct pre-simulation integration stage.

## Authority and scope

This specification should be read alongside:

- [Docs/08-live-memory-retrieval-and-ranking.md](Docs/08-live-memory-retrieval-and-ranking.md)
- [Docs/10-runtime-state-lifecycle.md](Docs/10-runtime-state-lifecycle.md)
- [Docs/project-context.md](Docs/project-context.md)

## Required behaviour

The runtime memory layer should do the following:

1. Load a bounded number of candidate records from memory storage.
2. Score each record on relevance to the current session or current context.
3. Combine confidence, recency, scope, and relevance into a single score.
4. Filter out noisy or low-support records below a configurable threshold.
5. Produce a compact `MemorySummary` that can be safely passed into state construction.

## Proposed ranking score

The score should be a bounded scalar derived from the following factors:

- confidence weight
- relevance weight
- recency decay
- scope matching
- session affinity
- memory weight (optional reinforcement factor from prior record importance)

The ranking function should remain deterministic for a given memory set and context.

### Example form

```cpp
float computeMemoryScore(const MemoryRecord& record, const RuntimeContext& context) {
    float score = 0.0f;
    score += record.confidence * 0.35f;
    score += record.relevance * 0.30f;
    score += record.recency_score * 0.20f;
    score += record.scope_match * 0.15f;
    return std::clamp(score, 0.0f, 1.0f);
}
```

This formula is illustrative. The final implementation should use the project’s already-established bounded numeric conventions.

## Memory summary contract

The retrieval layer should return a compact summary rather than raw memory records.

```cpp
struct MemorySummary {
    float memory_bias = 0.0f;
    float signal_quality = 0.0f;
    float recency_weight = 0.0f;
    float confidence_weight = 0.0f;
    int record_count = 0;
    std::vector<std::string> top_tags;
};
```

The summary must not expose the full memory record list to the state builder. The state builder should consume only the bounded summary values.

## Interaction with state construction

The memory retrieval stage should feed only into the bound memory signal. It must not directly set Q-values or inject raw memory logic into the policy update rule.

Required rules:

- the summary is bounded to a stable numeric range
- noisy matches are discarded before summarisation
- repeated retrievals for the same context remain consistent
- the memory summary can be empty without invalidating the state path

## Approval gate

This stage is considered complete when:

- ranked memory retrieval is deterministic for a fixed input set
- the top-ranked records are chosen by a transparent, bounded scoring rule
- the condensed memory summary is the only memory signal passed into state construction
- the baseline Q-learning behaviour remains unchanged when memory is absent or neutral

## Notes on scope

The runtime memory stage is intentionally not a full cognitive architecture. It is specifically a retrieval-and-ranking layer for the bounded state signal required by the project’s staged design.
