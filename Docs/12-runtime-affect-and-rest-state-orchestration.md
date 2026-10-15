# Runtime Affect and Rest-State Orchestration

Status: Draft specification for approval

## Purpose

This stage defines how affective state is updated and how low-confidence facts are created during rest states. The purpose is to connect the previously scoped affective-state model to the runtime lifecycle without turning the affective signal into a broad or unbounded behavioural policy.

This stage should remain bounded and should only influence the state-building path and aggregated memory candidates. It should not redefine the action policy directly.

## Authority and scope

This specification is a continuation of:

- [Docs/05-memory-affect-scaffolding.md](Docs/05-memory-affect-scaffolding.md)
- [Docs/06-memory-affect-state-integration.md](Docs/06-memory-affect-state-integration.md)
- [Docs/09-persistence-and-rest-aggregation.md](Docs/09-persistence-and-rest-aggregation.md)
- [Docs/project-context.md](Docs/project-context.md)

## Required runtime behaviour

The runtime affect engine should:

1. Update affective variables from observation, interaction, and reward outcomes.
2. Keep all variables bounded within the existing explicit affect range.
3. Expose a compact summary value for `affect_trend`.
4. Trigger a rest-state aggregation cycle only when the system is in a low-activity or idle state.
5. Convert repeated low-support interactions into low-confidence candidate facts rather than hard memory claims.

## Affect update rules

The affect model should continue to use the explicit bounded state defined in the project’s earlier contracts.

Required variables:

- valence
- energy
- arousal
- engagement
- stress
- affinity

Important rule:

- energy must retain the special exponential-low-energy penalty already established earlier in the project
- low energy should disproportionately reduce the effective affective trend when the agent is depleted or disengaged

## Rest-state aggregation rules

When the agent enters a rest or idle state, it may aggregate recent low-confidence observations into candidate memory facts.

Candidate generation should only happen when all of the following are true:

- the system is in a rest state
- a repeated pattern has occurred across multiple interactions
- the candidate fact has low confidence but consistent support
- the fact is not yet strong enough to be treated as a high-confidence memory

### Example candidate fact

```cpp
struct AggregatedFactCandidate {
    std::string subject;
    std::string relation;
    std::string object;
    float confidence = 0.0f;
    int support_count = 0;
    float generated_from_rest = 0.0f;
};
```

Such candidates must remain low-confidence until later reinforcement or confirmation.

## Interaction with state construction

The runtime affect layer should feed only into:

- current affective summary
- affective trend value used in state encoding
- aggregated fact candidate generation during rest states

It must not directly manipulate the Q-table or action policy in this stage.

## Acceptance criteria

This stage is considered complete when:

- affective variables remain within their bounded ranges
- energy has the required low-energy exponential weighting behaviour
- rest-state aggregation produces only low-confidence candidate facts
- low-confidence candidate facts are stored separately from high-confidence memory records
- state identity changes when the affective trend changes, but baseline policy logic remains unchanged

## Important boundary

This stage is still not the full simulation environment. It is the runtime logic that prepares the state and evidence layer for later integration into a richer environment loop.
