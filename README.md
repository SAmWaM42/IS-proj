# Animal Agent — Participant Client

This branch (`deployment/participant-client`) contains the participant-facing desktop application and release packaging configuration. It is intentionally narrower than `Dev`: it excludes researcher training orchestration, synthetic-user experiments, training dashboards and questionnaire analysis utilities.

The researcher/development implementation is maintained on [`Dev`](https://github.com/SAmWaM42/IS-proj/tree/Dev). The collection API and download redirect service are maintained in [`SAmWaM42/IS-proj-web`](https://github.com/SAmWaM42/IS-proj-web). The root `main` branch is the project-wide README and integration overview, not the active participant implementation.

## Participant application

The single CMake executable is named `AnimalAgent`. It starts at a participant menu with two modes:

- **General Interaction** — ordinary interaction with the simulated companion. Mutable companion state is kept in the platform's user-data location.
- **Believability Study** — guided two-condition study with participant assignment, consent, scripted interactions, in-app questionnaires and a final comparison form.

The study uses baseline and proposed conditions with counterbalanced A/B order. Participant number is loaded from a researcher-issued `credentials.json`; it must not be generated locally. The participant-facing UI should not reveal condition labels or internal model details.

The proposed condition can retain supported semantic preferences and use them to bias fetch-object selection. The guided preference demonstration offers ball, stick and rope; a supported rope preference should influence the proposed agent's FETCH object selection. The baseline does not receive or consult that preference memory and retains its neutral ball default. This is an object-selection bias, not a new Q-learning action.

The participant branch also contains the NLP intent adapter (greeting, request, disturbance, rest, negative feedback, praise, preference and unknown), C++ preference-to-memory handoff, and bounded no-response/INSIST state support. The current guided script does not create repeated ignored requests, so `no_response_count` remains zero and INSIST is not eligible in that protocol.

## Runtime and study data

- The participant app loads fixed trained baseline/proposed Q-tables; it does not run researcher training.
- Participant credentials are supplied separately and are never included in the common ZIP.
- Questionnaire responses are saved to a local durable queue and submitted to the configured HTTPS collection endpoint.
- Failed or ambiguous submissions remain pending for retry; the uploader sends the participant token as a Bearer header and excludes it from the response JSON body.
- The web API stores questionnaire JSON in Google Drive. It should receive questionnaire response data only, not raw interaction text, memory/affect traces, Q-tables or diagnostics.
- Mutable application data belongs in the platform's user-data directory, not the extracted installation folder.

The participant questionnaire and submission integration has been built and exercised end-to-end in the tested Windows package. Both A/B conditions and both sessions were completed; session 1 and session 2 generated distinct submission IDs, and the latest questionnaire answers were confirmed in the configured Google Drive folder. See [Windows manual validation](Docs/25-windows-participant-manual-validation.md). This is technical validation of the tested workflow, not a substitute for required study/ethics approval or ongoing collection monitoring.

## Build requirements

- CMake 3.16 or newer.
- C++17 compiler.
- SFML 3.0 development libraries.
- Python runtime with spaCy and `en_core_web_sm` for the NLP service.
- Platform-specific compiler/linker and runtime libraries.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/AnimalAgent
```

Enable participant tests with `-DANIMAL_AGENT_BUILD_TESTS=ON`, then run CTest. On Windows with a multi-configuration Visual Studio generator, build with `--config Release` and launch the executable from the generated Release directory.

## Packaging

Read [Deployment packaging notes](Deployment/README.md) before creating a release package.

- Linux: `bash Deployment/package-linux.sh`
- Windows: `powershell -ExecutionPolicy Bypass -File Deployment/package-windows.ps1`

The scripts create allow-list ZIPs under `dist/`. The Linux script expects a Python environment containing spaCy/model and an SFML 3 shared-library directory; an ordinary copied virtualenv is not guaranteed to be portable to a clean machine. The Windows GitHub Actions workflow uses MSVC, vcpkg-provided SFML and a provisioned standalone Python/spaCy runtime. The resulting Windows package was manually extracted and run on a Windows computer without the project development toolchain or separately installed application dependencies.

### Windows CI and release status

The Windows workflow was merged from PR #29 into this branch. It builds the participant client with MSVC/vcpkg, runs the configured C++ and NLP tests, provisions the standalone Python/spaCy runtime, packages the application and uploads the `AnimalAgent-Windows` artifact. The CI run passed. The downloaded package was then manually tested on a separate Windows computer; participant login, both A/B questionnaires, distinct session submission IDs and Google Drive persistence were confirmed.

The published [release candidate](https://github.com/SAmWaM42/IS-proj/releases/tag/v0.1.0-rc.1) contains both `AnimalAgent-Linux.zip` and `AnimalAgent-Windows.zip`. The website's platform download links are served by the web repository's release redirect endpoint. See [manual validation record](Docs/25-windows-participant-manual-validation.md).

## Branch boundaries

| Location | Responsibility |
|---|---|
| `deployment/participant-client` | Participant app, participant protocol, runtime dependencies, local queue and platform packages. |
| `Dev` | Researcher-side agent development, training, controlled evaluation and analysis. |
| `IS-proj-web` | Authentication, questionnaire submission API, Google Drive persistence and download redirects. |
| `main` | Overall architecture, build/pipeline/tooling summary. |

Any change to participant-visible behaviour or runtime semantics must be checked against the corresponding Dev implementation and deliberately ported where appropriate. Researcher-only training schedules, synthetic-user profiles, diagnostics and analysis scripts must not leak into the participant package.

## Participant credentials

The common ZIP deliberately excludes credentials. Each participant receives an individual `credentials.json` beside the executable:

```json
{
  "participant_number": 14,
  "token": "private-token-supplied-by-the-researcher"
}
```

Treat this file as secret. Never commit it, share it between participants or include it in a common ZIP.

## Related documentation

- [Deployment packaging details](Deployment/README.md)
- [Participant runtime parity update](Docs/24-participant-runtime-parity-update.md)
- [Current implementation map on Dev](https://github.com/SAmWaM42/IS-proj/blob/Dev/Docs/00-current-implementation-map.md)
- [Researcher-side status tracker](https://github.com/SAmWaM42/IS-proj/blob/Dev/Docs/07-implementation-status-and-roadmap.md)
