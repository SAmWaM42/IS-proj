# Synthetic User Training Experiment

**Status:** Approved implementation target for local development on `Dev`  
**Repository:** `SAmWaM42/IS-proj`  
**Scope:** Research/training harness only; not the participant client  
**Purpose:** Replace or supplement pre-generated user-response observations with a seeded synthetic interaction partner for controlled baseline/proposed-agent experiments.

## 1. Motivation

The current training harness creates a randomised `std::vector<EnvironmentObservation>` using `makeScenarioSchedule(...)`. This varies scenario composition, but it does not model a user who reacts to the companion's chosen action. In `runEpisode(...)`, the next observation is largely selected from that pre-generated schedule and merged with environment transitions. The current `meaningfulResponse` definition (`transition.completed || observation.user_status != 0`) therefore does not represent an independent user-response process.

A synthetic user should make the training interaction more contingent on the agent's behaviour. It is an engineering/testing mechanism, not a model of real human behaviour and not evidence of believability.

## 2. Experiment hypothesis

**Hypothesis:** Training with a simple, seeded, action-contingent synthetic user will provide more informative interaction feedback than the current open-loop observation schedule, allowing both agents to learn in a more interactive setting. The proposed agent may benefit from memory/affect when responses and requests persist across turns, but this is an empirical question.

Do not assume that synthetic-user training must improve mean reward. A valid result may be no improvement, a baseline advantage, or a need to simplify the response model.

## 3. Scope and non-goals

In scope:
- A training-only synthetic-user component.
- Stable per-episode user traits.
- Action-contingent positive, negative, or absent responses.
- Persistent requests and no-response progression.
- Seeded response randomness and paired-agent comparability.
- CLI/config selection between the existing scripted schedule and the synthetic-user mode.
- Diagnostics that describe user responses and request outcomes.

Out of scope:
- Changes to Q-learning equations, reward weights, action vocabulary, action validity, state encoding, or memory/affect architecture.
- Agent-specific user profiles or different response probabilities for baseline and proposed.
- A scripted sequence of desired companion actions.
- Training the synthetic user with an LLM or adding dependencies.
- Changes to participant UI, participant-client, deployment branch, Vercel, or production.
- Claiming that synthetic-user reward or response rates measure believability.

## 4. Proposed model

Implement the smallest self-contained model that fits the current training harness. Prefer a separate type in `Components/Training` (for example, `synthetic-user.h/.cpp`) rather than adding more policy logic to `training.cpp`.

### 4.1 Per-episode profile

Generate one profile at the start of each episode from the episode seed. It remains fixed for both agents during the paired episode. Keep traits bounded and log their values.

Suggested traits:
- `responsiveness`: probability scaling for acknowledging an appropriate interaction;
- `patience`: number of ignored turns tolerated before a pending request becomes more likely to elicit a response or negative feedback;
- `social_openness`: likelihood of positively acknowledging social bids.

Use modest, documented ranges. Do not make one profile uniformly easy or uniformly hostile. Profile generation must be deterministic from the episode seed.

### 4.2 User response contract

The synthetic user receives:
- the current public `EnvironmentObservation`;
- the selected action;
- the environment transition result (including fetch success/completion and updated proximity/energy);
- the profile;
- a pre-generated per-step random draw (or equivalent exogenous randomness).

It returns a small structured result containing at least:
- response status compatible with the existing convention (`0 = no response`, `1 = positive/acknowledging response`, `2 = negative/rejecting response`);
- updated no-response count for the current pending request;
- whether a pending request remains active or has been acknowledged/resolved;
- a response category/reason suitable for diagnostics.

Verify the meaning of existing `user_status` in source before implementation; preserve the existing convention rather than silently redefining it.

### 4.3 Behavioural rules

Use a compact, explainable response model. It must not simply reuse `actionContextFit()` as an oracle or return positive feedback only for a single hard-coded correct action.

At minimum, cover:
- **Pending fetch request:** successful FETCH is acknowledged; failed FETCH leaves the request active; APPROACH_FOLLOW may make progress without completing the request; unrelated actions may receive silence; repeated no-response can eventually produce acknowledgement or rejection according to patience and responsiveness.
- **Social context:** GREETING, NUZZLE, and PLAY can receive positive acknowledgement depending on social openness; neutral actions can receive silence; clearly aversive actions may receive negative feedback with a bounded probability.
- **Food/rest context:** EAT and SLEEP_LAY can be acknowledged when those needs are present; the user response must not override physical energy/food transitions.
- **Disturbance context:** responses should be plausible and stochastic; do not make BARK the universal correct response.
- **No-response progression:** silence increments the count only while the same request remains pending; a positive/negative response or a genuinely new request resets the count. INSIST must remain governed by the existing action-validity rule and must not be forced.

The synthetic user must not invent a new user request every step. Preserve a request across turns until it is acknowledged/resolved or a new scenario event genuinely replaces it.

A delayed response may be represented by a small pending-response countdown if this can be implemented without destabilising the harness. If it adds disproportionate complexity, use immediate stochastic responses for this first increment and describe that limitation; do not pretend immediate feedback models response latency.

## 5. Paired experiment contract

This is essential.

For each episode, baseline and proposed must receive:
- the same initial environment observation/request;
- the same synthetic-user profile;
- the same exogenous per-step response random draws;
- the same elapsed-time sequence;
- the same environment-transition random stream contract already used by the harness.

The synthetic user's response may differ after the agents choose different actions. That is the intended action-contingent consequence, not a pairing failure. Do not force both agents to receive identical realised responses after their actions diverge.

Do not let random-number consumption in one agent's branch shift the other agent's response draws. Pre-generate the per-step response randomness or derive it deterministically from episode seed and step index.

The synthetic user must be independent of agent identity, policy type, Q-values, memory contents, and affective state. It observes only public environment context and the action/outcome.

## 6. Integration and experiment selection

Keep the existing scripted schedule as a reproducible reference mode. Add an explicit experiment selector (for example, `--user-model scripted|synthetic`) with:
- default remaining `scripted` to preserve historical commands and artifact comparability;
- synthetic mode explicitly recorded in run metadata and every summary;
- invalid values rejected with a clear error;
- no change to participant application CLI.

Do not silently mix scripted and synthetic episodes in one run. If a mixed curriculum is later proposed, specify and evaluate it separately.

## 7. Diagnostics and output

Add per-agent and per-run metrics:
- synthetic-user profile values and profile seed;
- positive, negative, and no-response counts;
- response rate and positive/negative proportions with explicit denominators;
- responses by context/action category;
- requests initiated, acknowledged, completed, abandoned, and still pending at termination;
- no-response streak distribution and maximum;
- INSIST opportunities, attempts, and responses following INSIST;
- average interaction length and termination reason.

Each step log should include:
- user-model name;
- request/context identifier;
- synthetic-user response status/category;
- no-response count before/after;
- whether the request remains pending;
- response random draw or a reproducible identifier for it;
- action and environment transition outcome.

Keep existing reward, coverage, energy, and action diagnostics. Do not rename stimulus-alignment metrics as user satisfaction or believability.

## 8. Tests

Add focused tests for:
1. Identical episode seeds produce identical profiles and response sequences for identical action/outcome sequences.
2. Different seeds can produce different profiles/response outcomes.
3. A pending request persists through silence.
4. No-response count increments on silence and resets on acknowledgement/new request.
5. Successful FETCH is acknowledged and resolves the fetch request.
6. Failed FETCH does not incorrectly resolve the request.
7. APPROACH_FOLLOW can progress a request without falsely completing it.
8. INSIST remains unavailable at no-response counts 0 and 1 and eligible at 2, without forced selection.
9. Response generation does not depend on whether the caller is BaselineAgent or ProposedAgent.
10. Paired response draws are unaffected by divergent action paths or random-number consumption.
11. Existing scripted mode produces unchanged scenario streams and preserves historical CLI defaults.

Run focused tests, full build, `--self-test`, and CTest.

## 9. Experiment protocol

Preserve the current Stage A reference checkpoints and artifacts. Train new policies in a new output directory; never overwrite the existing reference.

Use:
- a small smoke run first;
- then multiple fixed training seeds (at least 5 if runtime permits);
- disjoint validation seeds;
- fresh checkpoints trained under the synthetic-user mode;
- frozen policies and zero exploration during validation;
- paired baseline/proposed runs with common exogenous inputs.

Compare scripted-reference and synthetic-user experiments separately. Report:
- mean reward/step and total reward alongside actual episode length;
- completion and termination reasons;
- energy depletion;
- action distribution and unresponsive repetition;
- request acknowledgement/completion rates;
- response rates and no-response streaks;
- INSIST opportunities and outcomes;
- state/action coverage and Q-table support;
- proposed memory/affect differentiation where available.

Do not tune response probabilities against validation outcomes. If parameters need calibration, define a small training/development set and reserve untouched evaluation seeds.

## 10. Acceptance and stopping rules

The implementation is ready for review when:
- scripted mode remains reproducible and unchanged by default;
- synthetic mode is deterministic and fully logged;
- both agents share the same synthetic-user mechanism and paired exogenous randomness;
- response behaviour depends on public context and action/outcome, not agent identity or hidden policy state;
- tests/build/self-tests/CTest pass;
- run provenance records the user-model mode and parameters;
- no participant/deployment/production files or branches are changed.

After the smoke and multi-seed results, stop for project-owner review. Do not automatically modify scripted participant interactions or reward/state/policy logic. The next decision is whether the synthetic-user experiment is stable and informative enough to justify a separate, narrowly scoped change to the participant interaction scripts.

## 11. Required implementation report

Report:
- source commit and worktree state;
- exact changed files;
- design choices and parameter ranges;
- test/build commands and exit status;
- smoke, training, and validation commands/seeds;
- checkpoint and executable provenance;
- before/after metrics with denominators;
- known limitations and any unexpected behaviour;
- a recommendation: proceed to scripted participant-interaction changes, revise the synthetic user, or stop and retain the current interaction design.
