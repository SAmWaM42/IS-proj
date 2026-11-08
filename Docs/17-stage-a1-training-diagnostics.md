# Stage A.1 — Training and Behavioural Diagnostics Specification

**Status:** Ready for local-codebase investigation  
**Target branch:** `Dev` only  
**Repository:** `SAmWaM42/IS-proj`  
**Companion specification:** [18-stage-a1-training-refactor-experiments.md](18-stage-a1-training-refactor-experiments.md)

## 1. Purpose

Diagnose why the current Stage A training policy exhibits high Q-table support but limited local state-action coverage, rarely selects the delayed `INSIST` action, and continues repetitive action sequences despite the shared boredom/repetition penalty.

This is an investigation specification, not an instruction to tune until a preferred outcome appears. The local coding agent must inspect the current implementation and evidence before changing behaviour. It must distinguish confirmed defects from expected consequences of the current reward function or learned policy.

The research comparison remains between:
- **Baseline:** environment-observation Q-learning without the proposed semantic-memory and affective-state augmentation.
- **Proposed:** the same task and action space, with bounded memory- and affect-derived state.

Both must retain the same environment, action vocabulary, scenario stream, reward semantics, and evaluation protocol.

## 2. Current evidence to explain

The project owner supplied the following summary of the current validation artifact. Treat these as reported observations to reproduce and verify against the actual files; do not assume they are independently verified by this document.

1. Local coverage per state is approximately **17%**.
2. Q-table-supported decisions are **100%**, with zero fallback decisions.
3. Scenario coverage/distribution is reported as stable across scenario steps and stimulus types.
4. The environment exercises no-response progression `0 → 1 → 2`, but the policy often chooses `BARK` or `ACT_CONFUSED` instead of `INSIST`.
5. Repetition is high, including extended `FETCH` sequences; the boredom penalty may be absent, too small, or outweighed by positive reward.
6. Differences in total episode reward appear substantially associated with variable episode lengths (examples reported: 28 versus 48 steps), while per-step reward and stimulus-alignment rates remain around 90–100%.

Important interpretation constraints:
- Q-table support is **not** state-space coverage. Report both separately.
- A 17% local branching coverage is not automatically a defect; establish which valid actions/states are omitted and whether omissions matter.
- Eligibility for `INSIST` does not imply it should be selected. Determine whether the policy has learned a reason to select it.
- High stimulus alignment or synthetic reward does not establish believability.
- Total reward must not be compared without accounting for episode length and termination.
- Training reward is an engineering signal, not a participant believability measure.

## 3. Mandatory audit rules

Before edits:
1. Read `Docs/project-context.md`, `Docs/07-implementation-status-and-roadmap.md`, the relevant Q-learning, state, agent-comparison and Stage A specifications, plus this document and its companion.
2. Inspect the actual current source, tests, configuration, checkpoint format, and artifact-generation code. Do not rely on remembered class/function names.
3. Record the checked-out branch, working-tree state, current commit, compiler/build configuration, and available checkpoint/artifact paths.
4. Reproduce the reported metrics from the exact artifact and record its metadata (mode, seed, episode count, checkpoint provenance, step limit, scenario configuration).
5. Do not combine metrics from different runs merely because filenames are similar.
6. Preserve a clean, reproducible copy of the pre-experiment checkpoints and outputs. Never overwrite the only copy.
7. Do not modify the participant deployment branch or production deployment.

If the current artifact does not contain enough information to answer a question, identify the missing telemetry and add the smallest diagnostic-only instrumentation first.

## 4. Diagnostic work packages

### D1 — Q-learning update correctness

Inspect the complete update equation and implementation, including terminal handling and action validity.

Verify:
- The target for a non-terminal transition is equivalent to
  `r + gamma * max_a' Q(s', a')`, where the maximisation is over **valid actions available in the next state**.
- Terminal transitions use target `r` and do not bootstrap.
- Unvisited actions are handled consistently with the intended initialisation. In particular, an implicit zero for an unvisited action must not silently dominate all negative values if the implementation intends to maximise over learned valid actions.
- The current-state/action update uses the intended learning rate and discount factor exactly once.
- Invalid actions, empty next-action sets, unseen states, and malformed checkpoint entries are handled explicitly.
- Greedy selection, epsilon exploration, tie-breaking and action filtering agree with the same valid-action contract.
- Training and evaluation modes do not accidentally update Q-values or explore during evaluation.

**Required output:** equation as implemented, source locations, edge-case tests, and a clear pass/fail/unknown result for each item. If a defect is found, create a minimal failing test before proposing a fix.

### D2 — INSIST eligibility, value and execution path

Trace `INSIST` end-to-end:
1. no-response counter update and reset conditions;
2. observation/state construction and encoded state key;
3. valid-action filtering at counts 0, 1 and 2;
4. action selection and Q-value lookup;
5. action dispatch/execution;
6. environment response and reward;
7. next-state and terminal handling;
8. logging.

For each agent, report at least:
- number of encountered observations at no-response counts 0, 1, and 2+;
- number of decision opportunities where `INSIST` is valid;
- number and proportion of eligible decisions where it is selected;
- `INSIST` Q-value and competing valid-action Q-values at eligible decisions (distribution and representative state/action records);
- reward and subsequent response following `INSIST`, compared with alternatives under matched scenario/state conditions;
- whether state keys at count 0, 1 and 2 differ as intended.

Do not force selection as a diagnostic shortcut. Distinguish an eligibility bug, a representation/coverage issue, a low learned value, and a valid policy preference.

### D3 — Repetition penalty accounting and causal path

Trace the shared boredom/repetition mechanism from category classification through penalty calculation to the actual reward used in the Q update.

The Stage A rule was described as:
- same action category repeated without a meaningful response incurs `-0.04` per repeat;
- cumulative penalty is capped at `-0.20`;
- the first occurrence is unpenalised;
- meaningful response or category change resets the count;
- it applies to both agents.

Verify this against current code rather than assuming it is unchanged.

Report, per agent and action category:
- repeated-category opportunities;
- penalty trigger count;
- total and mean penalty;
- penalty cap frequency;
- reset count and reset reason;
- positive reward components on the same steps;
- final reward passed to Q-learning;
- repeated-run lengths (median, upper quantiles, maximum);
- transitions into/out of repeated loops.

Check whether a response is being classified as meaningful when it is merely a stimulus-alignment event, whether category mapping merges or splits actions unexpectedly, and whether the penalty is accidentally logged but not included in the actual learning reward.

Do not infer that a small penalty is defective simply because the agent repeats. Compare its magnitude with other reward terms and the learned action values.

### D4 — State representation and coverage

Clarify the definitions and denominators of every coverage metric.

Report separately:
- theoretical state count, where computable;
- states encountered;
- unique states per agent;
- valid state-action pairs available on encountered states;
- selected unique state-action pairs;
- selected/available ratio;
- Q-table entries visited and valid action entries visited;
- Q-table-supported decision rate;
- fallback decision rate;
- state-action visitation frequency distribution;
- states with one, few, and many valid actions.

Investigate whether memory and affect dimensions actually distinguish states in runtime. Test controlled changes to each relevant dimension while holding environment observations fixed. Determine whether no-response count, memory summary, affective state, and other inputs are represented in the state key and whether meaningful differences collapse into the same key.

For the proposed agent, provide a state-key collision/aliasing analysis: identify which combinations of raw bounded inputs map to the same encoded key, whether collisions are intentional discretisation, and whether they erase distinctions needed for the research hypotheses.

Do not increase state granularity without quantifying the resulting state-space growth and data sparsity.

### D5 — Scenario and episode-length fairness

Verify that both agents receive paired identical exogenous scenario inputs:
- scenario seed and composition;
- initial conditions;
- observation/stimulus stream;
- transition randomness;
- time intervals;
- maximum-step and termination rules.

Report episode length distribution, termination reasons, completion rate, and scenario composition by agent. Determine whether an agent's own actions can legitimately alter episode length, or whether episode lengths differ due to a runner/logging defect.

For comparisons, provide:
- total episode reward;
- reward per actual step;
- reward per fixed opportunity / normalised horizon where meaningful;
- completion and termination rates;
- stimulus alignment with an explicit denominator;
- paired per-scenario differences.

Never compare total rewards from different horizons as if they were directly equivalent. Do not silently discard early terminations or incomplete episodes.

### D6 — Training/evaluation provenance and checkpoint integrity

Establish exactly which checkpoint was trained, loaded, or evaluated for each result. Verify:
- fresh training versus resume semantics;
- baseline/proposed checkpoint separation;
- checkpoint schema/version and state-key compatibility;
- evaluation exploration is zero;
- evaluation learning is disabled;
- evaluation does not mutate checkpoints;
- random seeds are recorded and all relevant random generators are seeded;
- output directory and summary metadata identify the run unambiguously.

Because Stage A added `no_response_count` to the encoded state, do not resume a checkpoint created with the old state encoding unless a deliberate migration is specified and tested. Prefer fresh checkpoints for the current experiment.

### D7 — Reproducibility and seed sensitivity

Run a small, declared multi-seed diagnostic set using identical paired seeds for the two agents. Start with at least 5 seeds if runtime permits; report actual seeds and failures. Keep training and validation seeds disjoint. Do not select seeds based on favourable results.

Report means, standard deviations and per-seed values for key metrics. These are engineering diagnostics, not inferential evidence of participant-perceived believability.

## 5. Required diagnostic artifacts

Produce a compact, machine-readable run summary (JSON) and a human-readable Markdown report containing:

1. Git commit, branch, build type, command line, seed(s), run ID, timestamp.
2. Checkpoint paths, hashes/schema versions and initialization provenance.
3. Agent-by-agent episode length, termination, total reward and per-step reward.
4. Scenario composition and paired-input parity checks.
5. State/valid-action/selected-pair coverage metrics with definitions.
6. Q-table support and fallback rates, separately.
7. INSIST eligibility, selection, Q-value and outcome metrics.
8. Repetition penalty accounting and run-length metrics.
9. Stimulus alignment with denominators and per-stimulus breakdown.
10. Tests executed, exit codes, and any deviations from the specification.
11. Defects, likely explanations, confidence level, and missing evidence.

Store new diagnostic output under a new uniquely named directory, e.g. `artifacts/training/stage-a1-diagnostics/<run-id>/`. Do not overwrite existing research artifacts or commit large generated logs unless the project owner explicitly requests it. Commit compact reports/configuration only when useful.

## 6. Acceptance criteria

The diagnostic phase is complete when:
- the current validation summary has been independently reproduced or the reproduction gap is documented;
- D1–D6 have evidence-based results (pass/fail/unknown); D7 is run if feasible;
- INSIST's eligibility-to-selection path is fully traced;
- repetition penalties are accounted for from trigger through Q update;
- coverage definitions and denominators are unambiguous;
- reward/episode-length effects are separated;
- checkpoint provenance and evaluation mode are verified;
- no deployment or participant-client branch has been modified;
- a ranked list of confirmed defects, likely issues, and non-defects is delivered.

**Do not start broad training refactors until this diagnostic report exists.** Diagnostic-only instrumentation and minimal tests to expose a defect are allowed. Any fix should be isolated, tested and recorded before the next experiment.
