# Regression-test requirements

The following are required before any approval of behavioural or instrumentation changes:

1. Provenance contract: the exported artifact must include commit hash, binary name, build mode, and command line.
2. Export completeness: the step log must include all fields needed to audit repeated attempts and INSIST eligibility.
3. Delayed-INSIST validity: `INSIST` must be invalid at `no_response_count < 2` and valid at `>= 2` under the same pending-task state.
4. Repetition penalty sequence: repeated same-category actions without meaningful response must produce `0.00, 0.04, 0.08, ...` discounting with a cap of `0.20`.
5. Q-learning regression: verify that the TD update formula remains identical and that learning remains bounded under the same reward/discount controls.
6. Coverage metric regression: verify the meaning and denominator for unique states, valid state-action pairs, Q-table support, and fallback decisions.
7. Seed determinism: same seed must reproduce scenario ordering and same `no_response_count` transitions.
8. Artifact integrity test: a fresh run must produce a step log that can be correlated to the exact build and source revision.

No regression tests were altered or run as a code change in this stage; this is a requirements list only.
