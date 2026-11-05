# Animal Agent — Participant Application

This branch contains the participant-facing desktop application only. Researcher training, evaluation, analysis and diagnostic workflows are maintained on the `Dev` branch.

## Application modes

- **General Interaction:** interact with the simulated companion. Memory and affective state are stored in the user's application-data directory.
- **Believability Study:** complete the guided two-condition study and questionnaire. Responses are queued locally and uploaded to the configured HTTPS collection endpoint. Failed uploads remain pending and are retried at the next launch.

The participant application loads fixed, trained baseline and proposed policies. It does not train or modify those policy files.

## Build from source

Build-machine requirements:

- CMake 3.16+
- C++17 compiler
- SFML 3.0 development libraries
- Python runtime containing spaCy and `en_core_web_sm` for typed NLP interaction

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/AnimalAgent
```

Participant-runtime tests can be enabled with `-DANIMAL_AGENT_BUILD_TESTS=ON`.

## Create participant ZIPs

Read [Deployment/README.md](Deployment/README.md) before packaging. Build each package on its target operating system:

- Linux: `bash Deployment/package-linux.sh`
- Windows: `powershell -ExecutionPolicy Bypass -File Deployment/package-windows.ps1`

The scripts create explicit allow-list packages under `dist/`; they do not package this repository.

## Participant credentials

The common ZIP deliberately does **not** contain participant credentials. For the Believability Study, the researcher supplies a separate `credentials.json` file. Place it beside the executable (beside `AnimalAgent.exe` on Windows).

```json
{
  "participant_number": 14,
  "token": "private-token-supplied-by-the-researcher"
}
```

Each participant receives their own token. Do not commit or include real credentials in the common ZIP. The uploader sends the token only as an HTTPS Bearer token; it removes the token from the questionnaire JSON body.

## Runtime data

Mutable application data is stored outside the installation directory:

- Linux: `$XDG_DATA_HOME/animal-agent/` when configured, otherwise `~/.local/share/animal-agent/`
- Windows: `%LOCALAPPDATA%/AnimalAgent/`

Do not delete pending questionnaire files while uploads are outstanding.

## Collection backend

The participant app sends questionnaire submissions to the HTTPS endpoint in `Data/Study/upload-config.json`. The collection API and its Google Drive credentials are deployed separately. Never put Google OAuth credentials or participant-token mappings in this client repository or ZIP.

## Scope boundary

This branch excludes researcher CLI modes, model training, headless evaluation, analysis scripts, visualisation outputs, researcher diagnostic datasets and backend deployment source. Those remain in their appropriate development/backend repositories.
