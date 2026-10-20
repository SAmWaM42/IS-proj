# Training and Validation Implementation Plan

Status: headless training/evaluation and an initial SFML participant-study prototype are implemented; build and end-to-end study-flow verification remain required.

## Goal

Build a reproducible synthetic experiment around the existing C++ baseline and proposed agents, then collect separate participant feedback through a controlled interaction interface. The baseline and proposed agent must receive equivalent environment events; only the proposed agent may use memory- and affect-derived state.

## Current repository constraints

- The current `runAgentComparison` is a short comparison harness, not yet a full evolving environment or a statistically meaningful experiment.
- The headless training path now seeds each agent's exploration RNG and uses a seeded environment generator. The separate SFML runtime still has its own time-seeded random stimuli and is not covered by that guarantee.
- Terminal transitions are now passed into Q-learning updates so terminal targets do not bootstrap from next-state values; the self-test checks this behavior.
- The SFML `BackyardSimulation` currently owns a separate policy and reward path. The experiment harness should be headless and must not depend on opening an SFML window.
- Existing validation checks cover component contracts; they do not by themselves establish that a policy improves over training or that users perceive it as believable.

## Implemented in the current branch

- Added `Components/Training/training.h/.cpp` as a headless synthetic experiment runner.
- Added `--train`, `--validate`, `--compare`, and `--self-test` CLI modes, plus episode, step, seed, and output-directory arguments.
- Training runs both agents against the same pre-generated observations and synthetic reward function, with a decaying exploration rate. Q-tables are saved as `baseline.qtable` and `proposed.qtable`.
- Validation/comparison loads those policies, disables exploration and learning, and uses an evaluation environment seed offset from the supplied seed.
- Each condition writes step-level JSONL logs; each run writes an episode-level JSON summary.
- Q-learning updates now accept a terminal flag, seeded exploration is per-agent, and validation checks cover seeded exploration, baseline feature isolation, and evaluation-mode Q-table immutability.

The synthetic reward function is deliberately a development proxy and must not be presented as perceived believability. Build/test execution against the branch still needs to be confirmed. The checked-in `Docs/test-results.json` is not automatically refreshed by these commits; run `./build/IS-2 --self-test` after building to regenerate it.

- Added an SFML participant-study prototype under `Components/Study`, exposed through `--study`. It uses randomised A/B condition mapping, the same five scenarios in both conditions, participant feedback after each action, and four 1–5 questionnaire items per condition.
- Participant-session Q-table learning is disabled. Feedback is supplied to the agent outcome path so the proposed condition can update memory and affect; questionnaire ratings are stored separately from reward.
- Study output uses a random session code and is written under `artifacts/study/`, which is git-ignored. The output contains a condition key for analysis and must be treated as restricted research data.
- This interface is a prototype, not an ethics-approved study instrument. Obtain supervisor/institutional approval, confirm the consent process and questionnaire, and pilot the full flow before collecting participant data.
- Optional policy sharing remains unimplemented.

## Delivery order

### Phase 1 — Headless synthetic training and evaluation

1. Define a small environment transition contract that returns an observation, reward, and terminal flag.
2. Give both agents the same seeded environment events and reward rules.
3. Keep baseline features environment-only. Do not pass proposed-agent memory, affect, or runtime context into baseline state encoding.
4. Add explicit train, validate, compare, and self-test modes. Training may update the policy; validation/test must run with exploration disabled and must not update Q-values.
5. Separate training scenarios from validation/test scenarios and record the seed and configuration in each run.
6. Export machine-readable episode/step logs and aggregate metrics. At minimum: total reward, episode length, terminal/completion rate, action counts, and per-episode outcome.
7. Add tests for reproducible seeded runs, baseline feature isolation, terminal updates, complete logs, and evaluation not mutating the policy.

### Phase 2 — Interaction UI and participant feedback

Initial prototype added in `Components/Study/study.cpp`. Launch with:

```sh
./build/IS-2 --study --policy-dir artifacts/training --study-out artifacts/study
```

The UI requires trained Q-tables, presents A/B conditions in randomised order, reuses the same five scenario observations, gathers feedback (positive, neutral, negative, or no response), and asks for 1–5 ratings of believability, consistency, adaptability, and perceived continuity. The participant-facing label does not identify the agent type; the output's condition key maps A/B to baseline/proposed for analysis. Q-table learning is disabled during sessions to avoid online policy updates confounding the comparison. Participant feedback still updates the proposed agent's memory/affect through its normal outcome path.

The JSON output is anonymous by design and stored under the git-ignored `artifacts/study/` directory. It includes scenario responses, ratings, and a condition key; keep these files restricted. This implementation does not replace institutional ethics approval or a supervisor-approved consent process. Pilot it with test data before collecting real responses.

1. [Prototype implemented] Build a user-facing interaction flow that can run either agent against the same bounded scenario.
2. [Implemented] Keep questionnaire responses separate from the reward signal used to update agent state.
3. [Implemented, subject to study review] Record consent status, anonymous session ID, condition, scenario/event sequence, and questionnaire responses.
4. [Required] Do not claim perceived believability from synthetic reward or synthetic-user engagement. Believability must be assessed through the planned participant evaluation and an explicit measure.

### Phase 3 — Optional shared policy experiment

Treat policy sharing as a separate, optional experiment after local training and evaluation are stable. Define policy versioning, state/action schema compatibility, provenance, conflict resolution, rollback, and data-minimisation requirements first. Sharing Q-table entries is centralised policy aggregation, not automatically federated learning, and Q-tables must not be assumed private merely because they contain no raw text.

## Experiment integrity requirements

- Keep agent conditions, environment rules, reward mappings, action space, and episode limits equivalent unless a difference is an explicit independent variable.
- Use fixed seeds for reproducibility, multiple seeds for robustness, and held-out scenarios for evaluation.
- Do not train on the validation/test scenarios.
- Report synthetic-environment performance separately from participant-reported believability.
- Log enough configuration to reproduce a run; do not log identifiable participant information by default.

## CLI target

The intended interface is:

```sh
./IS-2 --train --episodes 1000 --steps 40 --seed 42 --out ../artifacts/training
./IS-2 --validate --episodes 200 --steps 40 --seed 43 --out ../artifacts/training
./IS-2 --compare --episodes 200 --steps 40 --seed 44 --out ../artifacts/training
./IS-2 --self-test
```

The CLI modes are now wired into the branch. Run the self-tests and build before relying on experimental output; treat the synthetic metrics as engineering diagnostics, not evidence of perceived believability.
