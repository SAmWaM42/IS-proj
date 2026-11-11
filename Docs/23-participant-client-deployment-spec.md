# Participant Client Deployment Specification

**Target branch:** `deployment/participant-client`  
**Starting point:** `test/believability-evaluation-modelling` at `10674dc4314b24a89dc8e9cbc38db58e14b2bdb3`  
**Document status:** Approved for implementation; deployment work is in progress.  
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
- Keep the study's condition separation, counterbalancing, blinding, and required questionnaire-linking fields intact.
- Treat questionnaire form responses as the primary research dataset. Do not collect Q-tables in the current study: the trained policies are shared and learning is disabled, so per-participant copies would not represent participant-specific results. Revisit only if the experimental design changes.
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

Current implementation layout:

```text
Linux:   ~/.local/share/animal-agent/
Windows: %LOCALAPPDATA%/AnimalAgent/
    companion-state.memory.json
    companion-state.affect.json
    study-linkage/
        study-participant-counter.txt
```

- The two `companion-state.*.json` sidecars store only the proposed agent's memory records and affective state. The trained Q-table is loaded from the package and is never saved by the participant application.
- General Interaction interaction history is not persisted by the participant client. Only the proposed agent's memory and affect sidecars are saved locally.
- Do not create `run-<timestamp>-session.json`, `session-<counter>.json`, or equivalent files for every general interaction launch.
- Saving should update the one state file atomically (write a temporary file, flush/close it, then replace the previous file where supported). A failed save must not destroy the last valid state.
- If a state file is missing on first launch, initialise a clean state. Malformed memory/affect sidecars are quarantined with a `.corrupt` suffix and the corresponding state falls back to defaults. Sidecar schema versioning remains a follow-up hardening item.
- No participant-facing or researcher-only reset workflow is required in the deployed application. Memory testing/resetting remains a development-time concern. Participants simply use the application; their local General Interaction state may persist across launches.

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

Use **one study attempt per installation by default**, containing both condition encounters (session index 1 and 2), their condition codes, and the identifiers needed to link questionnaire responses. The application should not collect or export interaction history, companion state, or other local runtime data merely because it exists on the participant's machine. Participant builds must not write raw interaction text, affect traces, or step-level logs to local study files.

Separate the stable participant identifier from the attempt/run identifier:

- `participant_id`: stable identifier for the person, for example `P014`.
- `run_id`: combine the two, for example `P014-A01` or `P014-A02`.
- `session_index`: `1` for the first companion encounter and `2` for the second encounter within that attempt.

Both condition forms must receive the same `run_id` and different `session_index` values. The forms must retain `run_id` and `session_index` so the two responses can be joined. The run ID must be unique per participant attempt; do not rely on a per-installation counter as a globally unique participant ID.

No reset/re-run workflow is required for participants or the researcher in the deployed application. The study attempt identifier remains useful for linking the two condition forms, but attempt-number incrementing, completed-attempt lockout, and preservation of repeat-attempt records are out of scope unless the study protocol later requires them. If a participant needs to repeat the study, handle that administratively and ensure questionnaire records are not accidentally treated as independent participants.

Local interaction history and companion state are not research deliverables by default. Keep them local only where needed for runtime behaviour; do not include them in participant exports.

Before changing the current study code, compare its existing run-ID and form-prefill behaviour with this target. In particular, verify that the Google Form accepts the same `run_id` for both condition forms and distinguishes them using `session_index`. If the form currently uses separate run IDs per condition, update the prefill mapping and downstream join logic together; do not retain inconsistent identifiers merely to avoid changing the form.

The study must continue to:
- keep the A/B mapping counterbalanced as specified;
- reset proposed memory/affect between the two conditions;
- keep learning and exploration disabled;
- avoid modifying shared trained policies;
- preserve participant blinding;
- preserve only the minimum local metadata needed to connect the two questionnaire forms (participant/run identifier and session index), plus any study-integrity metadata explicitly required by the approved protocol;
- keep all participant data outside the repository.

A participant who exits or crashes partway through must not have an incomplete record presented as a completed study. Preserve enough status information to distinguish started, incomplete, and completed runs.

### 6.5 Data export

The distributed application itself will be delivered as **one common ZIP package** that can be sent to every participant. Each participant extracts and runs their own local copy; participant data is generated locally and is not included in the common distribution ZIP.

The primary collected data is the questionnaire form data, which is submitted through the existing form workflow. The application preserves the identifiers required to join the two condition responses. No participant data export workflow is required. The trained Q-tables are fixed and shared across participants, so copying them from each participant installation would not create participant-specific observations. Do not export general-interaction history, raw participant text, memory records, or affective state.

Do not automatically upload data. The researcher will provide the collection method and destination separately.

## 7. Analysis and researcher-only feature removal

The participant build must not expose analysis functions or ship researcher analysis tooling.

The starting repository snapshot does not show a top-level `analysis/` directory. Analysis-related functionality currently exists under `scripts/` and in CMake test registrations. Therefore:

- Exclude analysis scripts, analysis output directories, notebooks, plots, statistical reports, and analysis-only dependencies from the participant release package.
- Remove researcher-only analysis commands from the participant executable's command-line interface.
- Remove researcher UI screens, labels, and controls from the participant runtime.
- Keep the minimum study metadata required for questionnaire linking. Do not add local data export unless a concrete research requirement emerges.
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

**Phase 2 implementation note (2026-11-02):** The application-level placeholder menu is registered in CMake and is the default no-argument startup path. It routes to the existing General Interaction and guided-study modes, provides a Main Menu return from General Interaction, and exits cleanly. The participant target rejects command-line arguments so developer/researcher launch modes are not available through the participant executable. The menu is intentionally replaceable by the owner's menu implementation. Build and integrated runtime verification are deferred until the full client conversion is assembled, as requested.

**Phase 1 implementation note (2026-11-02):** General Interaction now stores proposed-agent memory and affect sidecars under the platform user-data directory, separate from trained policy files. The sidecars are written through temporary files and replacement; malformed sidecars are quarantined and default state is used. The fixed `simulation-session.json` and baseline-session Q-table writes have been removed from the participant save path. Participant study builds suppress local interaction/step/manifest log files; form responses remain the primary research dataset. Persistence and recovery have not yet been runtime-tested.

### Phase 3 — Study data and owner-provided code
1. Integrate the owner's supplied study-flow code at the menu boundary when provided.
2. Confirm that one participant run covers both conditions and that the form/log join keys still work.
3. Verify counterbalancing, memory reset between conditions, policy immutability, and incomplete-run handling required by the existing study protocol.
4. Preserve one run identifier shared across both condition forms, with session indices 1 and 2.
5. Keep questionnaire responses as the primary collected data. Do not build participant reset/re-run or local data-export workflows unless the study protocol requires them.
6. Do not add Q-table export in the current design; the study uses fixed shared policies with learning disabled.

**Phase 3 implementation note (2026-11-02):** Added `Data/Study/form-config.json` as the packaged source for the Google Form URL and prefill entry IDs, with environment-variable overrides for researcher testing. The existing guided flow retains one `run_id` across both conditions and changes `session_index` from 1 to 2. The configuration still contains placeholders and must be completed before release. Important: the existing participant counter is local to each installation, so it cannot provide global A/B counterbalancing across participants on separate machines. A researcher-issued participant assignment or another cross-installation assignment method must be agreed before collecting data.

### Phase 4 — Release cleanup and packaging
1. Remove analysis/researcher-only functionality from the participant branch/package after dependency checks.
2. Update CMake and build scripts so the participant target has no references to excluded analysis files.
3. Add a release staging script with an explicit allow-list.
4. Package the executable, assets, policies, NLP runtime/model, launcher, and participant README.
5. Confirm that no build cache, test data, research analysis, developer notes, or secrets are shipped.

**Phase 4 implementation note (2026-11-02):** Added allow-list staging scripts for Linux and Windows under `Deployment/`. They package only the participant executable, launcher, required assets/font, study JSON/configuration, NLP service, Python runtime, SFML runtime dependencies, and the two trained policies. The scripts require a relocatable Python runtime with spaCy/model already prepared, and matching SFML runtime files. CMake now builds a participant-only target by default, excludes training/study implementation modules and research test targets, and retains the developer tests/scripts in the repository. The NLP subprocess, browser launch, timestamps, and atomic sidecar replacement have platform-specific Windows/Linux implementations. These are packaging and portability changes only; no build or ZIP has been run.

### Phase 5 — Clean-machine verification
1. Build from a clean checkout of this branch.
2. Run the Linux ZIP on a clean Linux machine and the Windows ZIP on a clean Windows machine, neither with the development environment installed.
3. Test General Interaction persistence across multiple launches; no reset workflow is required.
4. Test the full guided study, both condition orders, and same-run-ID/different-session-index form-prefill linking.
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
- [ ] Trained baseline/proposed policies are unchanged by participant interaction.
- [ ] The guided study uses the supplied study-flow implementation and retains the approved protocol.
- [ ] Both study conditions use one attempt `run_id` with `session_index` 1 and 2, and form responses join correctly.
- [ ] The two questionnaire responses can be joined using one run ID and distinct session indices.
- [ ] Questionnaire form responses are treated as the primary research dataset; local interaction/state data is not collected by default.
- [ ] An optional Q-table snapshot can be retained only if required for the planned analysis.
- [ ] Incomplete studies are distinguishable from completed studies.
- [ ] Researcher screens, analysis commands, analysis scripts, and analysis outputs are absent from the participant release package.
- [ ] Separate Linux and Windows ZIP packages contain all runtime assets and NLP dependencies needed for their respective OS.
- [ ] Both packages run on clean machines without development tools installed.
- [ ] No participant data, secrets, or local machine paths are committed to Git.

## 13. Confirmed deployment decisions

The following decisions have been confirmed for this branch:

1. **Release operating systems:** Linux and Windows. Produce and test separate platform-specific ZIP packages.
2. **Reset workflow:** no participant-facing or researcher-only reset is required in the deployed application. Memory reset/testing remains a development-time operation.
3. **Research data:** questionnaire form responses are the primary dataset. The current fixed Q-tables are not participant-specific data; local interaction history, companion state, memory, and affect are not collected by default.
4. **Application distribution:** one common ZIP package per operating system, sent to all participants. Runtime state remains on each participant's machine; questionnaire responses are collected through the forms. No participant-specific local export is required.
5. **Form integration:** one unique run ID per participant attempt, shared across both condition encounters, with `session_index` 1 and 2 distinguishing the two forms/conditions.

Do not block implementation on cloud upload. Resolve local persistence, participant/run identity, form-prefill, and menu contracts first. Do not add reset or local data-export functionality without a demonstrated research need.


---

## Collection-mode amendment (2026-11-03)

This amendment supersedes earlier statements in this document that require Google Forms or prohibit cloud upload. The approved collection direction is now an in-app questionnaire with automatic HTTPS submission to a Vercel function, which stores questionnaire JSON in the researcher's Google Drive using Google OAuth.

### Collection architecture

```text
Participant questionnaire
        |
        v
SubmissionManager (local durable queue)
        |
        v
Bundled Python HTTPS uploader
        |
        v
Vercel Node.js function
        |
        v
Google Drive folder (OAuth refresh token held by Vercel)
```

- The participant client sends questionnaire response fields only. Do not send raw interaction text, memory/affect traces, Q-tables, or application diagnostics.
- Each participant receives a researcher-issued participant number and a unique high-entropy upload token. The participant number identifies the response; the token authenticates the upload. The common ZIP must not contain participant tokens.
- The token is entered/provided through the participant study flow and retained only in the local pending submission record so a failed upload can be retried. The uploader removes the token before serialising the request body.
- Submission IDs remain deterministic as `P{participantNumber}-S{sessionIndex}`, with session index 1 or 2. The same ID is used as the idempotency key.
- The client deletes a queued response only after receiving `{"stored":true,"submission_id":"..."}` from the endpoint. Failed or ambiguous uploads remain queued and are retried on application launch.
- The Vercel function validates the schema and participant token, then stores a JSON file in the configured Drive folder. OAuth client credentials, refresh token, folder ID, and participant-token mapping are Vercel environment variables and must never be committed or included in the desktop ZIP.
- Drive lookup-before-create handles normal retries but does not guarantee uniqueness under simultaneous duplicate requests. A transactional database with a unique key would be required for that stronger guarantee.

### Current implementation status

- [x] Add Vercel backend source under `CollectionBackend/`.
- [x] Add Google OAuth Drive storage and participant-token validation contract.
- [x] Add client uploader Authorization header and strip token from transmitted JSON.
- [x] Persist participant token with local pending submission for retry.
- [x] Change packaging validation to require a configured HTTPS Vercel endpoint.
- [x] Update deployment instructions for Vercel and Google OAuth.
- [ ] Connect the new `queueSubmission(..., participantToken, responses)` API to the final in-app questionnaire and participant assignment UI.
- [ ] Remove obsolete Google Forms prefill dependencies once the supplied study-flow/menu code has been integrated.
- [ ] Deploy the Vercel project and configure OAuth and participant tokens.
- [ ] Test endpoint authentication, durable Drive storage, duplicate retry, and offline queue recovery.
- [ ] Build and test both platform ZIPs on clean Linux and Windows systems.

**Important:** The current menu/questionnaire flow has not yet been replaced. The collection transport and server infrastructure are now present, but the participant-facing questionnaire integration is not complete. Do not recruit participants or distribute a release package until the remaining items above are completed and tested.


---

## Current implementation update — 11 November 2026

This specification describes only the participant desktop runtime and package boundary. The current participant branch contains the single `AnimalAgent` CMake executable, General Interaction and Believability Study menu modes, participant configuration/credentials parsing, guided study and questionnaire UI/content, questionnaire submission adaptation, local submission manager/queue, Python uploader/NLP service, trained baseline/proposed policy assets and platform package scripts.

The questionnaire completion path is connected in source: session 1 is queued after the first condition questionnaire; session 2 is queued after the second condition questionnaire and final comparison. Pending submissions are retried at startup. These are source-integration facts, not proof of end-to-end operation. A full build/CTest, interactive study walkthrough, restart/offline retry, clean-machine package test and synthetic API-to-Drive verification remain required.

The web API and Google Drive storage are specified in `IS-proj-web/README.md`; researcher-side training and analysis remain on `Dev`. The Windows vcpkg/static-SFML CI candidate is separate on `feature/windows-github-actions` via PR #29 and is not yet merged or verified. Do not treat it as release-ready.