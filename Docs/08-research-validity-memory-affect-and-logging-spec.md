# Research Validity and Behavioural Correctness Specification

**Status:** Proposed implementation specification  
**Target branch:** `Dev`  
**Scope:** Internal-state updates, memory retention, affect-driven adaptation, paired baseline/proposed evaluation, and auditable event logs  
**Research question:** How do persistent memory and affective state influence the perceived believability of a simulated companion agent?

## 1. Problem statement

Recent interactive runs suggest two defects that must be investigated before participant evaluation:

1. Interactions do not appear to change the proposed agent's internal state in an observable or reliable way.
2. The baseline agent appears to select more repeatable and contextually appropriate actions than the proposed agent.

These observations are symptoms, not yet confirmed root causes. Possible causes include the interaction path not applying outcomes to the intended agent, state being updated but not used in action selection, state encoding collisions or excessive discretisation, rewards that favour generic actions, stale state passed to Q-learning, incorrect terminal/update sequencing, different inputs or learning conditions between agents, and a UI that displays an action different from the action actually selected.

The next implementation must diagnose these paths with tests and structured traces rather than trying to fix them by tuning reward constants alone.

## 2. Goals and non-goals

### Goals

- Ensure each accepted interaction is processed exactly once and in FIFO order.
- Ensure both agents receive equivalent scenario inputs and feedback, while retaining their intentionally different internal representations.
- Make the proposed agent's affect and memory changes observable, persistent for the intended lifetime, and demonstrably relevant to action selection.
- Preserve a genuinely memory- and affect-free baseline.
- Add repeatable behavioural tests and event logs that can explain why an action was selected.
- Support a later controlled participant study without treating synthetic reward as a proxy for perceived believability.

### Non-goals

- Do not add a prioritisation scheduler; FIFO is sufficient.
- Do not replace tabular Q-learning with a neural model or LLM.
- Do not redesign the SFML interface or environment art in this stage.
- Do not claim that memory or affect improves believability until behavioural tests and participant data support that claim.
- Do not silently retrain or overwrite the policy used in a recorded evaluation.

## 3. Design principles

1. **One canonical interaction event:** UI input, NLP interpretation, scenario transition, agent decision, feedback, state update and log entry must share a stable event/interaction ID.
2. **One shared scenario, two policies:** Both agents observe the same external event and environment conditions. Their internal state differs only where the experimental treatment requires it.
3. **Explicit update order:** Do not let an action's reward, next state or memory update depend on accidental call order.
4. **Observable mechanisms:** Internal-state updates and memory retrieval must be inspectable in development logs and test output; participant-facing explanations are optional and must not bias ratings.
5. **No leakage:** Baseline action selection must not receive memory, affect, proposed-agent state or features derived from them.
6. **Separate behavioural validity from believability:** Correct recall and adaptation are mechanism checks; participant ratings are the outcome for perceived believability.
7. **Deterministic evaluation:** Fixed seeds, frozen policies and fixed scenarios must produce reproducible traces.

## 4. Target architecture and interaction lifecycle

The implementation should follow this logical sequence:

1. **Input capture:** Assign an `interaction_id`, timestamp/order index and raw input or scripted event.
2. **NLP interpretation:** Resolve the input into a structured intent and relevant entities. Record the interpretation method (model, rule or fallback), confidence if available, and any fallback reason.
3. **Shared environment transition:** Apply the external event once to a canonical scenario state. Produce a shared external observation for both agents.
4. **Agent decision:** Ask baseline and proposed agents to choose an action from that same external observation. The baseline receives environment-only features. The proposed agent additionally receives its own affect state and retrieved memory features.
5. **Action execution:** Execute/log each selected action against the same scenario rules. If the agents are shown in parallel, use paired or cloned scenario state so the first agent's action cannot alter the second agent's starting conditions.
6. **Feedback/reward:** Calculate outcome feedback through a shared reward/outcome function. Store raw user feedback separately from the training reward.
7. **Learning update:** Update each policy using its own pre-action state, chosen action, reward, next state and terminal flag. Do not recompute the pre-action state after mutating affect or memory.
8. **Proposed internal-state update:** Update affect and memory from the completed interaction/outcome according to the rules in Sections 6 and 7. The next decision must use the resulting state.
9. **Event record:** Write a complete structured record, including both agents' decisions and relevant proposed-state transitions.
10. **Queue progression:** Mark the interaction complete and process the next FIFO item. A queued interaction must not overwrite an in-progress one.

If current class boundaries make this sequence impossible to follow, introduce a small interaction coordinator rather than spreading sequencing logic across UI callbacks.

## 5. Internal-state update correctness

### 5.1 Trace the state lifecycle

For every proposed-agent interaction, capture the following values at the relevant points:

- affect before the event;
- memory candidates/retrieval result before decision;
- encoded state supplied to action selection;
- selected action and its Q-value or selection reason;
- external outcome and raw feedback;
- reward used for learning;
- affect after the event;
- memory records created or updated;
- next encoded state supplied to Q-learning.

Use the trace to determine where the current behaviour breaks. Do not assume the affect/memory update functions are at fault until the trace shows whether they are called, whether values change, and whether changed values reach the next decision.

### 5.2 Update ownership and order

- A single component must own the authoritative proposed-agent affect and memory state.
- UI code must not directly mutate these values.
- Each accepted interaction must cause no more than one state transition and no more than one memory write/update for each defined memory event.
- An interaction must not be applied twice because both a UI callback and an animation completion callback invoke the same update.
- Distinguish the **state used to choose the action** from the **next state after the outcome**.
- Ensure Q-learning receives the actual next state and terminal flag. Terminal transitions must not bootstrap from a nonexistent next action.
- Remove ambiguous uses of raw reward as a memory feature or state component. Reward, memory features and affect features must have distinct names and documented semantics.
- Verify that `reset()`, session boundaries and persistence operations follow an explicit lifecycle policy. Resetting a temporary session must not silently erase persistent memory or a learned policy unless the experiment explicitly requests it.

### 5.3 State encoding

- Document the meaning, range and discretisation of every state feature.
- Bound and normalise numeric values before encoding.
- Ensure a change in a meaningful affect or memory feature can change the encoded state where that feature is intended to influence behaviour.
- Add tests for distinct feature combinations and for unintended encoding collisions.
- Do not feed proposed-only features into the baseline's state encoder, reward, or action selection.
- Prefer named state structures and diagnostic output over opaque packed integers during debugging.

### 5.4 Acceptance criteria

- A scripted positive and negative interaction changes the expected affect dimensions according to documented rules.
- A memory-worthy statement creates or updates the intended memory record exactly once.
- The next decision sees the updated affect/memory representation.
- A no-op or irrelevant interaction does not create an arbitrary preference memory.
- A terminal transition updates Q-values without bootstrapping from a next state.
- Tests fail if the state update is skipped, applied twice, or not passed to the next decision.

## 6. Observable memory retention

### 6.1 Memory event rules

Define which event types are eligible for storage, for example:

- explicit user preference: `user prefers ball play`;
- explicit dislike or boundary: `user dislikes loud barking`;
- a repeated interaction pattern, only after a documented threshold;
- an observed outcome that is genuinely relevant to future behaviour.

Do not store every interaction as an equally reliable semantic fact. Keep source, confidence, user/general context, recency, and any generalisation value separate. A raw interaction log is not itself proof that semantic memory was retrieved.

Memory retrieval must return the records actually used by the decision, not just a count of all records. Log the record ID, retrieval score/reason and the features passed to action selection. Avoid storing unnecessary personal information in participant studies.

### 6.2 Controlled context-retention scenario

Add a deterministic test scenario with this structure:

1. The user explicitly establishes a preference (for example, prefers fetching a ball and dislikes loud barking).
2. Several unrelated interactions occur between the preference and the test.
3. The user later presents an opportunity to act on the preference without restating it.
4. The agent's response and the memory retrieval trace are recorded.
5. A matched baseline run receives the same external sequence but cannot access semantic memory.

Use multiple preference values and counterbalanced sequences to avoid a single hard-coded keyword demonstrating success. The test must verify the preference is retrieved and used in action selection, not merely present in storage.

### 6.3 Memory acceptance criteria

- The proposed agent recalls a previously stored preference after intervening events in a deterministic test.
- The event log identifies the retrieved record and shows that its feature reached the decision state.
- The baseline cannot retrieve or consume that record.
- A contradictory or updated preference follows a documented update/confidence policy rather than creating uncontrolled duplicates.
- Memory persistence across sessions is tested separately from within-session retention.
- Memory tests include negative cases where unrelated context should not affect action selection.

## 7. Affect-driven adaptation

### 7.1 Define operational rules

For every affect dimension currently used by the project (including attachment, stress, energy and curiosity where applicable), document:

- numeric range and initial value;
- which events change it and in which direction;
- update magnitude, decay and bounds;
- how it is encoded for action selection;
- expected observable behavioural effects;
- reset and persistence behaviour.

Avoid asserting human-like emotion. These are simulated affective state variables used to alter behaviour.

Affect updates must use the interaction outcome and defined event semantics, not arbitrary reward reuse. Keep reward calculation separate from affect transition rules so the same user feedback cannot accidentally be counted twice through two indistinguishable pathways.

### 7.2 Controlled adaptation scenario

Add a test where the same dog and environment receive a repeated sequence of positive feedback, followed by a separate negative-feedback sequence. Measure:

- affect values before and after each interaction;
- action distribution before and after feedback;
- whether relevant actions become more or less likely;
- whether the response is stable under a fixed seed;
- whether an unrelated affect dimension remains unchanged where the rules say it should.

Include a recovery/decay scenario so a single event does not permanently dominate behaviour unless the model explicitly specifies that outcome. Use multiple seeds or paired trials when measuring action distributions.

### 7.3 Affect acceptance criteria

- Expected affect transitions are unit-tested independently of Q-learning.
- The next decision receives the post-interaction affect state.
- At least one controlled scenario demonstrates a predictable, documented change in action selection attributable to affect.
- The effect disappears or changes as expected when the relevant affect feature is ablated.
- The baseline's behaviour and state are unchanged by proposed-only affect updates.

## 8. Baseline/proposed fairness and diagnosis of apparent baseline superiority

The baseline appearing more appropriate may reflect a real problem, or it may reflect different training conditions, state aliasing, reward incentives or UI presentation. Diagnose before changing the algorithm.

### 8.1 Shared experimental conditions

- Use the same external event sequence, initial environment conditions, available action vocabulary, action preconditions, transition rules and feedback function.
- Use paired scenario IDs and deterministic seeds.
- Ensure the first agent's action does not change the starting state for the second agent. Use cloned environments or replay the same recorded scenario.
- Keep UI sprite/animation quality and display duration as comparable as possible. Visual differences must not determine which policy is evaluated.
- Freeze policies during evaluation; training must occur in a separate mode and must not change the policy while a participant is interacting.
- Record policy version, training configuration, seed and scenario version.

### 8.2 Compare behaviour, not only total reward

For both agents, record:

- selected action and legal-action set;
- action-selection reason (greedy Q action, exploration, fallback or rule);
- Q-values for legal actions when available;
- scenario context and expected-action rubric;
- outcome, raw user feedback and training reward;
- action repetition rate and invalid/inapplicable action rate;
- context-appropriate response rate on labelled deterministic tests;
- state/action coverage and frequency of fallback behaviour.

Use a small labelled scenario suite to check whether each interaction has a contextually appropriate response. Avoid relying on a single hand-authored “correct” action when multiple actions are plausible; define an acceptable action set or rubric.

### 8.3 Training and Q-learning audit

- Confirm that the Q-table is loaded from the intended checkpoint and that training-v2 or later policies are not accidentally replaced by an older table.
- Confirm the intended exploration policy and that exploratory action selection is disabled during evaluation.
- Confirm reward and transition definitions reward context-appropriate behaviour rather than generic repetition.
- Confirm state encoding preserves distinctions needed to respond differently to relevant contexts.
- Confirm learning updates use the correct pre-state, action, reward, next state and terminal flag.
- Verify seeded runs are reproducible; avoid a global time-seeded RNG shared unpredictably by unrelated components.
- Track learning curves and hold out scenarios for validation. Do not tune and report results on the same scenarios only.

Do not make the proposed agent artificially win by adding a hand-coded response override unavailable to the baseline. Any deterministic rules should be shared when they express task/environment constraints rather than the experimental treatment.

## 9. Reliable event logging

### 9.1 Format and location

Write append-only JSON Lines (`.jsonl`) for each experiment/run, with one record per interaction and an explicit schema version. Keep logs outside source files and avoid committing participant-identifiable raw data to the repository. Synthetic development traces may be versioned if useful, but participant data should be handled under the study's data-management plan.

Each run should have a manifest containing:

- schema version, run ID and UTC start time;
- source commit/policy version;
- scenario suite and version;
- random seed;
- agent configuration and training checkpoint IDs;
- evaluation/training mode;
- environment and reward configuration;
- whether NLP used the model or a fallback.

### 9.2 Required interaction fields

At minimum, record:

- `schema_version`, `run_id`, `interaction_id`, `sequence_index`, timestamp;
- scenario ID and external-state snapshot or stable state summary;
- raw input only where necessary and permitted; otherwise a minimised/redacted representation;
- interpreted intent, entities, NLP method, fallback reason and confidence if available;
- queue time, processing start/end and processing status;
- baseline and proposed action, legal actions and selection method;
- each agent's pre-state and next-state summary;
- reward and outcome fields, kept distinct from raw user feedback;
- proposed affect before/after;
- memory record IDs considered/retrieved and memory records added/updated;
- terminal flag, Q-learning update status and policy/checkpoint identifier;
- errors, skipped updates and fallback reasons.

Use explicit nulls or omitted fields consistently. Do not log secret data, credentials or unnecessary participant-identifying information.

### 9.3 Logging guarantees

- Every accepted interaction has exactly one terminal status: completed, rejected, or failed.
- Queue order can be reconstructed and matches actual processing order.
- Both agents' actions and outcomes are recorded for the same scenario interaction.
- Logging failure is visible and must not silently produce a seemingly complete study run.
- A validator checks required fields, unique interaction IDs, sequence continuity, parseable JSON and complete baseline/proposed records.
- Provide a small script or test mode that summarises a run and flags missing transitions, repeated IDs, NLP fallbacks, absent memory traces and policy changes during evaluation.

## 10. Tests required before participant evaluation

### Unit tests

- affect transition rules and bounds;
- memory write/update/retrieval and contradiction handling;
- state encoding and feature separation;
- terminal Q-learning update;
- interaction coordinator's exactly-once processing;
- FIFO queue order and failure handling;
- log serialization and schema validation.

### Integration tests

- explicit preference → intervening events → preference-consistent response;
- positive/negative feedback → affect transition → measurable action-selection difference;
- same scenario replayed for both agents;
- baseline invariant under proposed-only memory/affect changes;
- persistent NLP request/response with multiple queued requests;
- reset, save/load and session lifecycle;
- fixed seed and frozen policy produce matching evaluation traces.

### Experiment-level checks

- evaluate both agents on the same held-out scenario set;
- report confidence intervals or uncertainty where sample sizes allow;
- separate synthetic-user results from human participant ratings;
- ensure the study questionnaire measures perceived believability, behavioural consistency and continuity without telling participants which agent is expected to perform better;
- preserve a policy/configuration snapshot for every recorded run.

## 11. Implementation order

### Stage A — Diagnose and instrument first

1. Inspect the current interaction path end-to-end.
2. Add structured decision/state traces and stable interaction IDs.
3. Verify the queue invokes processing exactly once and in FIFO order.
4. Audit action/reward/next-state/terminal update ordering.
5. Add a regression test showing baseline isolation from proposed-only state.

**Exit gate:** The team can explain, from one trace, exactly how an input became an action and whether state changed.

### Stage B — Correct state transitions

1. Make affect transition rules explicit and unit-tested.
2. Make memory write/update/retrieval rules explicit and unit-tested.
3. Fix state encoding and next-state handoff.
4. Add the context-retention and affect-adaptation scenarios.

**Exit gate:** Tests show state transitions are applied once and influence the next decision as specified.

### Stage C — Fair comparison and learning audit

1. Build a paired scenario runner using cloned/replayed external state.
2. Verify same action space, reward rules, seed controls and frozen evaluation policies.
3. Audit checkpoints, exploration and training/evaluation separation.
4. Report context-appropriate action rate, repeat rate, invalid-action rate and outcome metrics for both agents.

**Exit gate:** Apparent baseline superiority can be explained by evidence, and comparisons are reproducible.

### Stage D — Event log validation and study readiness

1. Finalise the JSONL schema and run manifest.
2. Add a log validator and summary report.
3. Run all integration scenarios and a short pilot.
4. Review data minimisation and study procedure before collecting participant data.

**Exit gate:** A complete run can be reconstructed and audited without relying on console output or memory.

## 12. Definition of done

This stage is complete only when:

- [ ] Proposed affect changes according to documented rules and reaches action selection.
- [ ] Relevant memory is stored, retrieved and demonstrably used after intervening interactions.
- [ ] Baseline action selection has no memory/affect leakage.
- [ ] Both agents receive paired external conditions and comparable feedback.
- [ ] Q-learning update order, next state and terminal handling are tested.
- [ ] Fixed-seed evaluation is reproducible with frozen policies.
- [ ] Logs contain both agents' actions, internal-state transition evidence and processing status.
- [ ] A validator detects incomplete or inconsistent logs.
- [ ] The study distinguishes mechanism tests from perceived-believability measurements.
- [ ] All tests and limitations are reported honestly; no unverified performance improvement is claimed.

## 13. Open decisions to settle during implementation

- Whether memory persists across UI sessions, experiment runs, or both.
- Whether the agents are run in parallel on cloned environments or in alternating replayed episodes. Cloned/replayed scenarios are preferred for fair comparison.
- Exact affect update coefficients, decay rates and memory confidence thresholds. These must be documented and tested rather than chosen implicitly in UI code.
- Whether participant runs permit any online learning. The recommended default is to freeze policies during the study and keep interaction outcomes for analysis, not policy updates.
- Final questionnaire instrument and study procedure, to be reviewed against the project ethics/data-management requirements.

These decisions should be recorded in the project documentation when resolved. Do not silently change the experimental treatment between runs.
