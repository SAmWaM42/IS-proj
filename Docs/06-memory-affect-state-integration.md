# Bounded Memory/Affect State Integration

Status: completed and validated.

## Objective

Add memory-derived and affective-state inputs to the state-construction path in a bounded, deterministic, and testable way without changing the current Q-learning policy or action space.

This stage has been implemented and validated. The goal was to extend state identity so that memory salience and affective trend affect the encoded state while preserving the validated baseline behavior and keeping the state space small and collision-resistant.

A key refinement for this stage is that energy is treated as an especially important state dimension. In the affective trend, low energy carries an exponential penalty so that the trend shifts sharply as energy drops, making energy a high-priority driver of the state signal.

## Non-goals

This stage must not:

- change the action set
- change reward semantics
- broaden the policy logic
- add new learning behaviors
- perform full memory retrieval or ranking logic
- create an unbounded state expansion

## Core design rule

The project already uses a mixed-radix encoding for the state key. This should remain the canonical encoding mechanism.

The new memory/affect integration must:

- remain bounded to a small integer range
- preserve deterministic encoding
- retain compatibility with existing baseline state keys when no memory or affect signal is present
- remain separately testable from the baseline Q update logic

## Data contract

### 1. Memory bias

Memory bias is a single scalar summary that captures whether recent memory context is weak, moderate, or strong.

It is not a storage model. It is a summary signal for state construction.

Recommended bucket range:

- 0 = negligible memory signal
- 1 = moderate memory signal
- 2 = strong memory signal

Exact normalization rule:

```
normalized_memory = clamp(
    0.45 * confidence +
    0.35 * relevance +
    0.20 * recency,
    0.0f,
    1.0f
);
```

where:

- confidence is in [0, 1]
- relevance is in [0, 1]
- recency is in [0, 1]

Then:

```
memory_bias = 0 if normalized_memory < 0.33f
memory_bias = 1 if normalized_memory < 0.66f
memory_bias = 2 otherwise
```

### 2. Affective trend summary

Affective trend is a bounded summary of the current affective context.

It is computed from the existing `AffectiveState` fields:

- valence
- energy
- arousal
- engagement
- stress
- affinity

Recommended bucket range:

- 0 = negative / draining / low-affinity trend
- 1 = neutral / balanced trend
- 2 = positive / engaged / stable trend

Exact normalization rule:

```
valenceWeight =
    1.0f  if valence == POSITIVE
    0.0f  if valence == NEUTRAL
   -1.0f  if valence == NEGATIVE

energyNorm =
    0.0f if energy == LOW
    0.5f if energy == MODERATE
    1.0f if energy == HIGH

energyLowBias = std::pow(1.0f - energyNorm, 2.0f);

engagementScore = engagement
affinityScore = affinity
arousalScore = arousal
stressPenalty = stress

affect_score =
    0.25f * valenceWeight +
    0.25f * arousalScore +
    0.20f * engagementScore +
    0.15f * affinityScore -
    0.20f * stressPenalty -
    0.35f * energyLowBias;

affect_score = clamp(affect_score, -1.0f, 1.0f);
```

This makes low energy disproportionately influential: when energy falls, the negative bias grows exponentially with the shortage of energy, because `energyLowBias = (1 - energyNorm)^2` increases sharply as energy decreases.

Then:

```
affect_trend = 0 if affect_score < -0.10f
affect_trend = 1 if abs(affect_score) <= 0.10f
affect_trend = 2 otherwise
```

This keeps the signal compact and makes it easy to fold into the mixed-radix state encoding without creating combinatorial growth.

## State schema extension

The `state` struct should be extended with two bounded, explicit fields:

- `memory_bias` captures how strongly memory context influences the current state
- `affect_trend` captures the current emotional tendency, including the low-energy penalty

```
struct state {
    std::map<std::string, distance> environmentCondition;
    Level energy = Level::LOW;
    Level stress = Level::LOW;
    Level curiosity = Level::LOW;
    Level attachment = Level::LOW;
    AffectiveState affect;
    float memory = 0.0f;
    float encoded_user_input = 0.0f;

    int memory_bias = 0;
    int affect_trend = 0;

    std::size_t encode() const;
};
```

These fields are intentionally small and bounded:

- `memory_bias` in {0, 1, 2}
- `affect_trend` in {0, 1, 2}

## State constructor contract

The existing constructor `makeState(...)` remains the stable baseline entry point. To avoid breaking callers, add a new overload or a helper function while preserving the current default behavior.

Recommended helper:

```
struct ContextSummary {
    int memoryBias;
    int affectTrend;
};

ContextSummary summarizeContext(
    float confidence,
    float relevance,
    float recency,
    const AffectiveState& affect);
```

Then:

```
state makeStateWithContext(
    int intent,
    distance proximity,
    Level energy,
    Level stress,
    Level curiosity,
    Level attachment,
    int userStatus = 0,
    float memory = 0.0f,
    float confidence = 0.0f,
    float relevance = 0.0f,
    float recency = 0.0f,
    const AffectiveState& affect = AffectiveState{});
```

This helper must:

- compute `memory_bias`
- compute `affect_trend`
- assign both values into the result
- leave the old `makeState(...)` behavior intact for baseline callers

## Encoding extension

The existing mixed-radix encoding must be extended without changing the baseline structure.

Current encoding structure is conceptually:

- intent
- target proximity
- user status
- energy
- stress
- curiosity
- attachment
- valence
- affect energy
- arousal bucket
- engagement bucket
- memory offset

The new encoding must add the bounded values as extra low-cardinality factors after the current core state features.

Recommended exact order:

1. intent
2. proximity
3. user status
4. energy
5. stress
6. curiosity
7. attachment
8. valence
9. affect energy
10. arousal bucket
11. engagement bucket
12. memory_bias
13. affect_trend
14. memory offset

This keeps the existing structure stable while making memory and affect visible to the state key.

## Exact implementation sequence

This is the stage-by-stage sequence to be followed once approved.

### Step 1: Add helper definitions

Add the summarization logic in the state-action component:

- `summarizeMemoryBias(...)`
- `summarizeAffectTrend(...)`
- `summarizeContext(...)`

These functions should be pure and deterministic.

### Step 2: Extend state fields

Add:

- `int memory_bias = 0;`
- `int affect_trend = 0;`

### Step 3: Add state-construction helper

Add `makeStateWithContext(...)` while leaving `makeState(...)` intact.

### Step 4: Extend `state::encode()`

Update the mixed-radix calculation to include the two bounded summary values.

### Step 5: Add validation tests

Add tests that verify:

- baseline encoding unchanged when bias/trend are zero
- changing memory bias changes state identity
- changing affect trend changes state identity
- repeated calls with identical inputs are stable
- legacy baseline cases remain valid

### Step 6: Run the validation suite

Run the existing baseline validation and the new memory/affect state tests together.

If the baseline fails, the integration is rejected and must be corrected before proceeding.

## Validation contract

Before implementation is considered complete, these tests must pass:

1. `testBaselineEncodingCompatibility`
   - ensure old baseline scenarios still encode identically

2. `testMemoryBiasChangesState`
   - same baseline state, higher memory bias => different state key

3. `testAffectTrendChangesState`
   - same baseline state, different affect trend => different state key

4. `testBalancedContextStable`
   - neutral memory and neutral affect produce the same stable neutral bucket

5. `testBaselineStillPasses`
   - existing Q-table validation remains green

## Expected behavior

This integration should produce a state that:

- remains stable under neutral memory and affect inputs
- changes when memory context becomes more relevant
- changes when emotional context becomes more positive or more negative
- keeps the state space bounded and reviewable
- does not alter the baseline learning policy yet

## Approval gate

Implementation should proceed only after all of the following are confirmed:

- memory bias meaning is accepted
- affect trend calculation is accepted
- the state extension is bounded and low-cardinality
- the validation tests are approved
- the Q-learning policy remains untouched for this stage

Once approved, implementation may begin in the state-action component and the corresponding validation tests.
