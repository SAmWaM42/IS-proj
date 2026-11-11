# Animal Agent — Research and Development

This branch (`Dev`) contains the researcher/development implementation: agent and simulation source, training, evaluation, behavioural diagnostics, and researcher-side questionnaire analysis.

It is **not** the participant release branch. The participant application is maintained on [`deployment/participant-client`](https://github.com/SAmWaM42/IS-proj/tree/deployment/participant-client). The collection API and download redirect service are maintained in [`SAmWaM42/IS-proj-web`](https://github.com/SAmWaM42/IS-proj-web).

> **Documentation authority:** this README describes Dev. See [Current implementation map](Docs/00-current-implementation-map.md) for cross-branch boundaries and verification status, and [Implementation status and roadmap](Docs/07-implementation-status-and-roadmap.md) for researcher milestones. Older numbered specifications retain design history and are not, by themselves, proof that every proposed detail is implemented.

## Research purpose

The project investigates how persistent semantic memory and affective-state modelling influence the perceived believability of a simulated companion animal. It is not a general-purpose chatbot or a physical robot.

- **Baseline:** tabular Q-learning using the shared environment/action contract without the proposed persistent semantic-memory and affect augmentation.
- **Proposed:** tabular Q-learning augmented with memory- and affect-derived state/context.

Participant-perceived believability is the primary human-study outcome. Behavioural consistency, adaptability and contextual appropriateness are supporting outcomes only where explicitly measured. Synthetic reward, Q-table support, coverage and action counts are engineering diagnostics, not believability scores.

## What belongs on Dev

- C++ agent, state/action, simulation and researcher-mode implementation.
- Tabular Q-learning training and policy generation.
- Randomized scripted training scenarios and paired baseline/proposed experiments.
- Memory retrieval/ranking, affect updates and persistence experiments.
- Training/validation diagnostics, per-decision traces and offline dashboard generation.
- Researcher-side study-data retrieval and questionnaire JSON analysis/visualisation.
- Development tests and implementation specifications.

The active training setup uses randomized scripted scenarios. Synthetic-user training experiments and earlier training designs remain in documentation/history but are not the current default training path. Check the current runner and status tracker before treating an older training specification as current.

## Architecture overview

```text
C++ simulation / researcher runner
  ├─ Environment observations and action constraints
  ├─ Baseline Q-learning agent
  ├─ Proposed agent
  │    ├─ Semantic memory and retrieval/ranking
  │    ├─ Affective state and bounded summaries
  │    └─ Persistence / session continuity
  ├─ Shared scenario and reward/transition handling
  └─ Structured training and evaluation traces

Python NLP service (spaCy + deterministic fallback)
  └─ newline-delimited JSON process boundary

Researcher utilities
  ├─ training dashboard from run summaries and JSONL traces
  ├─ Google Drive questionnaire submission downloader
  └─ questionnaire JSON validation, pairing and descriptive analysis
```

The baseline must remain free of the proposed agent's persistent semantic-memory and affect features.

## Build and test

The project uses C++17 and CMake. The participant/development runtime uses SFML 3.0. Dev also contains Python-based NLP and researcher utilities.

```bash
cmake -S . -B build
cmake --build build --parallel
```

Run the self-tests and CTest targets documented for the current revision before relying on a result. Some older specifications mention earlier CMake options or targets; consult the current `CMakeLists.txt` and `build.sh` rather than copying stale commands blindly.

The NLP interaction service is a persistent Python process using newline-delimited JSON. Stdout is reserved for protocol responses; diagnostics belong on stderr. The intent adapter supports spaCy with a deterministic keyword fallback when the model is unavailable.

## Training and diagnostics

Training outputs and policies are written under `artifacts/training/`. The active training runner uses randomized scripted scenarios and paired conditions. Per-decision diagnostics include selected action, valid actions and Q support, energy/reward/repetition information, INSIST eligibility, and proposed-agent memory/affect context where available. Baseline memory/affect diagnostic fields are explicitly not applicable.

Generate the offline dashboard after producing compatible training/validation artifacts:

```bash
python3 scripts/visualize_training_dashboard.py \
  --input-dir artifacts/training \
  --output artifacts/visualizations/training_dashboard.html
```

The dashboard is a researcher diagnostic. It does not measure perceived believability and must not be presented as a participant outcome.

## Researcher questionnaire data

1. The participant app submits questionnaire response envelopes to the web API.
2. The web API stores JSON submissions in the configured Google Drive folder.
3. The researcher downloads submissions locally using the researcher utility.
4. The analysis utility validates envelopes, pairs participant sessions/conditions and produces descriptive summaries/visualisations.

Downloaded participant data is stored outside the repository by default. Do not commit participant responses, credentials, OAuth material, tokens or generated private datasets.

## Related branches and repository

| Location | Responsibility |
|---|---|
| `Dev` | Researcher-side simulation, agent development, training, evaluation and analysis. |
| `deployment/participant-client` | Participant desktop app, guided study, questionnaires, local queue, runtime packaging. |
| `IS-proj-web` | HTTPS submission API, participant authentication, Google Drive storage and release-download redirects. |
| `main` in this repository | Cross-project overview, pipeline, build configuration and toolchain summary. |

Runtime changes affecting participant behaviour must be deliberately ported to the participant branch. Do not merge researcher-only training/analysis code into the participant build. Do not change production Vercel configuration without explicit approval.

## Research safeguards

- Never commit participant credentials, bearer tokens, token maps, Google OAuth secrets, refresh tokens or private response data.
- The common participant ZIP must not contain participant-specific credentials.
- Send questionnaire responses only; do not send raw interaction text, memory/affect traces, Q-tables or training diagnostics to the collection API.
- Keep synthetic reward and engineering diagnostics separate from human believability outcomes.
- Do not collect real participant data until the protocol, questionnaire, endpoint, packaging and end-to-end recovery path are approved and verified.


## Documentation index

The [current implementation map](Docs/00-current-implementation-map.md) explains branch ownership. The [specification status register](Docs/99-specification-status-register.md) classifies every numbered specification and the unnumbered study documents as implemented, superseded, experimental, proposed or mixed. Older documents are retained for design history; use the register to identify the current operational source of truth.
