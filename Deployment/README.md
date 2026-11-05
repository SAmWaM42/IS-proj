# Participant client packaging notes

The participant release is a desktop application, not a server deployment. Build one ZIP per operating system from this branch. The package scripts use an explicit allow-list and do not copy the repository, tests, analysis scripts, training outputs, or Git metadata.

## Runtime prerequisites for the packager

The Linux script can use the project's Python virtual environment directly. Set `ANIMAL_AGENT_PYTHON_RUNTIME` to that environment's directory, or let the script auto-detect `wordTokenizer/`, `.venv/`, or `venv/` at the repository root. It copies the environment and checks that the staged interpreter can import spaCy and load `en_core_web_sm`.

**Important portability limitation:** a standard `venv` is not a self-contained Python distribution. Its interpreter commonly links to the base Python installation used when the environment was created. Copying it can therefore work on the build machine but fail on a participant's machine that lacks a compatible Python installation. The staged `ldd` checks catch missing shared libraries on the build machine, but cannot prove portability to a clean machine. Before distribution, test the ZIP on a clean Linux installation. If that machine does not have the matching base Python/runtime dependencies, supply a genuinely standalone Python runtime instead of the ordinary venv.

- Linux: `bin/python` or `bin/python3` must exist.
- Windows: the existing Windows script still expects a standalone runtime with `python.exe`; the Linux venv auto-detection does not apply to Windows.

The Linux script also expects `ANIMAL_AGENT_SFML_RUNTIME`, containing SFML 3 shared libraries and the required non-system shared dependencies. It checks the staged executable with `ldd`. The Windows script expects `ANIMAL_AGENT_SFML_DLL_DIR` containing the SFML 3 runtime DLLs.

The packager itself needs the relevant platform compiler, CMake, and SFML 3 development package. These are build-machine prerequisites, not intended participant prerequisites.

## Before packaging

1. Deploy `CollectionBackend/` to Vercel with its root directory set to that folder.
2. Configure Google OAuth, the destination Drive folder, and participant-token mapping in Vercel environment variables. See `CollectionBackend/README.md`.
3. Set the deployed HTTPS function URL in `Data/Study/upload-config.json`.
4. Confirm `artifacts/training/baseline.qtable` and `proposed.qtable` are the final trained policies.
5. Prepare the Python environment for the target OS. On Linux, the script can use the project venv, subject to the clean-machine portability check above. On Windows, supply a standalone runtime.
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


## Collection transport status

Questionnaire collection is being moved from Google Forms to the Vercel endpoint documented in `CollectionBackend/README.md`. The participant application queues a response locally, uploads it with the participant's bearer token, and removes the queue file only after a valid durable-storage acknowledgement. Pending responses are retried on the next application launch.

The queue API now requires the participant token:

```cpp
queueSubmission(participantNumber, sessionIndex, condition, participantToken, responses);
```

The token is saved in the local pending record to support offline retry, but the Python uploader removes it from the JSON body and sends it only as an HTTPS Bearer token. Participants must receive their number and token separately from the common ZIP.

**Release blocker:** the final participant menu/questionnaire flow has not yet been connected to this API. The current package scripts are infrastructure only; do not distribute a participant build until the in-app questionnaire, token entry, and study-flow integration are complete and tested.
