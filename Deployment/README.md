# Participant client packaging notes

The participant release is a desktop application, not a server deployment. Build one ZIP per operating system from this branch. The package scripts use an explicit allow-list and do not copy the repository, tests, analysis scripts, training outputs, or Git metadata.

## Runtime prerequisites for the packager

The Linux and Windows scripts expect a **relocatable Python runtime** supplied through `ANIMAL_AGENT_PYTHON_RUNTIME`. It must contain the platform's Python executable, spaCy, and the `en_core_web_sm` model. Do not point this at an ordinary virtual environment unless you have verified that it is relocatable after extraction to a different path.

- Linux: `bin/python3` must exist.
- Windows: `python.exe` must exist.

The Linux script also expects `ANIMAL_AGENT_SFML_RUNTIME`, containing SFML 3 shared libraries and the required non-system shared dependencies. It checks the staged executable with `ldd`. The Windows script expects `ANIMAL_AGENT_SFML_DLL_DIR` containing the SFML 3 runtime DLLs.

The packager itself needs the relevant platform compiler, CMake, and SFML 3 development package. These are build-machine prerequisites, not intended participant prerequisites.

## Before packaging

1. Fill in `Data/Study/form-config.json` with the published Google Form prefill URL and the three entry IDs.
2. Confirm `artifacts/training/baseline.qtable` and `proposed.qtable` are the final trained policies.
3. Prepare and validate the relocatable Python runtime on the target OS.
4. Prepare the matching SFML runtime libraries.
5. Run the platform package script on that platform.

## Output

- Linux: `dist/linux/AnimalAgent-Linux.zip`
- Windows: `dist/windows/AnimalAgent-Windows.zip`

The ZIPs contain only the executable, launcher, participant README, required image assets, study configuration/scenarios, NLP service/model runtime, and the two read-only policy files. They do not contain the repository's analysis scripts or tests.

## Important limitations

- A package script completing does not replace a clean-machine extraction and launch test.
- Linux and Windows builds must be produced on their respective target platforms.
- SFML and Python runtime portability must be verified on a clean machine.
- The current study participant counter is stored locally on each installation. That does **not** provide global counterbalancing across participants using separate machines. The study needs a researcher-issued participant assignment or another shared assignment mechanism before data collection. Do not interpret the local counter as a global participant sequence.
- The Q-tables are loaded as fixed trained policies and study learning is disabled. An optional Q-table copy is therefore the same policy artifact, not a participant-specific outcome, unless a separate policy-update/measurement requirement is introduced.
