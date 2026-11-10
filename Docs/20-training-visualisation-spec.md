# Training Visualisation and Decision Diagnostics

**Status:** Proposed implementation specification for review on `Dev`  
**Scope:** Research/training harness and offline analysis only  
**Purpose:** Make the baseline/proposed training comparison inspectable through a self-contained visual report, including the behavioural mechanisms behind aggregate differences.

## 1. Motivation

The current training run already writes per-episode summaries, step logs for a limited number of episodes, and targeted diagnostic events. This is not sufficient for inspecting all training decisions or tracing why the proposed agent differs from the baseline.

The recent synthetic-user run showed shorter proposed-agent episodes, action-distribution changes (especially the near-absence of `APPROACH_FOLLOW`), lower state-action coverage ratios, and different INSIST selection rates. The summaries alone cannot establish the causes. In particular, energy depletion is plausible but must be recorded explicitly, and memory/affect must be shown to influence decisions rather than merely exist.

This increment adds observability and visualisation only. It must not change behaviour, rewards, Q-learning, action validity, state encoding, synthetic-user probabilities, or participant interactions.

## 2. Deliverable

Each training, validation, or comparison run must be visualisable as a **single self-contained HTML dashboard** generated from the run's own artifacts.

Proposed command:

```bash
python3 scripts/visualize_training.py \
  --input ~/AnimalAgentStudyData/synthetic-user-42 \
  --output ~/AnimalAgentStudyData/synthetic-user-42/training_dashboard.html
```

Requirements:
- Use Python standard library only to generate the report; no network access, CDN, or runtime package installation.
- Embed data and chart code in the HTML so the report works offline in a browser.
- Do not modify or overwrite source run artifacts.
- Accept a training-only directory, validation-only directory, or directory containing both.
- Show which inputs were missing and which charts were therefore unavailable; never fabricate or silently substitute values.
- Keep baseline and proposed comparisons side by side, with clear labels and denominators.
- Include a per-episode selector/table and a per-episode decision trace view.
- Support a user-selected episode and agent to inspect its timeline.

The C++ training runner should print the generated report command/path, or invoke the report generator only through a documented, optional post-run step. Do not make Python plotting packages a runtime dependency of the C++ executable.

## 3. Required data collection

### 3.1 Per-episode records

Retain and expose, for each agent and episode:
- episode index, scenario seed, user-model name and synthetic-user profile;
- requested scenario length, actual decision count, completion flag;
- explicit terminal reason (`energy_depleted`, `step_limit`, or another named reason);
- initial and final energy, stress, and user satisfaction where available;
- total reward and reward per actual decision;
- action counts and action availability counts;
- state count, selected state-action pairs, available state-action pairs, coverage ratio;
- Q-table-supported decisions and fallback decisions;
- request counts, acknowledgements, pending requests, and positive/negative/silent responses;
- INSIST predicate eligibility, valid-action availability, policy selection, final selection, execution, and response;
- repetition penalties, reset reasons, and maximum consecutive category count.

Do not infer energy depletion solely from `steps < scenario_steps`. Record the actual terminal reason at the point the episode ends.

### 3.2 Per-decision trace

Write a compact JSONL record for **every decision in every episode**, for both agents. This is separate from the existing verbose tail-only step logs. Include:
- mode, agent, episode, scenario seed, step, timestamp or elapsed simulated time;
- observation fields: goal, proximity, user status, task status, no-response count, signal strength, food/play availability, energy;
- valid actions and selected action;
- policy action and final action where overrides exist;
- action-selection source: learned Q choice, exploration, contextual fallback, or override, where this can be determined accurately;
- Q-values for valid actions, distinguishing stored values from default/unvisited values;
- reward before and after shared repetition penalty, terminal flag and terminal reason;
- energy, stress, and user-satisfaction before and after the transition;
- synthetic-user response status/category, request pending/acknowledged state, and no-response count before/after;
- repetition category/count/penalty and reset reason;
- Q-table state presence/support and encoded current/next state.

### 3.3 Proposed-agent memory and affect trace

For proposed-agent decisions, include:
- attachment, stress, energy, and curiosity values at decision time;
- bounded affect trend/state values actually passed to the state builder;
- memory record count;
- the bounded memory summary/signals actually passed into state construction;
- retrieved/relevant memory count and, where feasible, stable record identifiers plus retrieval score/relevance/confidence/scope;
- whether the encoded state changed when memory or affect inputs changed;
- whether a memory/affect-related policy override occurred.

Do not dump private participant data. Training diagnostics should use synthetic/test records only. Avoid copying full free-text memory content into logs; use record identifiers and structured, non-identifying metadata.

For the baseline, emit the same trace schema with memory/affect fields explicitly null or not-applicable. Do not add memory or affect to baseline decision-making.

Important: distinguish **memory available**, **memory retrieved**, **memory encoded**, and **memory causally influential**. A non-zero memory count alone is not evidence that memory changed a decision. If causal influence cannot be established from current architecture, expose the observable stages and label causal influence as unknown rather than claiming it.

## 4. Dashboard sections and charts

### A. Run overview
- Run metadata: mode, branch/commit, build type, seed, user model, episodes, steps, checkpoint initialization and hashes when available.
- Baseline/proposed KPI comparison: mean reward per step, total reward per episode, mean decisions, completion rate, termination counts, and stimulus-alignment rate.
- Clearly label synthetic reward and stimulus alignment as engineering diagnostics, **not believability**.

### B. Episode health and energy
- Per-episode decision count and scenario length (paired plot).
- Episode-length distribution by agent.
- Completion and terminal-reason counts/rates.
- Final-energy distribution and initial-to-final energy change.
- Energy trajectory over decision index for selected episodes, baseline and proposed.
- Reward per step and total episode reward shown separately; do not hide horizon effects by normalising away early termination.

### C. Action selection
- Action-frequency comparison (counts and percentages).
- Action availability versus action selection for each action.
- Per-episode action mix over time.
- Selected action versus contextual stimulus/request category.
- Explicitly highlight actions that are available but rarely selected (e.g. `APPROACH_FOLLOW`) without presuming that low selection is a defect.

### D. Learning and state/action coverage
- Per-episode and cumulative reward curves.
- Unique states, unique selected state-action pairs, available pairs, and coverage ratio.
- Q-table-supported versus fallback decision rate.
- Distribution of state-action visit counts (log scale if needed).
- Valid-action Q-value spread and selected-action Q-value versus best alternative, with unvisited/default values distinguished.
- Epsilon/exploration schedule if it changes during the run; otherwise state that it was fixed.

### E. User interaction
- Positive, negative, and silent response rates with denominators.
- Requests observed, acknowledged, completed, and pending at episode end.
- Acknowledgement rate by action/context.
- No-response streak distribution.
- INSIST eligibility → action availability → policy selection → final selection → execution → response funnel.
- Repetition penalty amount, trigger frequency, reset reasons, and repeated-category run lengths.

### F. Memory and affect
- Proposed-agent affect trajectories (attachment, stress, energy, curiosity) over decision index and selected episodes.
- Memory count and retrieved/relevant record count over time.
- Memory summary/state signal over time.
- Compare decisions with/without memory retrieval and affect-state changes where the trace supports it.
- Display policy action, final action, Q-values, and override reason together to reveal whether a memory/preference override altered the final action.
- Do not label correlation as causal influence. If the system does not currently log a required value, show an explicit instrumentation gap.

### G. Episode inspector
- Select agent and episode.
- Show the complete decision timeline with observation, valid actions, Q-values, selected action, reward components, energy, affect, memory signals, user response, and terminal reason.
- Allow filtering to decisions where INSIST was eligible, memory was retrieved, affect changed, an override occurred, or energy fell below a configurable threshold.
- Show paired episode identifiers and scenario seeds to support comparison.

## 5. Metric definitions and integrity

- Reward per step = total episode reward / actual decisions; retain total reward separately.
- Completion rate = completed episodes / total episodes.
- Termination reason rates use total episodes as denominator.
- Action selection percentage = selections of action / all decisions. Availability rate = decisions where action was valid / all decisions. Selection-given-availability = selections / available opportunities.
- Coverage ratio = unique selected state-action pairs / unique valid state-action pairs available on encountered states. Display numerator and denominator.
- Q-table support must distinguish state present, at least one stored valid action value, and selected action having a stored value.
- User response rates must distinguish responses per decision from acknowledgements per request.
- INSIST funnel stages are not interchangeable.
- Memory/affect plots must identify whether values are raw internal values, bounded summaries, or encoded buckets.
- Missing data is null/unknown, not zero.
- Do not compare training and validation as if they were the same phase. Label checkpoint provenance and evaluation learning/exploration settings.

## 6. Implementation boundaries

- Development branch: `Dev` only.
- No edits to `deployment/participant-client`, `IS-proj-web`, Vercel, or production.
- No changes to participant UI, action vocabulary, environment dynamics, reward weights, Q-learning, memory ranking, affect updates, or synthetic-user parameters.
- Prefer a compact JSONL trace and a standalone visualisation script over adding a charting dependency to the C++ executable.
- Keep existing summary/step/event artifacts backward-compatible; add a schema version to new trace output.
- Do not commit generated run data or HTML reports unless explicitly requested.

## 7. Tests and acceptance criteria

Add tests for:
1. Trace records are emitted for every executed decision for both agents.
2. Each record parses as JSON and includes the required common fields.
3. Proposed affect/memory fields are populated from the actual decision trace; baseline fields are explicitly null/not-applicable.
4. Terminal reason and final energy match the actual episode transition.
5. Metric denominators and null handling are correct.
6. The visualiser works with train-only, validate-only, and combined artifacts.
7. Missing optional artifacts produce visible warnings, not crashes or fabricated charts.
8. HTML output is self-contained and contains no external script/style/image URLs.
9. A small fixture produces expected chart data and episode filtering.
10. Existing self-tests and CTest remain unchanged and pass.

Acceptance:
- A normal training/validation run can be visualised without manually editing JSON.
- All required sections are present or explicitly marked unavailable.
- The user can inspect the exact episodes and decisions behind aggregate differences.
- Memory/affect values are traced through retrieval, state construction, and action selection where the code exposes them.
- No behavioural or participant-facing changes occur.
- Report exact changed files, tests, commands, and any instrumentation gaps.
