# Paired agent scenario regression tests

## Purpose

`paired_agent_scenarios_test` exercises the baseline and proposed agents against the same fixed observations and deterministic backyard-world transitions. It is an implementation regression test, not a believability study.

The test disables exploration and online Q-learning updates for both agents. It checks that shared, straightforward scenarios remain comparable, that emitted actions are valid for the observations, and that the proposed agent can change its response after negative feedback.

## Scenarios

- **Fetch:** both agents receive the same fetch request. The test expects both to choose `FETCH`; each world then records the ball as collected and increments its completed-fetch count.
- **Noise and feedback:** both agents initially respond to a high-intensity disturbance with `BARK`. The proposed agent receives a negative outcome and is expected to switch to `ACT_CONFUSED`; the baseline receives no feedback update and is expected to retain its initial response.
- **Rest:** both agents receive the same low-intensity observation and are expected to choose `SLEEP_LAY`; both worlds should enter the resting state.
- **Reset:** after changing world state, reset should restore the same initial world state.

## Run

From the repository root:

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The existing `backyard_environment_test` remains registered alongside this test.

## Interpretation and limits

These scenarios verify selected implementation contracts under fixed inputs. They do not establish that the proposed agent is generally more adaptive, nor that users perceive it as more believable. In particular, the paired scenarios intentionally check equal starting behaviour where the same context should produce a shared action; the negative-feedback scenario checks a specific adaptation path.

For research evaluation, keep scenario outcomes and event traces separate from participant ratings. Believability should be measured independently using a defined questionnaire or rating procedure, with the same scripted scenarios, counterbalanced agent order, and equivalent interaction opportunities across conditions. Record memory and affect changes so observed differences can be interpreted rather than inferred from the final action alone.
