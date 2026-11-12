# Animal Agent

Animal Agent is a C++/SFML simulated companion-animal research project investigating whether persistent semantic memory and affective-state modelling influence the perceived believability of a simulated companion.

It is not a general-purpose chatbot and does not currently control a physical robot. The system is split into a researcher/development implementation, a participant-facing desktop client, and a separate web collection/distribution service.

## Repository map

| Repository / branch | Owns | Does not own |
|---|---|---|
| [`IS-proj` — `Dev`](https://github.com/SAmWaM42/IS-proj/tree/Dev) | Agent and simulation development, baseline/proposed Q-learning, memory/affect, training, evaluation, diagnostics, researcher data retrieval and questionnaire analysis. | Participant release packaging and web hosting. |
| [`IS-proj` — `deployment/participant-client`](https://github.com/SAmWaM42/IS-proj/tree/deployment/participant-client) | Participant executable, General Interaction and Believability Study modes, participant credentials, questionnaire UI, local submission queue/uploader, runtime assets and platform packaging scripts. | Researcher training/analysis and server-side secrets. |
| [`IS-proj-web`](https://github.com/SAmWaM42/IS-proj-web) | Vercel API, participant-token authentication, questionnaire envelope validation, Google Drive persistence and GitHub Release download redirects. | C++ simulation, Q-learning or participant GUI. |

This `main` branch is the project-wide orientation document. Detailed implementation source lives on the branches above.

## Research comparison

- **Baseline agent:** tabular Q-learning using the shared external environment/action contract, without the proposed persistent semantic-memory and affect augmentation.
- **Proposed agent:** tabular Q-learning augmented with semantic memory and affective-state context.
- Both conditions should receive equivalent external scenarios and use comparable action/environment/reward contracts.
- The participant study uses frozen trained policies and counterbalanced A/B order; condition labels remain hidden from participants.
- Perceived believability is the primary human outcome. Behavioural consistency and adaptability are supporting outcomes where explicitly measurable.
- Synthetic reward, Q-table coverage, action frequency and training diagnostics are not substitutes for participant ratings.

## System pipelines

### 1. Development and training pipeline

```text
C++ environment and interaction/scenario runner
  -> shared observation and valid-action constraints
  -> baseline Q-learning policy
  -> proposed Q-learning policy
       -> semantic memory retrieval/ranking
       -> affective-state updates and bounded summaries
  -> reward / transition / Q-table updates
  -> training and evaluation summaries + per-decision JSONL traces
  -> offline researcher dashboard
```

The active training setup on `Dev` uses randomized scripted scenarios and paired baseline/proposed runs. Training and evaluation artefacts are stored under `artifacts/training/`. The offline visualisation script is `scripts/visualize_training_dashboard.py`. Researcher-only training schedules, synthetic-user experiments and diagnostics are not participant UI functionality.

### 2. Participant runtime pipeline

```text
AnimalAgent desktop executable (C++17 + SFML 3)
  -> participant menu
       -> General Interaction
       -> Believability Study
            -> credentials + consent + counterbalanced A/B conditions
            -> guided interactions
            -> in-app condition questionnaires + final comparison
            -> durable local submission queue
            -> Python HTTPS uploader
```

The participant executable loads trained baseline/proposed policies and does not perform researcher training. Participant identity is supplied through a private per-participant `credentials.json`, placed beside the executable; the common ZIP must not contain it. The participant workflow has been manually validated on Windows using the packaged release: participant login, both A/B questionnaires, distinct session 1/session 2 submission IDs, and persistence of the latest answers in Google Drive were confirmed. This is evidence for the tested Windows environment, not a guarantee for every Windows version or hardware configuration.

### 3. Questionnaire collection pipeline

```text
Participant client
  -> POST /api/submissions (Bearer participant token + Idempotency-Key)
  -> Vercel TypeScript function
  -> HMAC participant-token validation + envelope/A-B validation
  -> Google Drive JSON file
```

The server owns Google OAuth credentials and the HMAC seed/digest map. The participant client sends questionnaire response fields only. The current API validates the submission envelope, participant assignment, deterministic ID, idempotency header and token; full per-question answer-schema validation remains a separate requirement. Drive lookup-before-create handles ordinary retries but is not atomic under simultaneous duplicate requests.

### 4. Researcher data retrieval and analysis pipeline

```text
Google Drive questionnaire JSON
  -> researcher-side download utility (Dev)
  -> local data directory outside Git
  -> schema/metadata validation and participant-session pairing
  -> descriptive questionnaire summaries and visualisations
```

Questionnaire analysis is kept separate from training and behavioural diagnostics. Do not commit real participant submissions or credentials.

### 5. Build, package and distribution pipeline

```text
CMake + C++17 + SFML 3
  -> AnimalAgent executable
  -> explicit platform packaging script
  -> AnimalAgent-Linux.zip / AnimalAgent-Windows.zip
  -> GitHub Release asset
  -> IS-proj-web /api/downloads/latest redirect
```

Linux packaging uses `Deployment/package-linux.sh`; Windows packaging is built by the vcpkg/MSVC GitHub Actions workflow merged through [PR #29](https://github.com/SAmWaM42/IS-proj/pull/29). The Windows workflow builds and tests the participant executable, provisions a standalone Python/spaCy runtime, stages the application and uploads the package artifact. The resulting Windows package was manually extracted and exercised on a Windows computer without the project's development toolchain or separately installed application dependencies.

## Build configuration and dependencies

### Core desktop application

- Language standard: C++17.
- Build system: CMake (participant CMake minimum: 3.16).
- Graphics/windowing: SFML 3.0 (Graphics, Window, System).
- Threading: CMake `Threads`.
- JSON: nlohmann/json headers are used by the C++ application.
- Python boundary: persistent Python NLP process using spaCy and `en_core_web_sm`; deterministic keyword fallback is available in the NLP service.
- Participant tests: enable with `-DANIMAL_AGENT_BUILD_TESTS=ON` on `deployment/participant-client`.

Typical participant build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DANIMAL_AGENT_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/AnimalAgent
```

On Windows with a multi-configuration Visual Studio generator, run `cmake --build build --config Release` and launch the executable from the generated Release directory.

### Researcher utilities

Researcher-side C++ training/evaluation uses the Dev branch's CMake/build configuration. Python scripts support NLP, questionnaire download, analysis and dashboard generation. Use the actual branch-local `CMakeLists.txt`, `build.sh` and script help for exact current targets/options; older numbered specifications may describe superseded flags.

### Web service

- Hosting: Vercel.
- Language/runtime: TypeScript on Node.js serverless functions.
- Storage: Google Drive via Google OAuth refresh token.
- API routes: `/api/submissions`, `/api/downloads/latest`.
- Local verification: `npm run typecheck`, Vercel local development, and the repository's synthetic integration smoke test against a dedicated Preview + test Drive folder.

## Important configuration and secrets

### Participant client configuration

- `Data/Study/upload-config.json`: HTTPS `/api/submissions` endpoint. A placeholder endpoint prevents working uploads; configure it before packaging.
- `credentials.json`: individual participant number and private token, supplied separately beside the executable. Never commit or place it in the common ZIP.
- Trained policy assets: `artifacts/training/baseline.qtable` and `proposed.qtable` are required by the packagers and copied into the runtime package.
- Runtime assets include required images, font, NLP scripts/model and platform libraries.

### Vercel environment variables

- `GOOGLE_CLIENT_ID`
- `GOOGLE_CLIENT_SECRET`
- `GOOGLE_REFRESH_TOKEN`
- `GOOGLE_DRIVE_FOLDER_ID`
- `SEED` (server-only HMAC key)
- `PARTICIPANT_TOKENS` (participant-number-to-HMAC-digest map)

Never store these values in the desktop application, Git repository or public documentation. The participant's raw token arrives in the Authorization header, not in the JSON body.

## Packaging and release details

- Linux package script: `Deployment/package-linux.sh`.
- Windows package script on participant branch: `Deployment/package-windows.ps1`.
- Package outputs: `dist/linux/AnimalAgent-Linux.zip` and `dist/windows/AnimalAgent-Windows.zip`.
- The web download endpoint redirects to a matching asset attached to a published GitHub Release. A workflow artifact is not automatically a release asset.
- Test each ZIP on a clean target OS. A successful build or CI package does not prove GUI startup or full participant-flow correctness.

## Toolchain and working practices

- Git/GitHub branches isolate researcher development, participant deployment and web backend.
- CMake configures and builds C++; CTest runs registered C++/Python integration tests.
- SFML provides the participant desktop UI and simulation rendering.
- Python + spaCy provides deterministic text intent interpretation across a newline-JSON process boundary.
- nlohmann/json handles structured runtime and submission data in C++.
- Vercel deploys the web API and download redirect.
- Google Drive is the current questionnaire-response store.
- GitHub Releases distribute versioned platform ZIP assets.
- GitHub Actions runs the Windows MSVC/vcpkg build, test and package workflow merged by PR #29.
- Researcher-side Python scripts retrieve/analyse questionnaire submissions and render training diagnostics.

## Branch and release discipline

1. Make researcher-side training/analysis changes on `Dev`.
2. Port only required participant runtime changes to `deployment/participant-client`, preserving participant protocol and baseline/proposed parity.
3. Keep API, OAuth and release-redirect changes in `IS-proj-web`.
4. Do not merge participant changes or deploy production without explicit review/approval.
5. Record exact commit, build command, tests, package contents and target-platform validation.
6. Distinguish source-integrated, locally built, CI-built, clean-machine tested, and participant-flow validated states.

## Current release status — 12 November 2026

The participant Windows build workflow is merged into `deployment/participant-client`. Automated Windows CI completed successfully, and the packaged application was manually tested on a separate Windows computer without the development toolchain or application dependencies installed. The test covered participant login, both questionnaire conditions, session 1 and session 2 with distinct submission IDs, and successful Google Drive persistence with the latest answers.

The release candidate [v0.1.0-rc.1](https://github.com/SAmWaM42/IS-proj/releases/tag/v0.1.0-rc.1) contains `AnimalAgent-Linux.zip` and `AnimalAgent-Windows.zip`.

The website is deployed at [https://is-proj-web.vercel.app/](https://is-proj-web.vercel.app/) and exposes platform downloads through `/api/downloads/latest?platform=linux` and `/api/downloads/latest?platform=windows`.

The Windows package has passed the documented manual participant workflow on the tested machine. This does not establish compatibility with every Windows configuration, nor does it claim equivalent clean-machine testing for Linux. The web API validates the submission envelope and participant authentication but does not yet validate every questionnaire answer against the full questionnaire schema. Drive lookup-before-create supports ordinary retries but is not an atomic uniqueness guarantee.


## Specification history

The researcher-side repository includes historical numbered specifications from the initial prototype through the participant study. Their current disposition is recorded in [the Dev specification status register](https://github.com/SAmWaM42/IS-proj/blob/Dev/Docs/99-specification-status-register.md). This preserves original design rationale while identifying superseded drafts, experimental work and outstanding proposals. The current branch-specific README/specification remains authoritative for each component.
