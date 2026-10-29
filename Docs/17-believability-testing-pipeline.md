# Believability Testing Pipeline Specification

**Document type:** Research evaluation and implementation specification  
**Status:** Draft for review before implementation  
**Target branch:** test/believability-evaluation-modelling  
**Authority:** Subordinate to Docs/project-context.md; complements Docs/15-proposed-agent-comparison-spec.md and Docs/16-baseline-agent-comparison-spec.md.  
**Purpose:** Define a reproducible pipeline for checking that the baseline/proposed conditions are comparable, collecting evidence about believability, and analysing behavioural consistency and adaptability.

---

## 1. Purpose and research questions

The project investigates whether persistent semantic memory and affective-state modelling change the perceived believability of a simulated companion agent.

The pipeline must support these research questions:

- **RQ1 — Memory:** Does the memory-and-affect condition receive different perceived-believability ratings from the baseline condition across repeated interactions?
- **RQ2 — Affect:** Do observed changes in the proposed agent's affective state correspond to systematic, observable changes in its behaviour?
- **RQ3 — Comparative behaviour:** Does the proposed condition show greater behavioural consistency and/or adaptation to relevant interaction history than the baseline, under the same scenario conditions?

This is an evaluation specification, not a claim that the proposed condition will perform better. Null, mixed, or negative results must remain reportable.

### 1.1 Conditions

- **Baseline condition:** Uses the baseline environmental state and learning/policy path. It must not receive the proposed condition's semantic-memory or affect-derived state inputs.
- **Proposed condition:** Uses the memory-and-affect-augmented state path defined by the current project specifications.
- Both conditions use the same simulation, action vocabulary, scenario definitions, reward semantics, and interaction opportunities.

The interface may label conditions A and B and vary which implementation is assigned to each label. The recorded data must preserve the actual condition mapping for analysis.

### 1.2 What “believability” means in this study

For this project, perceived believability means a participant's judgement that the simulated companion's behaviour appears plausible, coherent, and appropriately responsive for a companion animal within the limited simulation.

Believability is a **perceived outcome**, not proof that the agent has genuine emotions, consciousness, or animal-level cognition. The report must distinguish participant ratings from objective behaviour measurements.

---

## 2. Pipeline overview

The implementation should make each stage separately testable:

~~~text
Specification + Scenario Set
          |
          v
Trial Configuration / Run Manifest
          |
          v
Assign Condition and Scenario Order
          |
          v
Reset or Load Controlled Trial State
          |
          v
Participant Interaction / Scripted Event
          |
          v
Interaction Manager -> Observation -> Agent Decision
          |                              |
          |                              v
          |                       Action Execution
          |                              |
          v                              v
   Interaction/Event Log <------- Outcome / Reward
          |
          v
Session Completion + Participant Ratings
          |
          v
Data Validation and Exclusion Checks
          |
          v
Derived Behaviour Metrics + Rating Summaries
          |
          v
Paired/Within-Participant Comparison (if design supports it)
          |
          v
Results, Limitations, and Reproducible Export
~~~

The diagram is logical. Implementation must follow the current repository interfaces and must not introduce a second agent runtime or change policy behaviour merely to collect metrics.

---

## 3. Evaluation modes

The pipeline has two complementary modes. They must not be conflated.

### 3.1 Mode A — Automated behavioural verification

Purpose: verify implementation and measure observable behaviour without requiring participant judgements.

Use repeatable scenarios and controlled initial conditions. This mode checks the event pipeline, state transitions, action logs, memory persistence, affect updates, and derived metrics.

### 3.2 Mode B — Human perceived-believability evaluation

Purpose: collect participant ratings of the two conditions after equivalent interaction opportunities.

Participants should experience the same scenario set in both conditions where practical. Counterbalance the condition order across participants (for example, some experience baseline first and others proposed first). Do not expose implementation details or the hypothesis before rating unless the approved study protocol requires it.

If participants can identify conditions from the interface, record this as a limitation rather than claiming full blinding.

---

## 4. Experimental controls

The comparison is valid only if differences between conditions are attributable as far as practical to the intended memory/affect treatment rather than unrelated setup differences.

For a matched scenario, hold constant:

- scene geometry and initial object positions;
- initial agent and user positions;
- available actions and input wording;
- food/ball availability and other relevant environment flags;
- scenario duration or number of interaction opportunities;
- reward semantics and the environment's response rules;
- evaluation order rules, with condition order counterbalanced where possible;
- logging schema and timestamp conventions.

Record the following for every run:

- run/session identifier;
- scenario identifier and scenario version;
- actual condition (baseline or proposed);
- UI label (A/B), if used;
- condition order and scenario order;
- software revision/commit;
- start/end timestamps;
- whether policy updates were enabled;
- reset/load status and any setup deviations.

### 4.1 Learning during evaluation

For the primary participant evaluation, policy updates should be disabled unless the approved protocol explicitly studies online learning. This avoids a participant's earlier actions changing the policy in a way that makes later trials incomparable.

The existing UI manifest has used the field policy_updates_enabled: false. Keep the manifest truthful and verify the actual runtime behaviour rather than treating the field alone as proof.

If the project later decides to evaluate online learning, treat that as a separate protocol with explicit initialisation and update rules.

### 4.2 Memory and persistence controls

For each scenario, specify whether it starts with:
1. a clean memory state;
2. a standardised preloaded memory state; or
3. memory accumulated during earlier interactions in the same trial sequence.

Do not mix these modes without recording them. For a repeated-interaction memory test, start from a defined initial state, introduce the same relevant fact/event, and then test recall or behaviour in a later interaction. If persistence across process restart is being tested, explicitly record the restart boundary.

The baseline must not be given hidden access to the proposed agent's memory records. Shared environmental history may be logged for analysis but must not leak into the baseline's decision state.

---

## 5. Scenario suite

The first implementation should support a small, deterministic scenario suite rather than an open-ended benchmark. Exact scenario wording and coordinates should be versioned in a scenario file or test fixture.

| Scenario ID | Purpose | Procedure | Evidence to capture |
|---|---|---|---|
| BEL-01 | Initial response / baseline sanity | Reset both conditions to the same initial state; issue the same greeting or social interaction. | Input, selected action, observation, condition, response/action timing. |
| BEL-02 | Repeated interaction consistency | Repeat the same interaction under the same relevant state, with no intervening event expected to change the response context. | Action sequence and state snapshot for each repetition. |
| BEL-03 | Memory retention/use | Present a fact or preference that the proposed memory system can represent; later create a defined opportunity to use it. | Fact/event created, retrieval result or memory signal, later action, memory confidence/context where available. |
| BEL-04 | Context-sensitive adaptation | Present a relevant event, then a matched later interaction where the event should influence the proposed state. Compare with the same scenario without the event. | Pre/post state, memory/affect features, action, reward, scenario variant. |
| BEL-05 | Affective response | Trigger a controlled positive or negative event; observe the relevant affective variables and subsequent action selection. | Affect values before/after, event, action, state key and reward. |
| BEL-06 | Spatial proximity sanity | Repeat an interaction at close, medium, and far measured distances to its relevant target. | Agent/target coordinates, Euclidean distance, bucket, selected action. |
| BEL-07 | Restart continuity (technical) | Save a proposed-agent state, restart, reload, and repeat a memory-relevant interaction. | Save/load outcome, memory/affect snapshot, resulting state/action. |

Not every scenario needs to be shown to participants. BEL-06 and BEL-07 are primarily technical verification scenarios unless later justified as part of the human study.

A scenario must state its expected invariant or directional expectation before running. Do not define success retrospectively based on the observed result.

---

## 6. Measures and operational definitions

Collect objective behaviour metrics separately from subjective ratings.

### 6.1 Primary outcome: perceived believability

After each condition's interaction block, ask participants to rate a small set of statements on a consistent Likert-type scale. A 1–5 scale is a practical initial proposal (1 = strongly disagree; 5 = strongly agree), but the scale and wording must be reviewed and fixed before data collection.

Candidate items for review:

1. “The companion's behaviour seemed plausible for the situation.”
2. “The companion's responses were coherent across the interactions.”
3. “The companion appeared to respond appropriately to what had happened earlier.”
4. “The companion's behaviour felt consistent rather than arbitrary.”
5. “The companion's behaviour gave the impression of an individual with a continuing interaction history.”

Use the same items and order rules for both conditions. These are **project-specific draft items**, not a claim to use a validated psychometric instrument. Keep item-level responses so internal consistency and item patterns can be inspected; do not present a custom scale as validated without evidence.

Predefine the primary score before analysing data. A reasonable candidate is the mean of the completed believability items for a condition, provided the questionnaire and missing-item rule are fixed in advance. Report item-level results as well.

Optional secondary ratings, if the study needs them:
- perceived consistency;
- perceived responsiveness/adaptation;
- overall preference.

Avoid adding many overlapping scales that make a small study difficult to interpret.

### 6.2 Behavioural consistency

Consistency should not mean “always performs the same action.” A believable companion may act differently when its state or context changes.

Operational measure:
- identify scenario pairs or repetitions with equivalent relevant context;
- compare the selected actions or action categories under those matched conditions;
- report the proportion of matched opportunities where the action is the same, and/or the distribution of action changes;
- separately flag cases where state or context differed, so legitimate adaptation is not counted as inconsistency.

Do not compute a single consistency score across unrelated scenarios without documenting the matching rule.

### 6.3 Adaptability

Adaptability is measured as a change in behaviour after a defined relevant event, compared with a matched no-event condition or baseline trajectory.

For each adaptation scenario, report:
- whether the intended memory/affect signal was present and bounded;
- whether the encoded state changed when expected;
- whether action selection changed;
- whether the change was directionally consistent with the scenario hypothesis;
- whether the effect persisted into the later interaction, when persistence is being tested.

A changed action alone does not prove successful adaptation. Interpret it alongside state/memory/affect evidence and the scenario definition.

### 6.4 Supporting technical metrics

Where available and reliable, record:
- action-selection result;
- reward/penalty and cumulative reward (supporting diagnostic only, not a substitute for believability);
- encoded state key;
- memory retrieval summary/selected record identifiers;
- affective-state snapshot;
- measured target distance and proximity bucket;
- interaction latency and NLP failure status;
- save/load status;
- invalid or unclassified input count.

Reward should not be treated as the primary believability measure unless the reward design has been explicitly validated as a proxy for believability.

---

## 7. Event logging contract

The pipeline should reuse the current JSONL interaction/event logging approach where possible. Extend the existing schema additively; do not silently change existing field meanings.

Each trial must be traceable from manifest to events to participant ratings.

### 7.1 Run manifest

Minimum logical fields:

~~~json
{
  "schema_version": 1,
  "run_id": "unique-run-id",
  "protocol_id": "believability-v1",
  "scenario_id": "BEL-03",
  "condition": "proposed",
  "ui_condition_label": "A",
  "software_revision": "git-commit-sha",
  "started_at": "UTC timestamp",
  "policy_updates_enabled": false,
  "memory_initialisation": "clean",
  "scenario_version": 1
}
~~~

This is an illustrative contract, not a requirement to copy the sample values literally. Use valid timestamps and actual runtime values. Add participant IDs only as pseudonymous study codes; do not place names, emails, or other direct identifiers in analysis logs.

### 7.2 Per-interaction event

Retain existing event fields where present, including schema version, run ID, interaction ID/sequence index, timestamp, condition code, input summary, processing status, action/response summary, and learning-update status.

Add or standardise the following fields if they can be obtained without intrusive changes:

- scenario_id;
- condition (baseline / proposed);
- observation_before (bounded, serialisable values needed to interpret the decision);
- state_key_before;
- action_selected;
- reward or reward components;
- observation_after / state_key_after, when available;
- memory_signal_summary and affect_snapshot for the proposed condition only;
- target_basis, measured distance, and proximity bucket for distance-dependent scenarios;
- event_tags to identify scenario events;
- error_code or processing-failure reason where applicable.

Do not log full private free-text unnecessarily. For participant study data, store only what is needed for the research question and approved consent protocol.

### 7.3 Rating record

Store one row/JSON object per participant-condition rating block with:
- pseudonymous participant code;
- run/session ID;
- condition and condition order;
- scenario block/version;
- item identifiers and raw item responses;
- completion status and missing-item markers;
- optional free-text comment, only if approved by the protocol.

Keep raw responses immutable. Derived scores should be produced in an analysis step, not overwrite the raw records.

### 7.4 Integrity checks

The pipeline must detect or flag:
- duplicate run or interaction IDs;
- missing manifest for an event log;
- events with unknown condition/scenario;
- invalid or out-of-range rating values;
- missing condition mapping for A/B labels;
- timestamps that cannot be parsed or are inconsistent;
- incomplete or failed save/load operations;
- events written after a run is marked complete.

A flagged record should not automatically be deleted. Export a validation report stating the issue and the predeclared handling rule.

---

## 8. Analysis plan

Before participant data collection, write down the primary comparison, exclusion rules, and handling of missing ratings.

### 8.1 Descriptive analysis first

Report:
- number of participants and completed sessions;
- number of valid trials per condition;
- missing or excluded records with reasons;
- item-level and condition-level believability summaries;
- consistency/adaptability metrics per scenario;
- implementation errors and failed runs.

Use distributions and uncertainty summaries appropriate to the sample size. Avoid claiming broad generalisation from a small convenience sample.

### 8.2 Condition comparison

If the same participants experience both conditions, use a paired/within-participant comparison for the primary believability score, provided each participant has valid data for both conditions. Report the difference, its uncertainty, and the underlying distributions. Select a statistical test only after considering sample size, score properties, and the study design.

If participants experience only one condition, use an analysis appropriate to an independent-groups design and record that the design has changed.

Do not treat multiple interaction events from one participant as independent participants. Account for the repeated-measures structure in any inferential analysis.

### 8.3 Negative and inconclusive findings

Report findings that contradict the hypothesis, no detectable difference, and technical failures that limit interpretation. Distinguish:
- no observed difference;
- insufficient data or precision;
- failed implementation/manipulation (for example, memory was not retrieved);
- inconclusive measurement.

Do not claim memory or affect caused a rating difference if the comparison also changed other system components.

---

## 9. Acceptance criteria

The pipeline is ready for a pilot only when all applicable criteria below pass.

### A. Trial configuration and controls
- [ ] Each run produces a unique run ID and a manifest.
- [ ] The manifest records the true condition, scenario, software revision, policy-update setting, and memory initialisation.
- [ ] Baseline and proposed runs can be reset to equivalent environmental conditions.
- [ ] Condition A/B labels can be mapped unambiguously to actual implementations.
- [ ] Policy-update configuration is verified in runtime code, not only in the manifest.

### B. Event logging and data integrity
- [ ] Every interaction event can be traced to a run and scenario.
- [ ] The pipeline preserves existing event fields and uses a documented schema version.
- [ ] Invalid/unclassified inputs and processing failures are represented explicitly.
- [ ] Missing, duplicated, malformed, or mismatched records are reported by a validation step.
- [ ] Raw rating records are kept separate from derived scores.

### C. Behavioural verification
- [ ] Matched initial conditions produce reproducible observations for deterministic test scenarios.
- [ ] BEL-03 can demonstrate whether a memory-relevant event is stored/retrieved and whether the later decision path receives the resulting signal.
- [ ] BEL-05 records affect values before and after the trigger event.
- [ ] BEL-06 verifies that measured distance and proximity bucket correspond to the selected target.
- [ ] BEL-07 confirms whether memory and affect state survive a restart according to the declared persistence contract.
- [ ] Consistency and adaptability metrics can be calculated from the exported data with documented rules.

### D. Human evaluation readiness
- [ ] The questionnaire wording, scale anchors, primary score, missing-item rule, and condition order procedure are fixed before recruitment.
- [ ] Participants receive the same instructions and comparable interaction opportunities across conditions.
- [ ] The pilot confirms that instructions, interactions, and ratings can be completed without researcher intervention that differs by condition.
- [ ] Consent, data retention, pseudonymisation, and any institutional/departmental approval requirements are addressed before collecting participant data.

### E. Reproducible analysis
- [ ] A single documented command or script validates/export logs into analysis-ready tables, or the manual process is documented.
- [ ] The analysis export includes a data dictionary and scenario/condition mapping.
- [ ] A report can be generated that includes missing/excluded records and reasons.
- [ ] Results can be regenerated from preserved raw logs and the recorded software revision.

---

## 10. Proposed implementation order

Implement in small increments. Do not build the complete human-study system before the technical logging path is reliable.

1. **Audit existing logs:** map current manifest and JSONL fields to this contract; list gaps without changing code.
2. **Scenario fixtures:** define versioned deterministic test scenarios and reset conditions.
3. **Automated checks:** implement BEL-01, BEL-03, BEL-05, BEL-06, and BEL-07 checks that can be run without participants.
4. **Event validation/export:** validate logs and produce analysis-ready interaction tables plus a validation report.
5. **Behaviour metrics:** calculate consistency/adaptability from explicitly matched scenarios.
6. **Questionnaire capture:** add or define a separate rating form/storage path after the technical path is validated.
7. **Pilot:** run a small usability/technical pilot, resolve protocol issues, then freeze the protocol before the main data collection.

Each increment must have tests or reproducible steps and must not alter action selection or reward semantics solely to make a metric look better.

---

## 11. Non-goals and constraints

This specification does not:
- assert that the proposed agent is more believable;
- treat reward or action diversity as a direct proxy for believability;
- require a new machine-learning model or a new UI framework;
- require online learning during the participant study;
- prescribe a final sample size or claim a questionnaire is already validated;
- replace the existing agent comparison specifications;
- authorise collecting identifiable personal data without an appropriate study protocol.

---

## 12. Open decisions for review before implementation

Resolve these decisions before implementing the participant-facing portion:

1. **Trial design:** will every participant experience both conditions, or will conditions be between participants? Recommended initial design: within-participant with counterbalanced order, if practical.
2. **Memory initialisation:** clean memory per condition, standardised preload, or accumulated interaction history? This depends on whether the trial is measuring immediate plausibility or long-term continuity.
3. **Primary believability score:** approve/revise the draft items and decide whether to use their mean as the primary outcome.
4. **Scenario source of truth:** should scenarios be represented as JSON fixtures, C++ test definitions, or a documented table initially?
5. **Evaluation updates:** confirm policy updates are disabled for participant evaluation.
6. **Data handling:** confirm consent, pseudonymous IDs, storage location, access, and retention requirements with the relevant academic supervisor/department.

Until these decisions are approved, the document should be treated as a reviewable draft, not an instruction to implement every proposed field or questionnaire item unchanged.
