# Participant Runtime Parity and Deployment Status

**Branch:** `deployment/participant-client`  
**Scope:** Participant runtime only  
**Related implementation:** `Dev` (researcher/development); `IS-proj-web` (collection and downloads)  
**Last reviewed:** 11 November 2026

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

**Verification qualification:** these integrations are present in source, but the latest questionnaire/submission integration has not yet been confirmed by a complete build, CTest run, interactive study walkthrough, clean-machine package test and live API/Drive end-to-end test. Treat source presence and verified release readiness as separate statuses. Do not collect real participant data yet.

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

The Windows packaging script on this branch is not the same as the newer static-SFML/vcpkg-aware script proposed in PR #29. The PR workflow is on `feature/windows-github-actions`, not merged. The last recorded status for PR #29 was no workflow run and no artifact observed. Do not infer a successful Windows package.

## 7. Release acceptance gates

Before distribution or participant recruitment:
1. Build the participant executable on the target OS.
2. Run the complete CTest suite on that OS.
3. Test the menu, both modes, consent/withdrawal path, counterbalanced conditions and questionnaire sequence interactively.
4. Confirm preference-to-fetch behaviour and baseline isolation.
5. Confirm credentials are loaded from the correct per-participant file and never included in the common ZIP.
6. Test local queue persistence, restart recovery and retry after network failure.
7. Test the configured HTTPS endpoint, token authentication, idempotency and actual Drive persistence with synthetic test records.
8. Extract and launch each ZIP on a clean machine.
9. For Windows, separately validate the GUI and complete participant flow on a real Windows desktop.

A successful CI build/package is necessary but does not satisfy the clean-machine or real desktop GUI acceptance gates.

## 8. Related repositories

- `Dev`: researcher-side agent implementation, training, evaluation and analysis.
- `IS-proj-web`: server API, HMAC-backed participant token authentication, Google Drive storage and release download redirects.
- `main`: project-wide system map and build/pipeline/tooling summary.
