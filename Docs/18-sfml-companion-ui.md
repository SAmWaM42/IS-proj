# SFML Top-Down Companion Simulation UI

Status: integrated source replacement prepared; local build and interactive verification are still required.

## Architecture integration

The top-down UI replaces the existing implementation at:

- `Components/Simulation/companion-ui.h`
- `Components/Simulation/companion-ui.cpp`

It continues to use the existing `CompanionUI` entry point and the current `BaselineAgent` / `ProposedAgent` interfaces from `Components/State-Action/state-action.h`. `CMakeLists.txt` already compiles `Components/Simulation/companion-ui.cpp`, so no build-system source-list change is required.

The interface adds a top-down yard, keyboard-controlled participant position, companion movement/poses, ball and food scene objects, interaction controls, and the existing agent-selection, simulation, agent-state, and comparison screens. It keeps policy learning disabled during interactive use so testing does not silently change the trained policies.

## Assets

The implementation references these existing repository assets:

- `Data/Assets/free version/free.png`
- `Data/Assets/white_dog_by_Brysia/` sprite frames
- `Data/Fonts/UIFont.ttf` with a system DejaVu Sans fallback

These paths were found in the current repository tree. Confirm that the runtime working directory is the repository root when launching the application.

## Integration procedure

1. Extract this archive at the repository root, preserving its directory structure.
2. Review the two replaced UI source files before committing.
3. Build with `cmake -S . -B build && cmake --build build`.
4. Run `ctest --test-dir build --output-on-failure`.
5. Run `./build/IS-2 --sim` and verify the top-down yard, keyboard movement, controls, navigation, agent selection, interactions, and session output.
6. Run `./build/IS-2 --sim-researcher` for researcher-visible diagnostics; do not use researcher mode for participant-facing sessions.
7. Inspect `artifacts/simulation/` after closing the UI.

## Important research limitations

- The visual scene is a presentation layer; do not treat its animations as evidence that the simulated agent has physically realistic behaviour.
- The UI compares both policies against the same interpreted interaction, but the displayed selected agent is randomized between A/B. Preserve the generated run manifest when analyzing condition order.
- Fixed development reward signals and keyword/intent mapping are not validated participant feedback or open-ended dialogue understanding.
- The combined baseline-versus-proposed comparison cannot independently identify the effect of memory versus affect. Use ablations/additional conditions if those individual contributions are research questions.
- The UI is not, by itself, the believability study protocol. Before participant data collection, define the scenarios, instructions, rating instrument, order/counterbalancing, data handling, consent and ethics approval.

## Acceptance checklist

- [ ] CMake build succeeds on the target machine.
- [ ] Existing automated tests pass.
- [ ] `--sim` launches and closes cleanly.
- [ ] Scene assets load without missing-file errors.
- [ ] Player movement stays within the yard; input focus prevents movement while typing.
- [ ] Preset and text interactions update both policies and the activity log.
- [ ] Agent State and Comparison screens remain usable.
- [ ] Session JSON and policy snapshots are written to the expected output locations.
- [ ] A fresh run is inspected to ensure the UI does not unintentionally persist participant-specific transient state.

A source import is not evidence that these checks passed. Record the actual build/test results before treating the UI as ready for study use.
