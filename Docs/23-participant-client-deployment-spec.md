# Participant Client Deployment Specification

**Target branch:** `deployment/participant-client`  
**Starting point:** `test/believability-evaluation-modelling` at `10674dc4314b24a89dc8e9cbc38db58e14b2bdb3`  
**Document status:** Proposed specification; implementation has not started.  
**Primary goal:** Produce a small, self-contained participant-facing build that can be downloaded and run locally without researcher infrastructure.

---

## 1. Purpose

This branch will contain the participant-facing version of Animal Agent. It is intended to be packaged as a downloadable desktop application for a study participant or a single general-interaction user.

The deployment build must provide two entry points through one menu:

1. **General Interaction** — an ordinary, open-ended companion interaction.
2. **Believability Study** — the guided, two-condition participant study. The owner will supply the final study-flow code or changes for this option; the deployment work must provide a clean integration point and must not invent or replace the study protocol.

The branch is not intended to be a second research-development branch. It should remove researcher-facing controls and analysis utilities from the participant-facing product while preserving the runtime components and records required to conduct the study.

## 2. Goals

- Start the application at a clear menu instead of requiring command-line mode selection.
- Allow the user to choose General Interaction or Believability Study.
- Avoid creating a new general-interaction session file every time the application is launched or the user starts another interaction sequence.
- Assume one participant/user per distributed application copy. Do not build multi-user account management.
- Keep participant data outside the source/build directory and outside the Git repository.
- Preserve the trained policies as read-only deployment assets.
- Keep the study's condition separation, counterbalancing, blinding, and required logs intact.
- Exclude researcher-only analysis features and unrelated development files from the participant release package.
- Keep the application runnable locally; no VM, hosted backend, Docker service, or paid infrastructure is required.

## 3. Non-goals

- Multi-user hosting, accounts, authentication, remote session management, or concurrent participant support.
- Cloud data collection or automatic Google Drive upload.
- Reworking the study script, questionnaire, hypotheses, or consent procedure. Those remain governed by the study specification and the code supplied by the owner.
- Retraining policies on participant machines.
- Rewriting the agent, memory, affect, NLP, or simulation architecture merely to make deployment easier.
- Removing useful development tests from the development branch.
- Shipping source build artefacts such as CMake cache files, object files, or the local `build/` directory as the release.

## 4. Current repository observations

The starting branch currently has:

- `main.c++` command-line entry points for training, validation, diagnostics, ordinary simulation, and guided study.
- `CompanionUI` with ordinary simulation and guided-study modes. Its current screen enum contains Agent Selection, Simulation, Agent State, and Comparison; a top-level mode menu does not yet exist.
- A guided study path launched through `--study`, with the study UI and participant flow in the simulation component.
- Study manifests, step logs, interaction logs, session summaries, policy copies, and participant-counter handling.
- General simulation persistence in `CompanionUI::saveSession()`, including a fixed `simulation-session.json` summary and per-run log/manifest names. It also currently writes mutable proposed context to a path under the training policy directory; this must be reviewed before release.
- Research analysis utilities under `scripts/`, including analysis scripts and validation helpers. CMake currently registers analysis-related tests.
- Trained policy files expected under `artifacts/training/`.

These are observations from the source snapshot, not a claim that the current code has been compiled or verified. The deployment implementation must re-check the live branch before changing these paths.

## 5. Application entry and menu

### 5.1 Startup behaviour

The participant executable should start directly in a menu. The ordinary participant build must not require the user to know or type `--sim`, `--study`, `--train`, or other developer flags.

The menu should contain:

- **General Interaction**
  - Short neutral description: interact freely with the companion.
  - Starts the ordinary, non-researcher simulation mode.
- **Believability Study**
  - Short neutral description: complete the guided companion study.
  - Starts the guided study mode and its existing consent gate/protocol.
- **Exit**
  - Closes the application cleanly.

Do not show the participant the terms baseline, proposed, memory, affect, condition mapping, Q-table, or researcher diagnostics.

### 5.2 Navigation

- Menu selection must be possible with mouse input.
- Provide a visible way to return to the menu from General Interaction, after safely saving the user's data.
- The guided study must not offer an ordinary menu exit that bypasses its own completion/withdrawal handling. Closing the window remains an emergency stop, and incomplete-study handling must follow the study protocol.
- Prevent accidental multiple launches of the same mode from one click.
- Display a clear startup error if required policy, asset, NLP, or configuration files are missing.

### 5.3 Integration boundary

Prefer one application-level menu state that selects which existing mode to initialise. Do not duplicate the simulation or study implementation.

The General Interaction selection should initialise the existing ordinary `CompanionUI` path. The Believability Study selection should initialise the guided-study path (currently represented by the `guidedStudyMode` constructor option). The owner-provided study code will be integrated at this boundary after it is supplied.

The researcher-only simulation path and researcher UI must not be reachable in the participant build.

## 6. Persistence and session-file policy

### 6.1 Single-user assumption

Each distributed application copy is assigned to one user/participant. The application does not need to create a new profile or participant account for every launch.

A new application launch is **not** automatically a new participant. Starting a new interaction sequence is **not** automatically a new participant.

### 6.2 General Interaction persistence

General Interaction must use a stable per-installation data location and stable filenames. It must not create a new session summary file for each launch or interaction sequence.

Proposed logical layout:

```text
<user-data-directory>/
└── AnimalAgent/
    ├── general/
    │   ├── companion-state.json
    │   └── interaction-history.jsonl
    └── study/
        └── ...
```

- `companion-state.json` stores the latest persistent companion state needed to continue General Interaction (for example, memory and affect, plus a schema version).
- `interaction-history.jsonl` is one append-only history for the installation. It may contain multiple launches, but each record should include a timestamp and a stable installation/profile identifier.
- Do not create `run-<timestamp>-session.json`, `session-<counter>.json`, or equivalent files for every general interaction launch.
- Saving should update the one state file atomically (write a temporary file, flush/close it, then replace the previous file where supported). A failed save must not destroy the last valid state.
- If a state file is missing on first launch, initialise a clean state. If it is malformed or has an unsupported schema version, preserve the file, report the problem, and use a safe recovery path rather than silently overwriting it.
- A General Interaction reset must be researcher-only. There must be no reset control in the ordinary participant menu or simulation UI. The researcher-only reset must clearly confirm the action, reset only General Interaction state, and must not delete study records.

The exact user-data path must be platform-appropriate for both supported release targets. Do not write participant state into the executable directory, source tree, or `build/` directory. On Linux, use an appropriate per-user data directory (for example, under `~/.local/share/`). On Windows, use the user's application data directory (for example, `%LOCALAPPDATA%`). Isolate path selection behind one helper so persistence logic is platform-independent.

### 6.3 Separate trained policy from mutable user state

The distributed baseline and proposed policy files are trained inputs and must be treated as read-only.

- Load the trained policies from the packaged policy directory.
- Do not save participant memory, affect, or interaction-derived context back into the packaged `artifacts/training/proposed.qtable` or baseline policy.
- Persist mutable General Interaction memory/affect in the user's data directory, separate from the trained Q-table.
- If the current agent API combines policy and persistent context in one file, split the deployment save/load boundary or provide a separate sidecar without changing the trained policy values.
- Verify policy checksums before and after a session to demonstrate that participant use does not modify the packaged policies.

### 6.4 Guided Study data

The study is different from General Interaction: it must preserve the evidence required to join participant questionnaire responses to the two companion conditions.

Use **one study attempt per installation by default**, containing both condition encounters (session index 1 and 2), their condition codes, and all required phase/step/interaction records. A completed attempt is locked against another ordinary participant-initiated run.

Separate the stable participant identifier from the attempt/run identifier:

- `participant_id`: stable identifier for the person, for example `P014`.
- `attempt_number`: starts at 1 and increments only when a researcher authorises a reset/re-run.
- `run_id`: combine the two, for example `P014-A01` or `P014-A02`.
- `session_index`: `1` for the first companion encounter and `2` for the second encounter within that attempt.

Both condition forms for one attempt must receive the same `run_id` and different `session_index` values. The manifest and every relevant log row must also retain `participant_id`, `attempt_number`, `run_id`, and attempt status. This makes the two forms joinable while preserving the fact that multiple attempts belong to the same person.

A researcher-only reset may unlock a new attempt and increment `attempt_number`. It must not erase the previous attempt. Mark earlier attempts as `incomplete`, `completed`, or `superseded` as appropriate, and retain their records for audit. A second attempt must never be silently treated as a new participant. The analysis workflow must group by `participant_id` and apply a documented rule for selecting or excluding repeat attempts; multiple attempts from one person must not be counted as independent participants. The default main-study rule should be one valid completed attempt per participant, with any replacement attempt explicitly marked and justified.

Interaction history generated during the study itself is not needed for future companion behaviour and may be discarded when the study attempt is reset, but the research records and audit metadata for the previous attempt must be retained or exported before reset. Do not conflate disposable in-memory interaction history with required study evidence.

Before changing the current study code, compare its existing run-ID and form-prefill behaviour with this target. In particular, verify that the Google Form accepts the same `run_id` for both condition forms and distinguishes them using `session_index`. If the form currently uses separate run IDs per condition, update the prefill mapping and downstream join logic together; do not retain inconsistent identifiers merely to avoid changing the form.

The study must continue to:
- keep the A/B mapping counterbalanced as specified;
- reset proposed memory/affect between the two conditions;
- keep learning and exploration disabled;
- avoid modifying shared trained policies;
- preserve participant blinding;
- record the manifest, condition, phase, step, interaction, timestamps, software revision, and policy checksums required by the study specification;
- keep all participant data outside the repository.

A participant who exits or crashes partway through must not have an incomplete record presented as a completed study. Preserve enough status information to distinguish started, incomplete, and completed runs.

### 6.5 Data export

The distributed application itself will be delivered as **one common ZIP package** that can be sent to every participant. Each participant extracts and runs their own local copy; participant data is generated locally and is not included in the common distribution ZIP.

The application should provide a simple way for the researcher to locate or export the study data generated by one installation. A participant-specific ZIP export is acceptable for collection, but it is distinct from the common application ZIP. The data export must contain only that participant's study records and necessary metadata. It must not include trained policy files, source code, local configuration secrets, unrelated general-interaction history, or analysis scripts.

Do not automatically upload data. The researcher will provide the collection method and destination separately.

## 7. Analysis and researcher-only feature removal

The participant build must not expose analysis functions or ship researcher analysis tooling.

The starting repository snapshot does not show a top-level `analysis/` directory. Analysis-related functionality currently exists under `scripts/` and in CMake test registrations. Therefore:

- Exclude analysis scripts, analysis output directories, notebooks, plots, statistical reports, and analysis-only dependencies from the participant release package.
- Remove researcher-only analysis commands from the participant executable's command-line interface.
- Remove researcher UI screens, labels, and controls from the participant runtime.
- Keep study logging and data export: these are data collection, not analysis, and are required by the experiment.
- Keep the analysis tools on the research/development branch. On this deployment branch, remove them from the participant-facing source/package only after checking dependencies and updating CMake so a clean participant build does not refer to deleted files.
- Do not remove runtime validation or tests that protect the simulation, persistence, study flow, or log schema merely because they are located in a `tests/` or `scripts/` directory.

The release package should be an explicit allow-list, not a copy of the repository with a few obvious folders deleted.

## 8. Release package contents

The participant download should contain only what is needed to run the application.

Expected logical contents:

```text
AnimalAgent-Participant/
├── AnimalAgent                 # compiled executable
├── run.sh                      # launcher for the target OS
├── README.txt                  # participant instructions
├── assets/                     # runtime fonts, textures, and other required assets
├── nlp/                        # required Python NLP runtime/service and model
├── policies/                   # read-only trained baseline/proposed policies
└── config/                     # non-secret runtime configuration
```

The exact directory names may change after the dependency audit. Do not bundle the local CMake build directory, object files, compiler caches, Git metadata, developer notes, analysis utilities, training datasets, test fixtures, or source-only documentation.

The package must include all runtime dependencies or clearly identify any unavoidable operating-system prerequisites. Participants should not need to install a compiler, CMake, Python development headers, spaCy, or download an NLP model themselves.

The first release must target both Linux and Windows. Produce separate platform-specific ZIP packages from the same source revision; do not assume a Linux executable or bundled runtime will run on Windows. Each package must include the appropriate launcher and runtime dependencies and must be tested on a clean machine for its target OS.

## 9. Build and runtime configuration

- Keep the existing CMake build as the development build path.
- Add a distinct release configuration/target or packaging script that stages only the allow-listed runtime files.
- Resolve runtime paths relative to the executable/package root or the user-data directory; do not rely on `IS2_SOURCE_DIR` being present on a participant machine.
- Ensure the Python NLP process starts using the packaged interpreter/environment and model path, not a developer's virtual environment or absolute machine-specific path.
- Send diagnostic output to a local diagnostic log that does not contain raw participant free text or sensitive data.
- Fail clearly if a required asset, policy, NLP script, or model is missing.
- Provide a clean shutdown path that stops the NLP subprocess and flushes persistent data.

## 10. Security and privacy

- Do not collect names, email addresses, or other direct identifiers in application logs.
- Do not log raw free-text participant input. Preserve the current fixed-label/generic-marker approach unless the approved study protocol explicitly changes it.
- Keep form URLs and entry identifiers in non-secret configuration; never embed private credentials.
- Store participant data in a user-writable directory with normal per-user filesystem permissions.
- Do not include one participant's records in another participant's package.
- Document how the researcher retrieves and deletes a participant's data on withdrawal.

## 11. Implementation sequence

### Phase 0 — Branch and audit
1. Create this branch from the current believability evaluation branch.
2. Inventory runtime files, researcher-only files, data assets, policy files, and current save/load paths.
3. Trace every call to `saveSession()`, study manifest/log writers, and policy persistence.
4. Record the exact files required by General Interaction and the guided study.
5. Confirm the Linux and Windows toolchain/runtime strategy and identify a clean test machine for each target.

### Phase 1 — Persistence contract
1. Introduce one stable General Interaction data directory and fixed state/history filenames.
2. Separate mutable companion state from trained policy files.
3. Remove per-launch general session-summary creation.
4. Add atomic save/load and schema-version handling.
5. Test persistence across close/relaunch and multiple interaction sequences.

### Phase 2 — Main menu
1. Add an application-level Main Menu screen.
2. Route General Interaction to the existing ordinary simulation mode.
3. Route Believability Study to the existing guided-study mode.
4. Add safe return/exit behaviour without bypassing study completion/withdrawal handling.
5. Remove researcher-mode access from the participant executable.

### Phase 3 — Study data and owner-provided code
1. Integrate the owner's supplied study-flow code at the menu boundary.
2. Confirm that one participant run covers both conditions and that the form/log join keys still work.
3. Verify counterbalancing, memory reset between conditions, policy immutability, and incomplete-run handling.
4. Implement one participant ID plus attempt-number run IDs (`P014-A01`), shared across both condition forms with session indices 1 and 2.
5. Lock completed attempts against ordinary re-entry; provide a researcher-only reset that increments the attempt number and preserves previous research records.
6. Add a participant-specific export bundle or an equivalent clearly documented folder structure.

### Phase 4 — Release cleanup and packaging
1. Remove analysis/researcher-only functionality from the participant branch/package after dependency checks.
2. Update CMake and build scripts so the participant target has no references to excluded analysis files.
3. Add a release staging script with an explicit allow-list.
4. Package the executable, assets, policies, NLP runtime/model, launcher, and participant README.
5. Confirm that no build cache, test data, research analysis, developer notes, or secrets are shipped.

### Phase 5 — Clean-machine verification
1. Build from a clean checkout of this branch.
2. Run the Linux ZIP on a clean Linux machine and the Windows ZIP on a clean Windows machine, neither with the development environment installed.
3. Test General Interaction persistence across multiple launches and verify that reset is researcher-only.
4. Test the full guided study, both condition orders, same-run-ID/different-session-index form-prefill linking, completion lockout, and researcher-authorised repeat attempts.
5. Confirm repeat attempts retain the same participant ID, increment the attempt number, and are not counted as independent participants by the analysis workflow.
6. Confirm no researcher controls or analysis tools are visible or included in either package.
7. Confirm all participant data is outside the repository and policy checksums remain unchanged.
8. Test missing/corrupt state files, missing assets, NLP startup failure, and interrupted study recovery.

## 12. Acceptance criteria

The deployment branch is ready for a pilot only when:

- [ ] The application opens at the participant menu without command-line arguments.
- [ ] The menu offers General Interaction, Believability Study, and Exit.
- [ ] General Interaction loads the same user's saved companion state after application restart.
- [ ] General Interaction does not create a new session-summary file for every launch or interaction sequence.
- [ ] General Interaction state is stored outside the repository and build directory.
- [ ] General Interaction reset is unavailable to participants and accessible only through a researcher-controlled mechanism.
- [ ] Trained baseline/proposed policies are unchanged by participant interaction.
- [ ] The guided study uses the supplied study-flow implementation and retains the approved protocol.
- [ ] Both study conditions use one attempt `run_id` with `session_index` 1 and 2, and form responses join correctly.
- [ ] A completed study attempt is locked against ordinary re-entry.
- [ ] A researcher-authorised repeat attempt increments the attempt number while retaining the same participant ID and previous attempt records.
- [ ] Repeat attempts are identifiable and cannot be counted as independent participants by the analysis workflow.
- [ ] Participant logs preserve the required research fields without raw free-text capture.
- [ ] Incomplete studies are distinguishable from completed studies.
- [ ] Researcher screens, analysis commands, analysis scripts, and analysis outputs are absent from the participant release package.
- [ ] Separate Linux and Windows ZIP packages contain all runtime assets and NLP dependencies needed for their respective OS.
- [ ] Both packages run on clean machines without development tools installed.
- [ ] No participant data, secrets, or local machine paths are committed to Git.

## 13. Confirmed deployment decisions

The following decisions have been confirmed for this branch:

1. **Release operating systems:** Linux and Windows. Produce and test separate platform-specific ZIP packages.
2. **General Interaction reset:** researcher-only; no participant-facing reset control.
3. **Study re-runs:** completed attempts are locked. A deliberate researcher-only reset starts a new attempt number while retaining the same participant ID and preserving the previous attempt's research records.
4. **Application distribution:** one common ZIP package per operating system, sent to all participants. Each installation generates its own local data. A separate participant-specific data export may be collected after the study.
5. **Form integration:** one run ID per participant attempt, shared across both condition encounters, with `session_index` 1 and 2 distinguishing the two forms/conditions.

The exact researcher-only reset mechanism (for example, a researcher launch option or protected maintenance screen) can be chosen during implementation. It must not be presented as a normal participant feature. Local controls are a workflow safeguard, not strong security against a technically capable participant inspecting the distributed program.

Do not block implementation on cloud upload. Resolve the persistence, attempt identity, form-prefill, and menu contracts first.
