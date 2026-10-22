# End-to-End Companion Loop and Evaluation

**Branch:** \`feat/end-to-end-companion-loop\`  
**Status:** Source changes committed; local build, UI walkthrough, and multi-seed run still require verification.  
**Purpose:** Turn the existing scaffolding into one demonstrable interactive path and a reproducible synthetic training/evaluation workflow.

## Interactive runtime path

The SFML UI sends preset controls and typed commands through the same interaction queue. The runtime path is:

1. Convert the user command into an \`EnvironmentObservation\`. Typed input uses the persistent NLP adapter when available and a local keyword fallback otherwise. Explicit preference statements are stored before action selection.
2. Each policy selects an action from its current state. The proposed policy's state construction can use persisted semantic memories and affective state accumulated from previous interactions.
3. Execute each action in a paired environment clone. Both clones receive the same scenario initially; action-caused task/proximity changes may subsequently diverge between the clones. Fetch attempts are stochastic, approach changes proximity, rest/eating change energy, and play consumes the available play affordance.
4. Compute reward from context fit, task outcome, drive relief, action effort, satisfaction, and explicit user feedback. Praise and Scold supply direct feedback signals; ordinary commands do not receive fixed per-button rewards.
5. Pass the outcome and next observation into the agent. Q-learning updates are enabled in the interactive UI. The proposed agent updates semantic outcome memory and affect; these updates inform later action selection.
6. Render both action animations and append an interaction record with each agent's observation, action/action ID, reward, state keys, energy/stress/satisfaction outcomes, memory count/candidates, and before/after affect.
7. Persist both Q-tables under the policy directory. The proposed agent also persists memory and affect sidecar files. Session metadata and JSONL interaction records are written under the simulation output directory.

The **Repeat Last** control replays the latest observation. Repeating an active fetch request continues that agent's own task state, allowing an approach/fetch sequence to be demonstrated. It does not guarantee the greedy action will change on every replay; the demonstration should show state, memory, affect, action and reward changes rather than assert a particular action change in advance.

## Training design

### Scenario sampling and transitions

Training episodes shuffle a 14-scenario catalogue instead of using a fixed position-to-scenario cycle. If an episode is longer than the catalogue, the catalogue is reshuffled for the next block. Both agents receive the same ordered observations in each paired episode.

The environment contains action-dependent transitions: fetch may succeed or fail probabilistically, approach changes target proximity, rest/eating restore energy, and play consumes its affordance. The same canonical transition outcome controls task completion and the corresponding completion bonus. A far target is reachable but has higher effort and lower fetch success probability; an unavailable target is not a valid fetch action.

Each experiment run samples one latent simulated-user profile. Its praise responsiveness, playfulness, noise sensitivity, rejection sensitivity and response rate affect stochastic positive/neutral/negative feedback. The profile is hidden from the policy observation and shared by the baseline/proposed agents for paired comparisons. Per-step logs include the sampled feedback class and reward diagnostics. The profile is held stable across episodes in a run so the proposed agent's persistent memory is not mixing unrelated synthetic users.

### Held-out evaluation

\`--validate\` and \`--compare\` load saved policies, disable Q-learning updates and exploration, use a separate environment seed, and sample a held-out generator. The held-out generator deliberately includes joint observation combinations absent from the training catalogue, including:

- an active task with an unavailable target;
- a far task with negative user status and food available;
- a disturbance with an active task and both food/play affordances;
- a negative social context with a pending task and play available.

This is a small held-out combination suite, not a claim of comprehensive coverage of all possible states. The self-tests check that these combinations are emitted.

## Metrics and oracle scope

The experiment summary records:

- mean reward per step and task completion rate for each agent;
- per-episode rewards, action counts, visited-state counts and stimulus-alignment diagnostics;
- policy agreement with a one-step exact Q* oracle, with per-state valid-action Q* values written to the step JSONL;
- reward components/diagnostics, including stochastic user-feedback class;
- transition/schema versions, seeds, and hyperparameters.

**Oracle limitation:** The exact oracle is for a separate one-step diagnostic MDP whose next state is terminal. In that diagnostic, each Q*(s,a) is the analytically expected immediate reward under the reward and stochastic-feedback model. Policy agreement is therefore a useful local action-ranking diagnostic, but it is not the optimal policy for the full multi-step environment. Multi-step reward and learning curves must be reported separately.

Run the multi-seed harness after building:

\`\`\`sh
python3 scripts/evaluate_multiseed.py --binary ./build/IS-2 --seeds 10 --episodes 200 --eval-episodes 40 --steps 40 --out artifacts/multiseed-evaluation
\`\`\`

It trains fresh policies for each of ten seeds, evaluates on held-out combinations without Q-learning updates or exploration, and reports the mean and sample standard deviation across seeds. It also writes aligned per-episode learning curves and a validation-approach paragraph.

## Justified validation approach

Validation combines mean reward per step, task completion rate, learning curves, and policy agreement with an exact one-step Q* diagnostic. Mean reward per step summarises the synthetic interaction objective without allowing longer episodes to dominate; completion rate checks whether the agent actually completes tasks; learning curves show whether performance changes with training; and oracle agreement checks whether the learned action ranking matches the exact immediate-reward optimum in the bounded diagnostic. The multi-step held-out evaluation checks transfer to unseen combinations of observable context, while paired user profiles and seeds reduce avoidable comparison noise. The synthetic reward is a controllable engineering signal built from action-context fit, task outcomes, effort, drive relief, and noisy simulated-user feedback. It does not measure human-perceived believability, establish a realistic psychological model, or replace participant ratings and behavioural analysis.

## Verification status

The source-level self-tests added in this branch cover stochastic feedback response classes and the presence of held-out joint contexts, alongside existing action-dependent transition, fetch retry/completion, and stable action-ID tests. A successful local build, full self-test run, smoke training/evaluation, ten-seed evaluation, and SFML walkthrough remain required before describing the implementation as verified.
