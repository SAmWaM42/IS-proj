# Stage −1: Action Architecture and Complexity Reduction

**Status:** Implemented in source; local build/test verification still required.
**Scope:** Establish a shared action vocabulary and context validity contract before Stage 0 training correctness work.

## 1. Canonical action vocabulary

The canonical list lives in `Components/State-Action/action-space.h` and is used by the baseline, proposed agent, training scenarios, study descriptions, and simulation animation mapping.

| Action | Intended role | Validity rule |
|---|---|---|
| `FETCH` | Retrieve the requested nearby object | Task is active and target proximity is close/near (`<= 1`) |
| `WANDER` | Explore or disengage safely | Always available as a fallback |
| `BARK` | Vocalise at a salient disturbance | Signal strength `>= 0.80` |
| `ACT_CONFUSED` | Pause/reorient when uncertain | Always available as a fallback |
| `SLEEP_LAY` | Rest during a quiet, task-free context | Rest/quiet context, no task, signal strength `< 0.80` |
| `NUZZLE` | Affectionate close contact | At zero proximity and user status is not negative |
| `INSIST` | Repeat a behaviour/request | A task is active and the user has not responded (`user_status == 0`) |
| `GREETING` | Initiate friendly contact | New user status, or social context without negative feedback |
| `APPROACH_FOLLOW` | Approach or follow the user | User is not rejecting contact and is not already at zero proximity |
| `SNIFF` | Investigate a nearby object/area | Target proximity `<= 1` |
| `WHINE` | Vocalise distress/appeal | Negative user status or salient disturbance |
| `HIDE_AVOID` | Retreat from aversive context | Negative user status or signal strength `>= 0.90` |
| `EAT` | Eat offered food | `food_available == true` |
| `PLAY` | Engage with an offered play opportunity | `play_available == true` and user status is not negative |

The current observation contract uses `user_status == 2` for negative/rejecting feedback. Food and play availability are explicit observable affordances; they must not be inferred from hidden simulated-user traits.

## 2. Shared validity masking

Both agents select only from the same valid-action list. The same list is used for the next-state bootstrap during Q-learning updates, so invalid actions cannot inflate the estimated future value. The proposed agent's memory-based preference and rejection rules are checked against the mask before an action is returned.

The mask is intentionally a context-feasibility rule, not a personality rule. Affect and memory may change which valid action is preferred; they do not make an impossible action valid. If a context yields no ordinary valid action, `WANDER` is the safe fallback.

## 3. Action-to-animation mapping

The UI uses one shared mapping from action to animation group:

- Walk/explore: `FETCH`, `WANDER`, `APPROACH_FOLLOW`, `SNIFF`, `PLAY`
- Rest: `SLEEP_LAY`
- Vocalise/react: `BARK`, `ACT_CONFUSED`, `WHINE`
- Close/social: `NUZZLE`, `GREETING`, `EAT`
- Retreat/lie down: `INSIST`, `HIDE_AVOID`

These are asset reuse mappings, not claims that the actions are behaviourally identical. Both conditions use the same renderer, so the proposed agent does not receive a special animation layer. The participant study records the selected action and a shared action description.

The UI already serialises queued interactions and displays the selected action for a fixed interval before processing the next queued interaction. This acts as a minimum display duration; new queued inputs do not interrupt the current animation.

## 4. Scenario coverage and feedback

The synthetic training sequence now includes explicit food, play, negative-feedback, and ignored-request contexts in addition to social, fetch, disturbance, rest, and positive-feedback contexts. The participant study includes food-offer, play-invitation, rejection, and ignored-request scenarios. Food/play availability is also encoded into the policy state and preserved across transitions. Observation logs record these flags so contexts are reconstructable. Q-learning bootstrapping uses only valid next actions.

The current synthetic reward values for these new actions are provisional coverage rewards. They are useful for exercising the action vocabulary, not evidence that the model is believable. The broader training redesign in [the training/evaluation specification](19-training-and-evaluation-redesign-spec.md) remains required before research evaluation.

## 5. Complexity reduction

Removed from the active project:

- The disconnected legacy `BackyardSimulation` window and its separate policy/reward/memory path (`Components/Environment/environment.h/.cpp`), which duplicated the current `CompanionUI` and was only reachable through legacy command flags.
- The unused `tab`, `window`, and `simulation_environment` placeholder abstractions that were part of that legacy path.
- The unused `base_action` / `complex_action` class hierarchy.
- The unused `QTable::chooseAction` method and redundant private Q-learning helpers that had no callers.
- The unused `SimulationTick` and duplicate `AgentComparisonRun` harness, superseded by the headless training/validation/comparison runner.
- The no-op `loadTrainingData` method.

The active `--sim`, `--train`, `--validate`, `--compare`, `--study`, and `--self-test` workflows remain. The `tokenizer_probe` target is retained because it is a focused test harness for the C++/Python NLP boundary.

## 6. Verification still required

The changes have been committed to `Dev`, but a local build and `--self-test` run have not yet been performed in this editing session. Before Stage 0 begins, verify:

1. CMake configures and compiles with the installed SFML 3 toolchain.
2. `./build/IS-2 --self-test` passes, including the new observable-action-mask test.
3. `--train` logs the food/play flags and never selects actions masked out for the current observation.
4. The study action labels cover all 14 actions and its logs include the new availability fields.
