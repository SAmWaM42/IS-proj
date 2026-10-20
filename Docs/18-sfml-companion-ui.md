# SFML Companion Simulation UI

Status: initial implementation committed; compilation and interactive verification still required.

## Source wireframes

The implementation follows Chapter 3, Section 3.5.3 of the project report and its three screens:

- Main Simulation (Figure 4.6): environment view, interaction controls and text input.
- Agent State (Figure 4.7): affective values, current context and relevant memory records.
- Comparison (Figure 4.8): baseline/proposed responses and interaction history.

## Implementation

- `Components/Simulation/companion-ui.h/.cpp`: interface and SFML event/render loop.
- `main.c++`: `--sim` launches the new interface; `--legacy-sim` preserves the previous scripted window.
- `CMakeLists.txt`: includes the new UI source and include directory.
- `README.md`: run instructions and limitations.

The baseline and proposed agents receive the same `EnvironmentObservation` for each interaction. The UI displays both selected actions. The Agent State view shows the proposed agent's current affective variables and recent in-session memory records with relation, confidence and context scope. The Comparison view reports current actions, interaction counts, reward signals and memory counts.

## Current limitations

1. The free-text input currently uses a small keyword-based intent mapper, not the spaCy NLP service. The displayed interpretation should not be treated as general natural-language understanding.
2. Preset interactions use fixed development reward signals. These are debugging stimuli, not participant-measured feedback or a validated reward model.
3. The interface saves session summaries and Q-table copies to `artifacts/simulation/`; it does not yet restore the proposed agent's complete episodic memory and affect state when the UI restarts.
4. The visual environment is a simple schematic yard. Action selection is visible, but there is not yet an animated action-specific world transition.
5. The participant-study screen and its questionnaire remain separate under `--study`; do not substitute the free simulation for the approved study procedure.

## Acceptance checks

After building locally:

- [ ] `cmake -S . -B build && cmake --build build` completes without errors.
- [ ] `./build/IS-2 --self-test` passes.
- [ ] `./build/IS-2 --sim` opens and closes cleanly.
- [ ] Navigation switches among Simulation, Agent State and Comparison.
- [ ] Each preset interaction updates both agents and adds an activity log.
- [ ] Text entry and Enter/Send produce a response.
- [ ] Agent State displays affect and recent memory after proposed-agent interactions.
- [ ] Comparison shows both responses to the same event.
- [ ] Closing the window writes `artifacts/simulation/simulation-session.json` and Q-table copies.

A successful source write is not evidence that these checks have passed; run them locally before presenting the interface as verified.
