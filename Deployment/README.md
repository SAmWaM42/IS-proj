# Participant client packaging

This branch produces a desktop participant application, not a server deployment. The scripts build an explicit allow-list ZIP for one target operating system.

## Build-machine requirements

### Linux

- CMake 3.16+, C++17 compiler and SFML 3 development libraries.
- A Python runtime containing spaCy and `en_core_web_sm`.
- `ANIMAL_AGENT_PYTHON_RUNTIME`: directory containing `bin/python` or `bin/python3`. If omitted, the script checks `wordTokenizer/`, `.venv/` and `venv/`.
- `ANIMAL_AGENT_SFML_RUNTIME`: directory containing SFML 3 shared libraries and non-system dependencies required by them.
- `ANIMAL_AGENT_FONT_FILE`: path to a redistributable TTF font. The script defaults to the system DejaVu Sans font path if available.

### Windows

- CMake 3.16+, C++17 compiler and SFML 3 development libraries.
- `ANIMAL_AGENT_PYTHON_RUNTIME`: standalone/relocatable Python runtime directory containing `python.exe`, spaCy and `en_core_web_sm`.
- `ANIMAL_AGENT_SFML_DLL_DIR`: directory containing the SFML 3 runtime DLLs.
- `ANIMAL_AGENT_FONT_FILE`: path to a redistributable TTF font. This is copied to `Data/Fonts/UIFont.ttf`.

Build on the operating system you intend to distribute for.

## Build commands

```sh
bash Deployment/package-linux.sh
```

```powershell
powershell -ExecutionPolicy Bypass -File Deployment/package-windows.ps1
```

Output:

- Linux: `dist/linux/AnimalAgent-Linux.zip`
- Windows: `dist/windows/AnimalAgent-Windows.zip`

## Included runtime files

The package scripts explicitly stage:

- `AnimalAgent` executable and platform launcher.
- Font at `Data/Fonts/UIFont.ttf`.
- Backyard atlas and dog PNG pose frames.
- Study JSON configuration, questionnaire schema and HTTPS endpoint configuration.
- NLP intent service and questionnaire uploader.
- Bundled Python runtime with spaCy and `en_core_web_sm`.
- Fixed `baseline.qtable` and `proposed.qtable` policy files.
- Participant instructions.

They do not copy the source repository, researcher analysis scripts, tests, training logs, visualisations, or Git metadata.

## Participant credentials

The common ZIP must not contain participant credentials. The researcher supplies a unique `credentials.json` for each study participant. Place it beside the executable after extracting the ZIP. Its structure is:

```json
{
  "participant_number": 14,
  "token": "private-token-supplied-by-the-researcher"
}
```

Do not commit a real credentials file or embed participant tokens in the package. Google OAuth credentials and participant-token mappings are held by the separately deployed collection backend, never by the desktop client.

## Portability and release checks

A copied Python virtual environment is **not necessarily a standalone Python runtime**. Its interpreter can depend on the base Python installation and shared libraries on the build machine. The Linux script checks spaCy/model loading and unresolved shared libraries on the build machine, but that does not prove the ZIP will run on a clean participant machine. Use a genuinely relocatable Python distribution where possible and test the extracted ZIP on a clean Linux installation.

The Windows script expects a relocatable runtime, not a machine-specific Python installation. SFML runtime dependencies must also be verified on a clean target machine.

A successful ZIP creation is not release validation. Before distribution, extract the ZIP into a clean user account or VM on the target OS, add a test credentials file, launch both menu options, exercise typed input, complete a synthetic questionnaire, and verify that a test submission reaches the backend. Confirm that a deliberately interrupted/offline submission remains queued and uploads on the next launch.

## Collection backend

The HTTPS collection function is deployed separately from this client. Its Vercel environment contains Google OAuth credentials, Drive destination settings and participant-token mappings. The client contains only the public HTTPS endpoint URL. The uploader sends the participant token as an HTTPS Bearer token and removes it from the questionnaire JSON body. Pending local responses retain the token to support offline retry.

The backend uses idempotency to make ordinary retries safe; simultaneous duplicate requests are not guaranteed to be transactionally deduplicated.


---

## Current branch and CI status (11 November 2026)

This packaging guide applies to the current `deployment/participant-client` branch scripts. The participant CMake project builds C++17 against SFML 3.0 Graphics/Window/System and Threads. The Windows packager currently expects `ANIMAL_AGENT_PYTHON_RUNTIME`, `ANIMAL_AGENT_SFML_DLL_DIR`, a configured HTTPS upload endpoint, both trained Q-table files and `ANIMAL_AGENT_FONT_FILE`. The Linux packager expects a Python environment containing spaCy/model, `ANIMAL_AGENT_SFML_RUNTIME`, configured endpoint, policy files and a redistributable font. Review the scripts themselves for exact checks and output paths.

The newer vcpkg/static-SFML Windows workflow and corresponding packaging changes are on `feature/windows-github-actions`, proposed by [PR #29](https://github.com/SAmWaM42/IS-proj/pull/29). They are not yet merged into this branch. At the last recorded check, no Actions run, status check or ZIP artifact had been observed. Do not treat the candidate workflow as a successful build until the run succeeds and the artifact is available.

A package-script exit or successful CI artifact does not prove portability on a clean Windows installation. The Windows executable still requires real desktop GUI, menu, consent, condition sequence, questionnaire, local queue and upload-flow validation. Linux and Windows packages must be tested on their respective target operating systems.
