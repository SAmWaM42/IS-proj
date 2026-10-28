# Companion Agent Project

## Overview

This project investigates a companion agent that learns from repeated interaction and can later incorporate persistent memory and affective state into its decision-making.

The current work is intentionally staged:

1. Establish a minimal, validated baseline learning loop.
2. Formalise the shared state and action contracts.
3. Add minimal memory and affect scaffolding.
4. Integrate these components only after the baseline is stable and testable.

The project is not intended to be a general-purpose chatbot or a large-scale LLM application. It is a bounded research prototype focused on agent behaviour, learning, and perceived believability under controlled conditions.

---

## Project scope

The system is designed around the comparison between two experimental agents:

- Baseline agent: learns from environmental observations and a fixed action space without advanced memory or affect integration.
- Proposed agent: augments the baseline with semantic memory, affective-state variables, and memory-derived features.

Both agents share the same environment and action definitions during the first experiment so that behavioural differences can be attributed to the memory/affect augmentation rather than to different underlying mechanics.

---

## Current status

Development status: Interactive SFML MVP + headless evaluation tools + participant-study prototype

The project has reached a demonstrable minimum viable prototype for the current sprint. The interactive SFML simulation builds and runs locally, supports A/B companion selection, preset and typed interactions, animated scene feedback, session logging, and an optional researcher/debug view. The underlying project also retains the headless training, validation, comparison, memory/affect, and persistence work. This is an engineering milestone, not yet evidence that the proposed agent is perceived as more believable.

Verified milestones:

- A minimal C++ baseline Q-learning loop has been implemented and validated.
- Shared state/action contracts are formalised and documented.
- The 14-action vocabulary, observable-context action mask, food/play affordances, and shared animation mapping are documented in [Docs/20-stage-minus-one-action-architecture.md](Docs/20-stage-minus-one-action-architecture.md).
- Memory and affect scaffolding was added as an explicit staged extension point.
- Bounded memory/affect state integration has been implemented without destabilising the baseline policy.
- Live memory retrieval, ranking, and state summarisation are in place.
- Persistence and rest-state aggregation are implemented for Q-table, memory records, affective state, session context, interaction history, and low-confidence aggregated facts.
- The headless `--compare` workflow compares the trained baseline and proposed policies using shared scenario inputs.
- A headless synthetic training/evaluation runner is available through `--train`, `--validate`, and `--compare`; it writes mode-specific per-step JSONL logs and episode summaries.
- A keyboard-driven participant-study prototype is available through `--study`, with randomized A/B condition order, fixed shared scenarios, participant feedback, and a separate questionnaire.
- The interactive SFML MVP is available through `--sim`; `--sim-researcher` enables the separate researcher/debug screens. The participant-facing mode hides internal action labels and memory/affect diagnostics.
- The simulation UI now includes a backyard scene, companion sprite animation, preset interaction controls, typed input via the persistent NLP service (with keyword fallback), an activity log for researcher mode, and session/interaction JSON logging.
- The interactive UI and its missing method definitions have been restored to the current branch; the project owner confirmed that the local build and launch work for the sprint demonstration.
- Terminal transitions no longer bootstrap from next-state Q-values, and policy exploration uses a per-agent seed derived from the experiment configuration.
- A machine-readable validation report is stored in [Docs/test-results.json](Docs/test-results.json).

The current implementation deliberately keeps the policy layer stable and explainable while the runtime, memory, and affect pipeline are exercised under controlled conditions. The UI is suitable as a sprint demonstration MVP; the full study protocol, experimental validity, and believability outcomes still require separate validation.

---

## Headless experiment commands

Build the project with the existing CMake setup, then run:

```sh
./build/IS-2 --self-test
./build/IS-2 --train --episodes 1000 --steps 40 --seed 42 --out artifacts/training
./build/IS-2 --resume-training --episodes 500 --steps 40 --seed 43 --out artifacts/training
./build/IS-2 --validate --episodes 200 --steps 40 --seed 43 --out artifacts/training
./build/IS-2 --compare --episodes 200 --steps 40 --seed 44 --out artifacts/training
```

Training saves `baseline.qtable` and `proposed.qtable`. `--train` starts from fresh Q-tables; `--resume-training` loads existing tables and continues learning. Before replacing a result, the runner preserves the previous version with a `compare_` filename prefix. Comparison-mode results use `compare_previous_` for their previous snapshot to keep the filenames unambiguous. Each output type therefore retains one current version and at most one previous version. Use `artifacts/training` as the single canonical directory for policies, training, validation, comparison, and diagnostic outputs.

The policy state includes task availability, discretized signal strength, explicit food/play affordances, and shared physical energy. Both agents observe the same energy level and include its low/medium/high category in the policy state; the proposed agent additionally uses it to update its affective energy category. Both agents use the same 14-action vocabulary and observable-context mask; invalid actions are excluded from selection and Q-value bootstrapping. The synthetic reward is also intent-conditioned (social greeting, explicit request, or disturbance/rest).


### Training report visualizations

After rebuilding and generating fresh training/validation/comparison artifacts, create plots and a compact coverage report with the Python helper:

```sh
# Install once into the active project virtual environment.
python -m pip install matplotlib

# Run from the repository root after the C++ experiment modes.
python scripts/visualize_training.py --input-dir artifacts/training --output-dir artifacts/visualizations
```

The script reads the C++ JSON/JSONL artifacts and writes:

- `reward_per_episode.png`: total reward per episode for both agents, with rolling-mean trends.
- `state_visitation.png`: grouped bars comparing encoded-state visit counts for both agents.
- `mean_reward_by_mode.png` and `mean_reward_by_mode.csv`: mean total reward per episode for available `train`, `validate`, and `compare` summaries. These are engineering checks, not measures of believability.
- `early_vs_late_training_reward.png` and `training_window_summary.csv`: compares the first and final training windows (default 100 episodes each), including mean episode reward, standard deviation, and min/max. This separates early learning from late-training performance rather than relying on a single final episode.
- `final_episode_behaviour_trace.png` and `final_episode_behaviour_trace.dot`: the final training episode's context → action → reward trace for each agent. The DOT file can be rendered with Graphviz if installed.
- `policy_coverage.png`, `state_action_visitation.png`, and `coverage_summary.json`: decision-level availability of stored valid Q-values, fallback frequency, unique state/action pairs, and the most-visited state/action pairs.
- `sleep_lay_active_request_diagnostics.csv` and `.json`: extracts `SLEEP_LAY` decisions while a request is marked active, with energy, proximity, food/play availability, Q-table coverage, fallback, proposed-agent Q-values, and override details when available. These are investigation candidates, **not automatically classified as errors**. Check energy restrictions and action availability before changing policy behaviour.

The C++ runner writes detailed per-step JSONL records only for the final two episodes of each run by default, to keep artifacts compact. Episode summaries still retain reward, step counts, coverage counts, and per-episode state/action visitation for all episodes. The state/action visitation plots and sleep-action diagnostic extracts therefore describe only the retained final episodes; `coverage_summary.json` and the early/late CSV use the full episode summaries. The coverage and proposed-agent decision-path fields (pre-override action, final action, valid-action Q-values, and preference/rejection override flags) are emitted by the current C++ source. Rebuild and rerun `--train` to regenerate the compact step logs and refresh the summaries before interpreting results. Pass `--training-window 100` (or another positive episode count) to `scripts/visualize_training.py` to choose the early/late window size. Total episode reward can be affected by episode length, so interpret it alongside mean reward per step and steps per episode in the experiment summaries. Reward and stimulus-alignment summaries are synthetic engineering diagnostics; they are not measurements of perceived believability. These plots are intended to support debugging and model comparison, not to replace held-out evaluation.

Important: these state-encoding changes make previously saved Q-table keys incompatible with the current policy. **Retrain both agents before validation or running the participant study**. The headless runner uses repeated contextual scenarios and action-dependent synthetic environment transitions. During training, each non-terminal decision is followed by a seeded interval sampled uniformly from 0.5 to 5 simulated seconds; paired agents receive the same intervals. Both agents share physical energy dynamics: action effort costs plus passive drain of 0.0025 energy units per simulated second (0.0008 while resting). Below 35% energy, recovery receives increasing reward and non-recovery actions are penalised; at or below 20%, both policies are restricted to available recovery actions (eat when food exists, otherwise sleep). If energy reaches zero, the transition receives a terminal −1 reward and does not bootstrap from a next state. The interactive simulation likewise stops actions and ignores further interaction after depletion until a new session is started. The proposed agent also applies feedback and affect decay over that interval before constructing the next state for its Q-learning update. This interval range is an engineering assumption, not a calibrated biological timescale. Training logs include elapsed simulation time and post-transition affect values for debugging policy collapse. No raw global-clock feature is added to the Q-table; the state already includes a discretized affect trend. Treat this as a synthetic transition model, not evidence of long-horizon planning.

These are synthetic engineering diagnostics only. The reward model does not measure believability. The checked-in `Docs/test-results.json` predates the Stage −1 source edits and must be regenerated by running `--self-test`; build and test the branch before relying on generated results. Run a focused fixed-context idle-policy diagnostic after training with `./build/IS-2 --diagnose-idle --policy-dir artifacts/training --out artifacts/training --seed 42`. It samples the UI-like ambient context at 0, 1, 5, 10, and 30 simulated seconds, writing proposed-agent affect, encoded states, fallback/policy/final actions, valid-action Q-values, memory/override information, and a summary to JSON/JSONL. This isolates state discretisation and policy/override effects under affect decay; it does not reproduce exact animation completion timing or measure believability. For a broader paired sequence test, run `./build/IS-2 --diagnose-temporal --policy-dir artifacts/training --out artifacts/training --seed 42`. This holds ambient, social-contact, disturbance, quiet/no-stimulus, play-invitation, and return-to-ambient phases for six decisions each (five simulated seconds between decisions), logging per-phase action distributions, action changes, proposed affect, Q-table coverage, fallbacks, and overrides. It disables exploration and policy learning and does not apply reward/outcome feedback, so it isolates observation changes plus proposed-agent time-dependent affect decay rather than full environment transitions. For an actual runtime trace, launch `./build/IS-2 --policy-dir artifacts/training --sim-researcher` and let the simulation run idle; it appends decisions made after the real behaviour-completion/5-second-idle loop to `artifacts/simulation/idle-runtime-diagnostics.jsonl`. Each row records elapsed time, actual ambient observation, both agents’ actions, and the proposed policy’s state, affect, override path, and valid-action Q-values. The runtime trace is only written in researcher mode.\n\nSimulation outputs are also rotated on launch: the previous session's files receive a `compare_` prefix, and older comparison files are removed. Participant-study data remains local and git-ignored.

### Interactive SFML simulation UI

Launch the participant-facing companion simulation interface:

```sh
./build/IS-2 --sim
```

The UI follows the Chapter 3.5.3 wireframes. The default participant-facing mode presents an A/B companion selection and simulation without exposing internal action labels, memory, affect, or comparison statistics. The simulation includes a backyard scene, animated companion behaviour, preset interactions, hover descriptions for scene objects, and text input. The optional researcher mode exposes agent state and comparison screens for debugging.

The participant-facing Answer Form button opens the configured questionnaire URL. Set it in the environment before launching the UI (do not hard-code participant URLs into source control):

```sh
export ANIMAL_AGENT_STUDY_FORM_URL="https://your-approved-questionnaire-url"
./build/IS-2 --sim
```

If the variable is unset or invalid, the application logs a message instead of opening a placeholder URL. Launch `./build/IS-2 --sim-researcher` to enable researcher mode; the default `--sim` launch intentionally uses participant-facing mode.

The text input is routed through the persistent spaCy NLP service when available, with a C++ keyword fallback. It recognises fetch/request, praise, disturbance, rest, negative feedback, feeding, and play contexts. Preset controls use fixed development reward signals, so this UI is for simulation and debugging, not a substitute for the controlled participant-study protocol. The interface saves a session summary and session Q-table copies under `artifacts/simulation/`. Explicit general preferences are stored without forcing an immediate action; the proposed agent may use a preference only when the later context is relevant (for example, a dislike of sound during a loud disturbance). Such general preferences do not override urgent affective needs or a directly matching explicit fetch request. Interaction logs include general semantic preference records among the proposed agent's memory candidates.

### NLP-enabled simulation and dog sprites

The SFML `--sim` interface sends typed messages to `Components/NLP/nlp_intent_service.py`. The adapter uses spaCy's `en_core_web_sm` lemmatization when installed and reports the selected method in the simulation view; a keyword fallback is used if Python, spaCy, or the model is unavailable.

Set up the optional NLP dependency in a virtual environment:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install spacy
python -m spacy download en_core_web_sm
```

The UI launches `python3` by name, so activate the intended virtual environment before starting the application. The scene uses the supplied dog pose frames from `Data/Assets/white_dog_by_Brysia/` and the backyard atlas from `Data/Assets/free version/free.png`. If pose frames are missing, the UI reports this in the activity log; verify that these asset paths exist in your checkout. The NLP intent mapper still emits a small set of discrete intents and fixed development reward signals; it is not a general dialogue model or a validated participant-feedback mechanism.

### Participant study prototype

After training the policies, launch the SFML study interface:

```sh
./build/IS-2 --study --policy-dir artifacts/training --study-out artifacts/study
```

The interface presents two blinded conditions (A/B) in random order, uses the same five scenarios for both, records positive/neutral/negative/no-response feedback, and asks four 1–5 ratings after each condition. Policy learning is disabled during participant sessions; feedback can update the proposed agent's affect and memory state, while the questionnaire ratings remain separate from the agent's reward signal.

Participant data is written as an anonymous session JSON file under `artifacts/study/`, which is git-ignored. The file includes the condition key for later analysis, so treat it as restricted research data. A recorded "no response" is treated as missing feedback: the trial is retained in the dataset, but it does not update either agent's state, affect, or memory. Neutral feedback remains a distinct observed response. This prototype does not replace supervisor/institutional ethics approval, an approved consent process, or a piloted questionnaire. Do not collect real participant data until those requirements are met.

## Architecture

The project separates responsibilities into bounded components:

```text
User / Interaction
      |
      v
NLP Client (Python + spaCy)
      |
      v
Memory Parser / Memory Layer
      |
      v
Affective State
      |
      v
State Builder
      |
      v
Q-learning Policy
      |
      v
Simulation Environment / Reward Loop
```

The main architectural rule is that the learning loop is kept separate from NLP, memory, and affect details until those interfaces are formally validated.

---

## Core design principles

- Prefer working, testable increments over architectural complexity.
- Keep the Q-table state bounded and deterministic.
- Use discrete action and observation categories during the first prototype.
- Treat memory and affect as explicit, structured inputs rather than hidden system behaviour.
- Validate each stage before moving to the next.

---

## Component overview

### NLP boundary

The project uses a Python spaCy-based tokenizer process communicating with the C++ application through a lightweight stdin/stdout interface. This boundary was validated with a minimal C++ harness and is intended to supply structured language-processing results without making the decision layer directly dependent on Python internals.

### Environment and state

The environment exposes small, bounded observations such as object proximity, interaction status, and relevant task state. These are converted into discrete categories before entering the Q-table.

### Learning loop

The baseline uses tabular Q-learning with a fixed action space and a compact state representation. The Q-table updates based on observed rewards and the best value available in the next state.

### Memory subsystem

The memory layer stores structured facts, confidence, generalisation, source, scope, and relevance metadata. This allows a future retrieval process to select records that are helpful in the current context without overloading the decision system with raw historical data.

### Affective state

The affective layer tracks bounded variables such as valence, energy, arousal, engagement, stress, and affinity. These are designed to provide structured inputs for future state construction without yet directly changing the baseline policy.

---

## Minimal memory model

The project currently defines a minimal memory contract intended for future retrieval and state construction.

### `fact`

```cpp
typedef struct {
    std::string subject;
    std::string relation;
    std::string object;
} fact;
```

A `fact` captures the underlying semantic relationship, for example:

```text
user -> likes -> fetch
agent -> observed -> user_playing_fetch
```

### `record`

```cpp
struct record {
    fact raw_fact;
    float confidence = 0.0f;
    float generalization_factor = 0.0f;
    enum user_context { GENERAL, USER_SPECIFIC };
    user_context current_context = GENERAL;
    std::vector<std::string> relevant_users;
    enum source_type { USER_STATEMENT, OBSERVATION, ACTION_HISTORY, INFERENCE };
    source_type source = OBSERVATION;
    int observation_count = 0;
    std::string last_updated;
    float relevance = 0.0f;
};
```

A `record` augments the raw fact with metadata describing how reliable, general, and context-specific it is.

### `action_log`

```cpp
class action_log {
public:
    std::string action_taken;
    std::string time;
    float count = 0.0f;
};
```

Action logs provide a historical record of what the agent has done, which can later be used to update memory or influence behavioural patterns.

### `cache`

```cpp
class cache {
public:
    std::vector<ex_record> data_store;
};
```

The cache is a lightweight optimization layer for reusing already extracted memory information.

### `AffectiveState`

```cpp
struct AffectiveState {
    AffectValence valence = AffectValence::NEUTRAL;
    AffectEnergy energy = AffectEnergy::MODERATE;
    float arousal = 0.5f;
    float engagement = 0.5f;
    float stress = 0.5f;
    float affinity = 0.5f;
};
```

These values are intentionally small and bounded so they can be tested cleanly before being integrated into the state representation.

---

## Build and validation

The project uses CMake as the build entry point.

```bash
cmake -S . -B build
cmake --build build --target IS-2
./build/IS-2
```

The tokenizer boundary is also validated with a dedicated C++ harness:

```bash
cmake --build build --target tokenizer_probe
./build/tokenizer_probe
```

Current validated baseline checks include:

- basic Q update
- unseen state handling
- persistence
- terminal handling

The current baseline validation passes successfully.

---

## Current development focus

The repository has moved past the initial baseline-only stage and is now focused on the comparison and runtime validation layer.

The immediate work after the sprint demonstration is:

1. Run and record a clean build, self-test, CTest suite, and NLP smoke test from the current merged revision.
2. Review the interaction JSONL and session outputs to confirm the UI produces usable, reproducible research traces.
3. Audit baseline/proposed fairness, especially which environment variables are observable to both agents and how contextual fallbacks affect action selection.
4. Verify memory retrieval scoring and that query relevance influences the memory summary, not only record ordering.
5. Define and pilot the believability questionnaire and controlled comparison procedure before collecting participant data.
6. Document limitations and preserve the participant-facing separation from researcher diagnostics.

---

## Summary

This project is a staged research prototype for a learning-based simulated companion agent. The current sprint MVP provides a working interactive demonstration around the baseline/proposed-agent comparison, while the memory, affect, NLP, logging, and headless evaluation components provide the basis for further controlled testing. A working demonstration should not be interpreted as validation of perceived believability; that remains an empirical research question.
