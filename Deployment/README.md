# Participant client packaging notes

The participant release is a desktop application, not a server deployment. Build one ZIP per operating system from this branch. The package scripts use an explicit allow-list and do not copy the repository, tests, analysis scripts, training outputs, or Git metadata.

## Runtime prerequisites for the packager

The Linux and Windows scripts expect a **relocatable Python runtime** supplied through `ANIMAL_AGENT_PYTHON_RUNTIME`. It must contain the platform's Python executable, spaCy, and the `en_core_web_sm` model. Do not point this at an ordinary virtual environment unless you have verified that it is relocatable after extraction to a different path.

- Linux: `bin/python3` must exist.
- Windows: `python.exe` must exist.

The Linux script also expects `ANIMAL_AGENT_SFML_RUNTIME`, containing SFML 3 shared libraries and the required non-system shared dependencies. It checks the staged executable with `ldd`. The Windows script expects `ANIMAL_AGENT_SFML_DLL_DIR` containing the SFML 3 runtime DLLs.

The packager itself needs the relevant platform compiler, CMake, and SFML 3 development package. These are build-machine prerequisites, not intended participant prerequisites.

## Before packaging

1. Deploy `CollectionBackend/` to Vercel with its root directory set to that folder.
2. Configure Google OAuth, the destination Drive folder, and participant-token mapping in Vercel environment variables. See `CollectionBackend/README.md`.
3. Set the deployed HTTPS function URL in `Data/Study/upload-config.json`.
4. Confirm `artifacts/training/baseline.qtable` and `proposed.qtable` are the final trained policies.
5. Prepare and validate the relocatable Python runtime on the target OS.
6. Prepare the matching SFML runtime libraries.
7. Run the platform package script on that platform.

## Output

- Linux: `dist/linux/AnimalAgent-Linux.zip`
- Windows: `dist/windows/AnimalAgent-Windows.zip`

The ZIPs contain the executable, launcher, participant README, required image assets, non-secret endpoint configuration, NLP service/model runtime, and the two read-only policy files. Participant upload tokens are assigned and entered separately; they must not be embedded in the common ZIP. They do not contain the repository's analysis scripts or tests.

## Important limitations

- A package script completing does not replace a clean-machine extraction and launch test.
- Linux and Windows builds must be produced on their respective target platforms.
- SFML and Python runtime portability must be verified on a clean machine.
- Participant numbers and their unique upload tokens must be issued by the researcher. A participant number alone is not authentication. The participant token is retained with a pending local response for retry and stripped from the questionnaire JSON before transmission.
- The collection function stores questionnaire JSON in Google Drive using the researcher's Google OAuth credentials held in Vercel environment variables. Never place OAuth credentials in the desktop package.
- Drive lookup followed by create prevents ordinary retry duplicates but is not transactional under simultaneous duplicate requests.
- The Q-tables are loaded as fixed trained policies and study learning is disabled. An optional Q-table copy is therefore the same policy artifact, not a participant-specific outcome, unless a separate policy-update/measurement requirement is introduced.
