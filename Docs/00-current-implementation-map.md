# Current Implementation Map

**Last reviewed:** 11 November 2026  
**Purpose:** Cross-branch source-of-truth index. This records source implementation separately from verification status; it does not replace detailed design contracts.

## Repository responsibilities

| Location | Responsibility | Documentation authority |
|---|---|---|
| `IS-proj/Dev` | Researcher-side agent/simulation development, training, evaluation, diagnostics, questionnaire retrieval and analysis. | Branch README, this map, `Docs/07-implementation-status-and-roadmap.md`. |
| `IS-proj/deployment/participant-client` | Participant desktop app, guided study, questionnaires, local queue/uploader, assets and platform packaging. | Branch README, `Docs/24-participant-runtime-parity-update.md`. |
| `IS-proj-web/main` | Vercel questionnaire API, HMAC participant authentication, Google Drive persistence, GitHub Release download redirects. | Web repository README. |
| `IS-proj/main` | Project-level system map, pipelines, dependencies, build/package configuration and repository boundaries. | Root README on main. |

## Dev: researcher implementation

### Implemented in source

- C++ agent/simulation and tabular Q-learning baseline/proposed comparison.
- Proposed-agent semantic memory and affective-state mechanisms, including bounded state summaries, retrieval/ranking and persistence paths.
- Researcher-side randomized scripted training scenarios and paired-agent diagnostics.
- Per-decision training traces and offline dashboard generation in `scripts/visualize_training_dashboard.py`.
- Researcher-side questionnaire submission retrieval and questionnaire JSON analysis/visualisation utilities.
- Python NLP service boundary using newline-delimited JSON; spaCy intent classification with deterministic fallback.

### Important distinctions

- The active training path uses randomized scripted scenarios. Older synthetic-user and earlier training specifications are historical/experimental unless the current runner explicitly invokes them.
- Training reward, action counts, Q-table support and dashboard plots are engineering diagnostics, not participant-perceived believability.
- Researcher training/analysis tools are not part of the participant runtime package.
- Runtime behaviour changes that affect participant experience must be reviewed and selectively ported to the deployment branch.
- Source integration does not prove the current revision has been built, tested, trained or validated. Use the exact status tracker and run artifacts.

## Participant branch: deployment implementation

### Implemented in source

- Single CMake executable, `AnimalAgent`, with General Interaction and Believability Study modes.
- Participant configuration/credentials parsing, participant menu, simulation and guided-study components.
- In-app questionnaire rendering/content and typed submission adaptation.
- Local submission manager/queue and Python HTTPS uploader; pending submissions are retried at startup.
- Baseline/proposed trained policy assets and platform-specific packaging scripts.
- NLP intent classification and preference handoff; proposed-agent fetch-object preference bias among ball, stick and rope, while baseline retains its neutral ball default.
- Bounded no-response state support. The current guided script does not create repeated ignored requests, so INSIST is not eligible in that protocol.

### Verification still required

- Complete build and CTest on the current participant branch.
- Interactive menu, consent/withdrawal, A/B order, guided interaction and questionnaire walkthrough.
- Preference-to-fetch scene validation and baseline isolation.
- Local queue restart recovery and network retry.
- Correct public participant API endpoint and authenticated end-to-end Drive storage using synthetic records.
- Clean-machine Linux and Windows extraction/launch tests.
- Real Windows desktop GUI and participant-flow validation.

Do not collect real participant data or distribute a package as release-ready before the study/protocol and technical gates are complete.

## Windows build candidate (not yet part of deployment branch)

A vcpkg/static-SFML-aware Windows GitHub Actions workflow and revised packaging script are on `feature/windows-github-actions`, proposed in [PR #29](https://github.com/SAmWaM42/IS-proj/pull/29). This branch is separate from `deployment/participant-client` until reviewed and merged. The last recorded check found no workflow run, status check or artifact for its then-current head. A future successful CI artifact will still not replace real Windows desktop GUI/participant-flow validation.

## Web service

The web repository provides:
- `POST /api/submissions`: schema-version-1 envelope checks, deterministic submission ID, odd/even A/B mapping, idempotency-header check, HMAC-SHA256 participant-token authentication and Google Drive JSON storage.
- `GET|HEAD /api/downloads/latest?platform=linux|windows`: redirects to the newest eligible GitHub Release asset; `channel=stable` excludes prereleases.

Current API limitation: validation covers envelope metadata, ID, condition assignment, idempotency header and authentication. It does not yet validate every active questionnaire question ID, answer type, required field, allowed option, scale bound or questionnaire version. Drive lookup-before-create handles ordinary retries but is not atomic under concurrent duplicate requests.

A GitHub Actions artifact is not automatically a GitHub Release asset; the download redirect requires a published release containing the expected ZIP filename.

## Document status and precedence

Numbered `Docs/*.md` files were authored incrementally as staged specifications, implementation reports and experiments. Their status labels and commands are time-specific. Where an older document says a feature is planned, absent or pending, compare it with current source and the latest status tracker. Where an older proposed formula or workflow conflicts with current implementation, do not silently treat the proposal as implemented.

Use this precedence:
1. Current source on the branch being discussed.
2. Current branch README and this cross-branch map.
3. `Docs/07-implementation-status-and-roadmap.md` and participant milestone tracker for status/evidence.
4. Scoped implementation reports and specifications for detailed contracts.
5. Earlier draft/proposal documents for historical rationale only.

Retain older design documents as design history; update their status when revising them rather than deleting their rationale.
