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

The participant questionnaire and submission integration exists in source, but the latest integration has not yet been fully built, interactively walked through, or end-to-end verified. Do not use this branch for real participant collection until the study milestone tracker and validation gates are complete.

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

The scripts create allow-list ZIPs under `dist/`. The current Linux script expects a Python environment containing spaCy/model and an SFML 3 shared-library directory; an ordinary copied virtualenv is not guaranteed to be portable to a clean machine. The current Windows script expects a standalone Python runtime, an SFML DLL directory and a redistributable font. A packaging-script exit alone is not proof that the package runs on a clean target machine.

### Windows CI status

A Windows GitHub Actions workflow is being developed on the separate feature branch `feature/windows-github-actions`, proposed through [PR #29](https://github.com/SAmWaM42/IS-proj/pull/29). It is not yet part of this branch. At the last recorded check, no CI run or artifact had been observed. Do not describe the Windows package as CI-verified until the workflow builds, tests, packages `AnimalAgent-Windows.zip`, and the uploaded artifact is present. Even then, a real Windows desktop GUI and participant-flow test remains necessary.

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
