# Training and Evaluation Redesign Specification

**Branch:** `Dev`  
**Status:** Approved implementation plan; code changes are not yet implemented by this document.  
**Purpose:** Replace the current narrow synthetic training loop with a reproducible, action-dependent environment that can fairly evaluate a baseline agent against a memory-and-affect agent.

## 1. Executive summary

The current training runner is not sufficient evidence of generalisation or of the proposed architecture's value. It cycles through six scenario types, introduces only small binary variations, and chooses the next observation independently of the selected action. Its reward function is therefore an oracle over the current observation and action. In that setup, the baseline can learn the reward table without needing memory or affect, and the discount factor has little or no meaningful work to do.

The implementation should proceed in controlled stages: first repair the current reward/data contract and make its limits measurable; then add a stochastic user simulator and action-dependent world transitions; then add internal-drive reward only to the proposed condition; finally add coverage-based evaluation, exact baselines where mathematically valid, multi-seed reports and training hygiene.

Synthetic reward is a debugging and policy-learning measure, not a substitute for perceived-believability ratings from participants.

## 2. Current problems and required corrections

### 2.1 Tiny deterministic scenario space

**Current issue:** `makeObservation()` cycles through six cases using `stepIndex % 6`, with only a couple of binary flips. A validation seed changes those flips but does not create a genuinely different task distribution. Episodes whose length is not divisible by six also alter scenario frequency.

**Required change:**
- Replace fixed position-based cycling with scenario generation from a typed scenario catalogue and configurable distributions.
- Generate complete episodes from interaction sequences, with randomized order and variable length, while preserving a known distribution for reproducibility.
- Define explicit train, validation and test scenario generators. Hold out combinations of context, user traits, task availability, proximity, feedback history and environment state—not merely a different random seed.
- Include coverage targets and report counts for each scenario family and each important state/action pair.
- Keep rare but meaningful contexts (e.g. unavailable target, repeated ignored user, conflicting request, changed preference) represented deliberately rather than hoping random sampling discovers them.

**Acceptance:** The held-out suite contains scenario combinations absent from training, and reports their coverage. Train/validation/test seeds and scenario definitions are recorded. Non-multiple episode lengths do not systematically over-sample one scenario family.

### 2.2 Current loop is effectively a contextual bandit

**Current issue:** The next observation is selected independently of the action. Consequently, action choice does not change the world, and the discount factor γ cannot represent meaningful long-term consequences. With deterministic reward (r(s,a)), the exact optimum is (Q^*(s,a)=r(s,a)) for a one-step/terminal contextual bandit, or the appropriate state-action value for a genuine transition model.

**Required change:**
- Introduce an explicit environment transition contract, such as `step(state, action, user, rng) -> {next_state, feedback, reward_components, terminated, truncated}`.
- Make actions change relevant state. Examples: a successful fetch clears the active task and changes object location; an infeasible fetch leaves the task pending and may reduce simulated-user satisfaction; greeting can change user receptiveness; ignoring repeated bids for attention can increase irritation; rest can restore energy and reduce stress; a disturbance can change stress/arousal; playing can satisfy curiosity or increase fatigue.
- Separate true terminal states from time-limit truncation. At terminal states, do not bootstrap. At truncation, bootstrap from the final observation unless the environment contract explicitly defines the cutoff as terminal.
- Use γ only where future rewards depend on transitions. If a specific diagnostic remains a one-step bandit, label it as such and use it to test the learner, not as evidence of RL performance.
- Ensure both agents receive cloned initial worlds and the same exogenous random stream/user seed for paired comparisons. Their actions may produce different consequent states within their own clones.

**Acceptance:** A test demonstrates that changing the action can change the next environment state and later reward. A terminal transition and a truncated transition produce the expected, different Q update.

### 2.3 Reward must not be an oracle for the proposed architecture

**Current issue:** The reward only sees observation and action. It contains no user history or latent user preference, and no internal drive. Thus there is no legitimate benefit for memory to discover, and the baseline may be optimal under the reward.

**Required change:**
- Replace direct deterministic scoring with feedback generated by a stochastic simulated user whose hidden traits influence response probabilities.
- Define user archetypes as distributions, not absolute labels: e.g. playful, reserved/low-response, praise-responsive, noise-sensitive, and variable/neglectful. Avoid treating these as psychological facts; they are controllable synthetic profiles.
- Generate positive, neutral, negative, ignored, or delayed feedback probabilistically based on action appropriateness, current context, hidden preferences, recent interaction history and response noise.
- Keep latent user traits hidden from both policies. Only expose information available through interactions, observations and valid retrieved memory.
- Define a stateless baseline reward from external outcomes/feedback. The proposed agent may additionally receive a bounded internal-drive reward component (see §2.5). Keep components separately logged so any advantage is interpretable.
- Do not hand the proposed agent the hidden preference directly as a feature. Its advantage must come from learning/retrieving evidence gathered through interactions.
- Pre-register the reward weights for the final evaluation, or tune them only on training/validation data and freeze them before held-out testing.

**Acceptance:** Repeated identical conditions can yield different simulated-user feedback under a controlled seed/noise model. User-specific history can legitimately affect expected reward, and memory can in principle improve decisions for a particular user. Hidden profile variables never leak into observations or policy state.

### 2.4 Correct reward and scenario logic

**Current issues:**
- `fetch_far` can yield positive reward for FETCH despite the success metric requiring proximity ≤ 1.
- Branches keyed on `task_status` are unreachable for some `goal_state` values because the generator couples them.
- Many incorrect actions share flat penalties, giving poor action ranking.
- The documented interactions (feeding, playing, praising, scolding) do not match the trained scenario families (greeting, fetch, disturbance, rest).

**Required change:**
- Define typed enums for intent, task state, proximity, user feedback and action. Avoid fragile magic integers and positional aggregate initialization. Use named builders or designated factory functions for observations.
- Make reward and success metrics use one canonical transition outcome. A far-but-reachable fetch may succeed only when the environment's sampled attempt succeeds; do not infer success solely from task status or proximity. Reserve hard masking for physically impossible conditions such as an explicitly unavailable target. Distance and energy should modulate effort and success probability rather than acting as arbitrary policy gates.
- Remove unreachable branches and add tests that enumerate each supported combination, checking that every intended branch is reachable.
- Use differentiated, justified reward components rather than arbitrary flat penalties. Keep the range bounded and document why each component exists. Include feasibility, user feedback, task outcome, social appropriateness, repeated unwanted action and drive changes as separate components where relevant.
- Align docs and UI with the actual supported interaction vocabulary. Include greeting, fetch, disturbance, rest, play, praise, scolding/negative feedback, and feeding only if the environment and action set genuinely support them. Do not document an interaction as supported merely because its keyword is recognized.
- Do not use a fixed action reward table as both the training oracle and the sole correctness measure. Retain a deterministic reward-table test only as a separate unit-level diagnostic.

**Acceptance:** Tests cover every intent/action family, especially far-target fetch, unavailable task, contradictory fields, and each user feedback class. No scenario has internally inconsistent completion and reward definitions.

### 2.5 Proposed-agent internal-drive reward

**Required change:**
- Keep external task/user reward common in meaning across both agents.
- Add a clearly separated proposed-only homeostatic term derived from changes in bounded internal variables, for example Δstress, Δenergy, or curiosity satisfaction.
- Use a potential-difference formulation where appropriate, such as (r_{drive} = β(Φ(s_{t+1})-Φ(s_t))), so the agent is rewarded for improving drive state rather than for remaining permanently in a high-value state. Document the exact potential and clipping.
- Ensure the drive reward cannot dominate user interaction/task outcomes through an arbitrary scale. Log extrinsic reward, each drive component and total training reward separately.
- Keep the baseline free of memory/affect state and proposed-only reward features. Do not leak these features through shared observation fields or shared state encoders.
- Add ablation modes: baseline (external only), proposed with memory only, proposed with affect only, and proposed with both. These help determine which mechanism accounts for any measured effect.
- Freeze the selected reward coefficients for held-out evaluation.

**Acceptance:** With the same initial state and exogenous conditions, internal-drive reward is nonzero only when the relevant proposed-agent drive changes. Removing a drive component removes only that component's contribution. Reward ranges are tested.

### 2.6 Memory has to be used, not merely stored

**Required change:**
- Define a compact, explicit memory feature contract. Example bits: `user_prefers_play`, `user_dislikes_barking`, and optionally `known_fetch_success_pattern`. Keep the state space small enough for tabular learning.
- Memory confidence represents evidence quality, updated from the success/intent-consistency of the encoded fact and subsequent outcomes. Record evidence count and positive/negative support; do not treat one noisy response as certainty.
- Use retrieval at action selection time. Log the exact memory record IDs/facts retrieved, ranking scores, resulting encoded memory bits, and whether those features changed the state key or action ranking.
- Create paired memory tests: preference disclosed early, several intervening events, later opportunity to use it; preference shift; contradictory feedback; a no-memory baseline; and cleared-memory control.
- Prevent persistent user memory from crossing participant boundaries. Synthetic training profiles should have explicit profile IDs and isolated memory stores.
- Do not give the baseline agent memory or affect features. Keep a memory-free baseline state encoder.

**Acceptance:** A test proves that clearing memory removes the relevant memory feature, and that a retained preference changes the proposed agent's decision in a context where that preference is relevant. Logs identify the actual retrieved record.

### 2.7 Exact optimum and table-quality diagnostics

For a one-step deterministic diagnostic with known reward (r(s,a)), compare learned values with the exact table (Q^*(s,a)=r(s,a)) under the stated terminal convention. Do not use this equation for the action-dependent multi-step environment without solving its Bellman equations.

**Required metrics:**
- greedy policy agreement with the exact optimum across enumerated diagnostic states;
- maximum and mean (|Q-Q^*|);
- visit count per state/action, including minimum visits and unvisited entries;
- percentage of states with ties or near-ties among the top actions;
- for the transition environment, policy return against a known planner/oracle on small fully enumerable test worlds, or a converged reference policy where exact solving is feasible;
- action appropriateness by intent, task success, user satisfaction/feedback rate, unwanted repetition, drive trajectory, memory retrieval/use rate and generalisation gap.

**Acceptance:** The diagnostic oracle is independent of the learning implementation, and test failures identify the state/action pair and value discrepancy.

### 2.8 Fair state-space and training budget

**Required change:**
- Coarsely bin affect variables into low/mid/high only where justified; avoid multiplying state dimensions for continuous values that do not add useful decision information.
- Begin with a small number of interpretable memory bits. Calculate the theoretical state count and compare it with observed visits.
- Provide a baseline-only state representation and a proposed representation with the extra memory/affect dimensions. Ensure no accidental feature leakage.
- Warm-starting the proposed table from baseline Q values is an optional controlled experiment, not the only result. Report both cold-start and warm-start results, since warm-starting changes the initialization and could bias comparison.
- Give the larger proposed state space an adequate training budget, selected through learning curves and state/action coverage rather than assuming equal episodes imply equal training.
- Report sample efficiency (performance versus environment interactions), not only final score.

**Acceptance:** The report includes theoretical and observed state counts, state/action visit histograms, total environment interactions and budget justification.

### 2.9 Standard RL and experiment hygiene

**Required change:**
- Record all hyperparameters: learning rate schedule, discount factor, exploration start/end/decay, seed, reward weights, user model parameters, state encoding version, action list, scenario generator version and policy initialization.
- Decay both epsilon and alpha. For example, epsilon can decay exponentially to a floor; alpha can decay per state/action visit or according to a documented schedule. Retain a nonzero exploration floor during training if appropriate, but use greedy evaluation.
- Run periodic evaluation episodes with learning disabled and epsilon zero. Evaluation must not mutate the policy or persistent agent memory unless a separately specified test intentionally examines memory persistence.
- Produce learning curves (mean return and task/user metrics by training checkpoint), not only one final aggregate.
- Run at least 10 independent training seeds initially; increase if confidence intervals remain too wide. Report mean, standard deviation and confidence intervals where appropriate.
- Keep train, validation and test data/scenario seeds separate. Use validation for selecting hyperparameters and report the held-out test only after configuration is frozen.
- Save policy checkpoints with a schema/version and metadata. Reject incompatible state-encoder versions rather than silently loading old Q tables.
- Make runs reproducible and outputs non-overwriting by default, with a run manifest and unique run ID.
- Keep `validate` and `compare` distinct: validation measures one policy/configuration on held-out scenarios; compare evaluates baseline and proposed policies on paired clones and emits paired differences. They must not be aliases that execute identical logic.

**Acceptance:** Repeating a run with the same seed and version reproduces the same traces; distinct seeds generate measurable variability; summary includes mean ± standard deviation and hyperparameters.

### 2.10 Data contract and implementation hygiene

- `outcome.action_id` must identify the action (or a stable action enum), not the step index. Keep a separate `step_index` field in logs.
- Distinguish `terminated` from `truncated`; do not overload one `terminal` boolean.
- Replace positional aggregate initializers with named factory helpers so field reordering cannot silently change scenario meaning.
- Check stream open/write/flush errors and include schema version on each log/summary.
- Use separate run IDs and file names for train, validation, test and comparison; never accidentally evaluate a partially overwritten checkpoint.
- Document every supported interaction and action in one source of truth shared by UI, NLP adapter, environment and tests.

## 3. Proposed architecture

```text
Scenario Generator / Split
          |
          v
Cloned Environment + Synthetic User (per condition)
          |
    observation_t
          |
          v
Agent policy -> action_t
          |
          v
Environment.step(action_t)
  |       |        |
  |       |        +--> user feedback (stochastic, hidden profile)
  |       +-----------> state transition
  +-------------------> reward components
          |
          v
Agent outcome update (s, a, r, s', terminated/truncated)
          |
          +--> Q update (training only)
          +--> proposed memory/affect update
          +--> structured event log
```

For paired comparisons, clone the same initial environment and user profile for each condition and reuse the same exogenous random draws where practical. Action-dependent state transitions are allowed to diverge after each agent acts. Record this design choice because the resulting trajectories will no longer have identical observations after actions diverge.

## 4. Staged implementation plan

### Stage −1 — Shared action architecture and scope cleanup

This prerequisite stage is tracked in [Docs/20-stage-minus-one-action-architecture.md](20-stage-minus-one-action-architecture.md). Define one canonical action vocabulary shared by both agents, make validity depend only on observable environment affordances, apply the same mask during selection and Q-value bootstrapping, and provide the same action-to-animation layer for both conditions. Include food, play, and negative-feedback contexts so the additional actions can actually be exercised. Remove disconnected duplicate simulation and unused abstractions only when no active workflow depends on them.

**Exit gate:** Both agents expose the same 14 actions; impossible actions are masked; food/play affordances are represented in the observation and policy state; action selection and next-state bootstrap respect the mask; the shared animation mapping is used by the UI; and the project builds with the active workflows intact.

### Stage 0 — Correctness and instrumentation
1. Replace `action_id = step` with a stable action ID.
2. Introduce explicit terminated/truncated fields and correct Q-update semantics.
3. Add typed scenario builders and validate observation invariants.
4. Fix far-fetch reward/success consistency and dead branches.
5. Add a deterministic reward-table oracle test and state/action visit counts.
6. Record current hyperparameters and policy/state schema versions.

**Exit gate:** Unit tests pass; reward and completion logic agree; current diagnostic results are reproducible.

### Stage 1 — Scenario coverage and split
1. Implement separate train/validation/test scenario generators.
2. Randomize scenario order and episode length without fixed six-step cycles.
3. Enumerate or sample the discretized observation space with coverage targets.
4. Add holdout context combinations and report coverage.

**Exit gate:** Held-out states/combinations are demonstrably excluded from training and included in validation/test reports.

### Stage 2 — Stochastic user and transition environment
1. Implement hidden synthetic-user traits and noisy feedback.
2. Make actions change world state and future observations.
3. Add task, social, disturbance, play, praise/scold and rest transitions where supported.
4. Add terminal and truncation handling and transition tests.
5. Use paired environment clones for baseline/proposed comparison.

**Exit gate:** At least one action changes a later state and reward; same seed reproduces traces; paired agents begin from equivalent conditions.

### Stage 3 — Proposed memory and affect contribution
1. Define compact memory bits and coarsely discretized affect bins.
2. Connect retrieved facts to policy state and log exact retrieval/use.
3. Add proposed-only potential-difference drive reward, separately logged.
4. Add memory-only and affect-only ablations.
5. Add persistence isolation for synthetic users and tests for preference retention/shift.

**Exit gate:** Controlled ablations show which features change behaviour; memory and affect effects can be traced from input to state to action.

### Stage 4 — Learning curves and multi-seed evaluation
1. Add alpha/epsilon schedules and periodic greedy evaluation.
2. Train across at least 10 seeds and save checkpoints.
3. Report mean, standard deviation, confidence intervals, learning curves and sample efficiency.
4. Add exact-optimum metrics for the bandit diagnostic and a reference planner for small transition worlds.
5. Keep validation for tuning and freeze final parameters before test.

**Exit gate:** Results are reproducible, held-out, and include uncertainty and coverage rather than a single-seed score.

### Stage 5 — Research-facing comparison
1. Keep questionnaire outcomes separate from synthetic reward.
2. Replay controlled context-retention and affect-adaptation scenarios.
3. Compare baseline, full proposed model, memory-only and affect-only where feasible.
4. Ensure participant-facing A/B labels remain blind and memory is scoped per participant.
5. Report behavioural evidence and perceived-believability evidence separately.

**Exit gate:** The simulation can answer whether the mechanisms work; the participant study can separately assess whether people perceive the resulting behaviours as more believable.

## 5. Suggested output layout

```text
artifacts/training/<run-id>/
  manifest.json
  train_summary.json
  train_episodes.jsonl
  train_steps.jsonl
  learning_curve.jsonl
  checkpoints/
  validation_summary.json
  validation_episodes.jsonl
  test_summary.json
  test_episodes.jsonl
  comparison_summary.json
  comparison_pairs.jsonl
  coverage_report.json
  q_oracle_diagnostic.json
```

Every summary should include schema version, code revision, seed list, state/action schema version, reward weights, user-simulator parameters, alpha/gamma/epsilon schedules, episode/step budgets, checkpoint identity, and train/validation/test split identifiers.

## 6. Interpretation rules

- Higher synthetic return is evidence only relative to the defined synthetic environment and reward.
- A perfect bandit-oracle score confirms the learner can reproduce that diagnostic reward table; it does not show long-horizon competence.
- Better task performance does not automatically mean greater perceived believability.
- A proposed-agent advantage is meaningful only if it persists on held-out contexts and is attributable to memory/affect rather than extra action options, unequal observation, reward leakage or extra training alone.
- Report negative or null results honestly. If the baseline matches or exceeds the proposed agent under a valid evaluation, that is useful evidence about the current design.

## 7. Recommended immediate next task

Complete the Stage −1 verification gate first, then implement **Stage 0 only**. Stage −1 standardises the action contract; Stage 0 then repairs reward/data semantics and instrumentation before the project adds a complicated user simulator or more reward coefficients. After Stage 0 passes, proceed sequentially through Stages 1–4.
