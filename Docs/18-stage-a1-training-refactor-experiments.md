# Stage A.1 — Controlled Training and Policy Refactor Experiments

**Status:** Experiment menu; hypotheses require diagnostic evidence before implementation  
**Target branch:** `Dev` only  
**Repository:** `SAmWaM42/IS-proj`  
**Companion specification:** [17-stage-a1-training-diagnostics.md](17-stage-a1-training-diagnostics.md)

## 1. Purpose

Define a controlled, reversible set of possible training and policy refactors to address the Stage A findings:
- limited local state-action exploration despite 100% Q-table support;
- delayed `INSIST` being eligible but rarely selected;
- repeated action loops despite a shared bounded boredom penalty;
- reward comparisons confounded by variable episode lengths;
- possible state aliasing, Q-learning update defects, and checkpoint/provenance ambiguity.

This document is not blanket approval to implement every experiment. The local coding agent must complete the diagnostic specification first, rank candidate interventions by evidence, and run one isolated experiment at a time. Preserve the stable baseline and the current Stage A implementation as comparison points.

## 2. Non-negotiable research and engineering constraints

1. Keep the baseline and proposed agents' action vocabulary, environment rules, reward semantics, scenario exposure, and training opportunities comparable.
2. Do not add memory or affect features to the baseline.
3. Do not remove, weaken, or silently bypass memory/affect features in the proposed agent.
4. Do not force `INSIST` selection, hard-code desired action sequences, or script a companion personality to make a metric look better.
5. Do not tune against participant believability scores or use synthetic reward as a proxy for believability.
6. Do not use validation results to tune hyperparameters. Use training plus a separate development-validation split; preserve a final untouched evaluation seed set.
7. Keep learning/exploration enabled only in training. Evaluation must use frozen Q-tables and zero exploration.
8. Preserve paired scenario inputs and record all seeds/configuration.
9. Make every change reversible through a separate commit or isolated experiment configuration. Do not overwrite the only known-good checkpoint.
10. All behaviourally relevant changes to shared simulation, observation, action, reward or state contracts must be assessed for participant-client parity. Do not port to `deployment/participant-client` without explicit project-owner approval after Dev validation.
11. Never modify or redeploy production Vercel. No deployment changes are authorised by this document.
12. Do not introduce neural RL, new frameworks, or dependencies. Retain tabular Q-learning unless a separate approved research decision changes scope.

## 3. Experimental protocol

### 3.1 Establish immutable reference runs

Before any refactor:
- preserve the current Stage A source commit and a clean build/test result;
- preserve the current baseline and proposed checkpoints separately;
- identify whether the checkpoints use the new `no_response_count` state encoding;
- save compact summaries and hashes for all reference artifacts;
- record exact commands and all relevant seeds.

Do not compare runs trained from different checkpoint provenance as if only one hyperparameter changed.

### 3.2 One hypothesis per experiment

For each candidate:
1. State the observed evidence motivating it.
2. State a falsifiable hypothesis and expected metric movement.
3. Identify which agent(s) and components it affects.
4. Change one conceptual variable only.
5. Add or update focused tests.
6. Run unit/self-tests and paired scenario checks.
7. Train using a fixed training-seed set.
8. Evaluate with frozen policies on disjoint fixed validation seeds.
9. Compare against the same reference configuration.
10. Record positive, negative, and inconclusive outcomes. Revert or retain based on evidence, not preference.

Use the same seeds and scenario streams across paired configurations wherever possible (common random numbers). If a change alters the scenario generator, first prove the paired-stream contract remains valid.

### 3.3 Minimum reporting

For each experiment, report:
- training and validation seeds;
- episodes, steps/horizon, checkpoint provenance;
- learning rate, discount factor, exploration schedule and all changed parameters;
- mean and spread per agent for reward/step, total reward, episode length, completion/termination;
- stimulus alignment with denominators;
- selected action distribution and action-category run lengths;
- INSIST eligible opportunities and selection rate;
- boredom penalty triggers, total penalty and contribution to final reward;
- state and state-action coverage, Q-table support, fallback rate;
- proposed-agent memory/affect state differentiation diagnostics;
- tests and build commands/results;
- a clear decision: retain, revert, or investigate further.

Do not rank experiments on total reward alone.

## 4. Candidate refactors (prioritised, conditional)

### R0 — Correctness fixes before tuning (highest priority; only if D1 proves a defect)

Potential issues to check:
- Q update bootstraps from terminal states;
- maximisation includes invalid actions;
- implicit zero values for unvisited actions dominate negative learned Q-values unintentionally;
- next-state action set is empty or incorrectly constructed;
- epsilon exploration and greedy selection use inconsistent valid-action sets;
- evaluation accidentally learns or explores;
- reward/penalty is logged but not included in the reward passed to Q update.

**Experiment:** create the smallest failing unit test for each confirmed defect, fix only that defect, then rerun all relevant tests and a reference training/validation run.

**Acceptance:** update equation and edge cases match the intended tabular Q-learning contract; no regression in paired environment inputs or agent separation. Do not combine correctness fixes with hyperparameter changes.

### R1 — Training exploration schedule (conditional on insufficient state-action visitation)

Hypothesis: the current exploration schedule may not provide enough early coverage of valid actions/states, or may decay before useful alternatives are sampled.

Possible controlled variants:
- constant epsilon during an initial exploration phase followed by a documented decay;
- slower linear or exponential decay;
- a non-zero exploration floor during training only;
- episode-based rather than step-based decay, if that better matches the implementation.

Requirements:
- use the same schedule family and comparable opportunity budget for both agents unless the diagnostic evidence justifies a difference;
- record epsilon per episode and aggregate exploratory versus greedy decisions;
- keep evaluation epsilon at zero;
- compare visitation coverage and validation performance, not training reward alone.

Do not add state-count bonuses or forced action quotas in the first experiment. Those change the learning objective and can mask the actual problem.

### R2 — INSIST learning signal and state coverage (conditional on D2 evidence)

Do not make INSIST mandatory. Possible interventions, only if diagnostics support them:
- ensure eligible `no_response_count >= 2` states are sufficiently represented in training scenarios;
- verify INSIST has a valid, distinct and observable environment consequence;
- correct reward credit assignment if INSIST's delayed benefit is not credited to the action;
- adjust exploration exposure so eligible states sample all valid actions;
- refine state encoding only if no-response distinctions are being aliased.

A reward change is a separate experiment and must be justified by the intended behaviour definition, not by a desire to increase INSIST frequency. Compare INSIST with alternatives under matched eligible states and report downstream response, not selection frequency alone.

**Acceptance:** INSIST remains unavailable at counts 0 and 1, becomes eligible at 2, is dispatched correctly, and any learned use is supported by reproducible state/action values and a meaningful environment outcome. If the optimal policy legitimately prefers another action, document that rather than forcing a change.

### R3 — Repetition penalty calibration or formulation (conditional on D3 evidence)

First verify that the current penalty is applied to the Q-learning reward. If it is not, fix wiring as a correctness defect.

If it is applied but dominated, consider separate experiments:
- small, bounded calibration of the per-repeat penalty, with the existing cap retained initially;
- penalty applied to repeated action-category runs only when no meaningful response occurs (preserve current intended semantics);
- alternative diminishing/increasing schedule for successive repeats, only if the current linear penalty is demonstrably ineffective;
- clearer definition of meaningful response/reset events if the current event classifier is inaccurate.

Do not penalise repetition indiscriminately: repeating FETCH may be appropriate when the task remains unresolved, and a companion can have stable preferences. The objective is to discourage unresponsive loops, not maximise action diversity.

Any reward adjustment must be applied identically to both agents and must not selectively disadvantage the proposed agent. Preserve component-level reward logs so trade-offs are visible.

**Acceptance:** tests cover first action, repeated action, cap, category change, meaningful response, no-response, and both agents. Validation should show fewer *unresponsive* repeated loops without materially degrading task completion or stimulus alignment. Report cases where repetition remains appropriate.

### R4 — State representation and aliasing (conditional on D4 evidence)

Potential changes:
- correct missing state dimensions or encoding mistakes;
- refine only the specific discretisation boundary shown to collapse behaviourally distinct states;
- reduce redundant dimensions if they cause sparse state-action visitation without adding useful distinctions;
- retain bounded memory/affect summaries and deterministic state construction.

Before increasing granularity, estimate theoretical state-space growth and expected data sparsity. Do not add raw memory records or continuous histories to the Q-table key.

**Acceptance:** controlled unit tests show the intended inputs change state identity; unchanged inputs remain deterministic; baseline remains free of memory/affect; proposed-agent state distinctions are interpretable and do not explode the table without measurable benefit.

### R5 — Episode horizon and metric normalisation (conditional on D5 evidence)

Do not alter environment dynamics merely to make episode lengths equal.

Instead:
- preserve a fixed maximum horizon and record actual termination reason;
- report total reward and per-step reward together;
- add paired fixed-horizon analysis where scientifically meaningful;
- report completion/early termination separately;
- ensure scenario comparisons use paired inputs and explicit denominators.

If variable episode length is caused by a runner defect, fix it. If it is a legitimate action-dependent outcome, keep it as an outcome and report it rather than normalising it away.

**Acceptance:** summary outputs make duration effects visible and do not reward/penalise an agent simply for being evaluated over a different number of steps without explaining why.

### R6 — Hyperparameter sensitivity (only after R0–R5)

Evaluate a small, predeclared grid or one-factor-at-a-time range for learning rate (`alpha`), discount factor (`gamma`), and exploration schedule. Keep the search budget modest and symmetric for both agents. Do not run an unconstrained recursive search that selects on validation reward.

Record each configuration and use training/development-validation seeds for selection. Reserve untouched evaluation seeds for the final comparison.

**Acceptance:** selected parameters are reproducible, not chosen from a single lucky seed, and no change degrades the fairness of the baseline/proposed comparison.

### R7 — Checkpoint and run management

Improve run metadata and checkpoint isolation if D6 identifies ambiguity:
- unique run IDs and output directories;
- explicit checkpoint schema/state-encoding version;
- metadata for source commit, seed, hyperparameters, mode and initialization;
- reject incompatible checkpoint resumes with a clear diagnostic;
- evaluation loads checkpoints read-only and records hashes before/after.

**Acceptance:** every summary can be traced to one exact source revision, configuration, seed set and checkpoint.

## 5. Recommended experiment order

1. Complete diagnostics D1–D6 (D7 if feasible).
2. Fix only confirmed correctness/provenance defects (R0/R7).
3. Investigate the repetition reward path (R3) and INSIST path (R2) independently; choose the order based on diagnostic severity.
4. Test exploration schedule (R1) if coverage evidence warrants it.
5. Address state representation only if aliasing is demonstrated (R4).
6. Improve episode reporting/runner only if fairness or accounting is defective (R5).
7. Perform limited hyperparameter sensitivity (R6) after the preceding issues are stable.

Do not implement R0–R7 as one combined patch. Keep each experiment independently revertible.

## 6. Automated local-agent loop and stopping rules

The local coding agent may execute this bounded loop on `Dev`:

1. Read both Stage A.1 specifications and relevant existing specifications.
2. Inspect source and produce the diagnostic report.
3. Select the highest-priority evidence-backed hypothesis.
4. Propose a minimal diff and list affected files/contracts.
5. Add a regression test before or alongside the fix.
6. Build and run focused tests, then full self-tests/CTest.
7. Run paired train/validate experiments with recorded seeds and fresh compatible checkpoints.
8. Generate a comparison report against the immutable reference.
9. Retain the change only if acceptance criteria pass; otherwise revert it cleanly.
10. Record the result and proceed to the next independent hypothesis.

The loop must stop and request project-owner review if:
- the proposed change alters the research question, agent definitions, action vocabulary, environment semantics or reward objective;
- baseline/proposed parity cannot be preserved;
- a state/checkpoint schema migration is required;
- tests fail and the cause is unclear;
- the change would require modifying `deployment/participant-client`, credentials, external services or production;
- the agent is tempted to tune against participant believability responses or validation outcomes;
- no objective acceptance criterion distinguishes success from failure.

**No automatic production deployment, participant-branch port, or participant study run is authorised.**

## 7. Required test and validation gates

At minimum, preserve and rerun the current project verification sequence after relevant changes:

```bash
cmake --build build -j"$(nproc)"
./build/AnimalAgent --self-test
ctest --test-dir build --output-on-failure
```

Then run controlled training and validation with:
- fresh compatible checkpoints for each changed state encoding;
- multiple paired training seeds;
- disjoint validation seeds;
- zero exploration and no learning in validation;
- identical paired scenario inputs for baseline and proposed;
- a new output directory for each experiment.

Use the actual executable's current CLI help/source to confirm flags; do not assume historic command examples still match the current code.

## 8. Completion and hand-off

For every experiment, the local model must report:
- hypothesis and diagnostic evidence;
- commit(s) and changed files;
- tests/build commands and exit status;
- exact training/validation commands and seeds;
- checkpoint provenance;
- before/after metrics and their denominators;
- known regressions or limitations;
- retain/revert decision and rationale.

Stage A.1 is complete when confirmed correctness issues are resolved, the INSIST and repetition mechanisms are understood, coverage and reward comparisons are interpretable, and any retained training changes improve a predeclared behavioural objective without breaking parity.

A successful outcome may be **no refactor** if the existing behaviour is correct and the observed policy is a legitimate consequence of the current objective. The goal is a reliable, explainable experiment—not a predetermined increase in reward, INSIST frequency, or action diversity.
