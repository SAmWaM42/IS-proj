# Participant Runtime Parity and Deployment Status

**Branch:** `deployment/participant-client`  
**Scope:** Participant runtime only  
**Related implementation:** `Dev` (researcher/development); `IS-proj-web` (collection and downloads)  
**Last reviewed:** 12 November 2026

## 1. Purpose and branch boundary

This branch is the participant-facing runtime. It is a selective implementation, not a wholesale merge of `Dev`. It includes the application menu, ordinary interaction, guided study, questionnaire UI/content, participant identity, submission queue/uploader, runtime assets and platform packaging scripts.

The `Dev` branch remains the source for researcher-side training, synthetic experiments, evaluation, dashboards and analysis. Runtime changes that alter participant behaviour must be reviewed for parity and ported deliberately. The web repository owns the server-side API and release-download redirects. Production deployment changes require separate approval.

## 2. Current application structure

The participant executable is built by CMake as `AnimalAgent` (C++17, SFML 3). Current source includes:
- participant menu and ordinary/general interaction;
- backyard environment and companion simulation;
- participant configuration and per-participant credentials parsing;
- questionnaire rendering and versioned questionnaire content;
- questionnaire submission adaptation and local submission management;
- Python NLP intent service and uploader scripts;
- baseline/proposed trained policy assets and study configuration.

The menu offers General Interaction and Believability Study. The guided study path contains consent and counterbalanced A/B condition handling, guided interactions and in-app questionnaires. Questionnaire completion is adapted to the versioned submission envelope: session 1 follows the condition-A questionnaire; session 2 follows the condition-B questionnaire and final comparison form. Pending submissions are retried at application startup. The code reports server acknowledgement versus locally pending status.

**Verification status — 12 November 2026:** the participant Windows workflow passed automated CI build/test/package and a manual end-to-end test using the downloaded release package on a separate Windows computer without the project's development toolchain or separately installed application dependencies. The manual test confirmed participant login, both A/B questionnaires, distinct session 1/session 2 submission IDs, and persistence of the latest answers in Google Drive. See [Windows manual validation](25-windows-participant-manual-validation.md). This does not establish compatibility with every Windows configuration or replace study/ethics approval.

## 3. Behavioural parity with Dev

The following selected runtime behaviours are present on this branch:
- bounded `no_response_count` in environment observation and encoded state;
- INSIST eligibility only for a pending task after the required non-response observations, subject to critical-energy restrictions;
- deterministic NLP intent categories and preference statement handling;
- proposed-agent user-scoped semantic preference storage;
- fetch-object preference bias among ball, stick and rope, while baseline retains the ball default;
- interaction logging of the selected fetch object.

The guided participant protocol does not currently model repeated ignored requests. Its no-response count therefore remains at zero and INSIST is not eligible in the current guided sequence. Randomized training scenario generation and researcher training controls remain on `Dev`; they are not participant UI features.

The baseline must remain without the proposed agent's persistent semantic preference memory. A preference statement is not itself an action command and must not force an unrelated action.

## 4. Runtime data and privacy contract

- Participant number and private token are loaded from a participant-specific `credentials.json` beside the executable.
- The common package contains no participant credentials or token map.
- Mutable runtime data is stored under the platform's user-data directory.
- Questionnaire submissions use deterministic IDs `P{participant}-S{session}` and the same value as the idempotency key.
- The participant token is sent as an HTTPS Bearer token and removed from the questionnaire JSON body.
- Only questionnaire response data is sent. Do not transmit raw interaction text, memory/affect traces, Q-tables or researcher diagnostics.
- Failed/ambiguous submissions remain in the local queue for retry.

## 5. Build and test contract

The current `CMakeLists.txt` requires CMake 3.16+, C++17, SFML 3.0 Graphics/Window/System and Threads. Windows additionally links `shell32`. Tests are enabled with `-DANIMAL_AGENT_BUILD_TESTS=ON`; the current CMake file registers NLP smoke/classification, backyard environment, participant config, questionnaire submission and questionnaire content tests.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DANIMAL_AGENT_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## 6. Packaging status

The Linux script requires:
- a Python runtime/venv containing spaCy and `en_core_web_sm`;
- `ANIMAL_AGENT_SFML_RUNTIME` with SFML shared libraries;
- a configured HTTPS endpoint in `Data/Study/upload-config.json`;
- baseline/proposed Q-table assets;
- a redistributable font.

The Windows script currently requires:
- `ANIMAL_AGENT_PYTHON_RUNTIME` with `python.exe`, spaCy and `en_core_web_sm`;
- `ANIMAL_AGENT_SFML_DLL_DIR` with SFML runtime DLLs;
- a configured HTTPS endpoint;
- baseline/proposed Q-table assets;
- a redistributable font supplied through `ANIMAL_AGENT_FONT_FILE`.

The Windows GitHub Actions workflow (`.github/workflows/windows-build.yml`) uses MSVC x64 and vcpkg SFML, provisions a standalone Python runtime with spaCy and `en_core_web_sm`, stages the participant executable and runtime assets, and uploads the `AnimalAgent-Windows` artifact. It runs the configured C++ and NLP tests. This workflow and packaging update were merged through PR #29; CI passed and the package was subsequently manually tested on a separate Windows computer.

## 7. Release acceptance and verification record

The following Windows release gates have been completed:
1. Windows CI compiled the participant application and ran the configured C++/NLP test jobs.
2. CI provisioned the standalone Python/spaCy runtime and produced the Windows package.
3. The downloaded package was extracted and launched on a separate Windows computer without the project's development toolchain or separately installed application dependencies.
4. Participant login and participant-number entry succeeded.
5. Both questionnaire conditions (A and B) were exercised.
6. Session 1 and session 2 produced distinct submission IDs.
7. The submission service accepted the test submissions and the corresponding Google Drive JSON contained the latest answers.

The evidence is recorded in [Docs/25-windows-participant-manual-validation.md](25-windows-participant-manual-validation.md). The Windows package is published as `AnimalAgent-Windows.zip` in [release v0.1.0-rc.1](https://github.com/SAmWaM42/IS-proj/releases/tag/v0.1.0-rc.1), alongside `AnimalAgent-Linux.zip`.

These checks validate the tested Windows package and workflow, not every Windows hardware/OS combination. The Linux asset is present in the release, but its inclusion alone does not claim a new clean-machine Linux test in this validation cycle. Study/ethics approval and any remaining protocol requirements remain separate from software validation.

## 8. Related repositories

- `Dev`: researcher-side agent implementation, training, evaluation and analysis.
- `IS-proj-web`: server API, HMAC-backed participant token authentication, Google Drive storage and release download redirects.
- `main`: project-wide system map and build/pipeline/tooling summary.
