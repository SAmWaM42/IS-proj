# Training and Validation Implementation Plan

Status: Phase 1 headless training/evaluation CLI added; compile/runtime verification is still required.

## Goal

Build a reproducible synthetic experiment around the existing C++ baseline and proposed agents before adding participant-facing interaction. The baseline and proposed agent must receive equivalent environment events; only the proposed agent may use memory- and affect-derived state.

## Current repository constraints

- The current `runAgentComparison` is a short comparison harness, not yet a full evolving environment or a statistically meaningful experiment.
- `EnvironmentConfig::seed` exists, but the complete agent/policy path must be audited for deterministic random-number generation before seeded reproducibility can be claimed.
- `ActionOutcome` carries terminal information, while the Q-learning update API currently does not expose a terminal flag. Terminal transitions must not bootstrap from a next-state value.
- The SFML `BackyardSimulation` currently owns a separate policy and reward path. The experiment harness should be headless and must not depend on opening an SFML window.
- Existing validation checks cover component contracts; they do not by themselves establish that a policy improves over training or that users perceive it as believable.

## Implemented in the current branch

- Added `Components/Training/training.h/.cpp` as a headless synthetic experiment runner.
- Added `--train`, `--validate`, `--compare`, and `--self-test` CLI modes, plus episode, step, seed, and output-directory arguments.
- Training runs both agents against the same pre-generated observations and synthetic reward function, with a decaying exploration rate. Q-tables are saved as `baseline.qtable` and `proposed.qtable`.
- Validation/comparison loads those policies, disables exploration and learning, and uses an evaluation environment seed offset from the supplied seed.
- Each condition writes step-level JSONL logs; each run writes an episode-level JSON summary.
- Q-learning updates now accept a terminal flag, seeded exploration is per-agent, and a validation check covers seeded exploration and baseline feature isolation.

This is a first headless experiment slice, not a complete participant study. The synthetic reward function is deliberately a development proxy and must not be presented as perceived believability. The participant-facing interface/questionnaire and optional shared-policy experiment remain unimplemented. Build/test execution against the branch still needs to be confirmed.

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

1. Build a user-facing interaction flow that can run either agent against the same bounded scenario.
2. Keep participant questionnaire responses separate from training rewards and synthetic-user labels.
3. Record consent status, anonymous session ID, agent condition, scenario, event sequence, and questionnaire responses only as required by the study protocol.
4. Do not claim perceived believability from synthetic reward or synthetic-user engagement. Believability must be assessed through the planned participant evaluation and an explicit measure.

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
