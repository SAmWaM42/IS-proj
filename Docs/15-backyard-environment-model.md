# Backyard environment model

The SFML simulation now has a small deterministic world-state model in
backyard-environment.h/.cpp. It is deliberately separate from drawing.

## Current world state
- Whether the ball and food are available.
- Whether the companion has the ball or is resting.
- Whether the current interaction represents a disturbance.
- Number of completed fetches and interactions.

## Action consequences
- FETCH collects an available ball and increments the fetch counter.
- SLEEP_LAY marks the companion as resting.
- EAT consumes available food.
- DROP and RETURN_BALL make the ball available again.
- A new stimulus clears the previous transient disturbance, then applies the new stimulus.
- Mentioning a ball or fetch request does not respawn a ball already collected; an explicit throw/offer does.
- The environment can be reset to its initial state for repeatable scenarios.

## Deterministic regression scenarios
The regression test covers fetch and repeated fetch, explicit ball re-offer, loud-noise detection regardless of letter case, disturbance reset, rest, and feeding. These tests validate world transitions independently of SFML rendering. The interaction event log records both agents' chosen actions alongside the selected-condition world state; comparative runs should use the same scenario order and starting conditions.

The simulation draws the ball, food bowl, disturbance marker, and fetch count from
this state. The selected A/B condition alone changes the displayed world, while
both policies continue to receive the same observation and their decisions are logged.

## Known limitations
This is an initial deterministic environment contract, not a full physics engine.
The NLP-to-observation mapping remains in the UI, and most actions do not yet
have meaningful world effects. In particular, there is no spatial collision,
object targeting, time-based energy depletion, or explicit user-response event
after the companion acts. The current model should be built out incrementally
and tested with fixed scenarios before being used for study data.
