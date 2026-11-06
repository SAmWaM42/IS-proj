# Implementation Status and Roadmap

**Current branch under review:** `Dev` (researcher-side data retrieval work is being reviewed)  
**Last documentation review:** 5 November 2026  
**Authoritative milestone tracker:** [21-participant-study-milestones.md](21-participant-study-milestones.md)

## 1. Current project state

The core simulated companion and its baseline/proposed agent architecture exist. The current participant application is a single CMake executable whose output name is AnimalAgent. It opens an SFML menu with **Free Roam** and **Research Session** choices.

The participant research workflow remains a prototype, not a completed data-collection pipeline. Participant credentials are parsed from credentials.json beside AnimalAgent and the guided-study local counter has been removed. The project owner confirms valid credentials.json loading; IS-2 and participant_config_test previously built successfully and the focused CTest passed. The embedded shortened questionnaire is now integrated into the guided flow and saves durable local drafts. A new adapter maps its completed results into the API response envelope and connects session 1 / session 2 completion to SubmissionManager. These latest source changes have not yet been built or tested. The upload endpoint configuration remains a placeholder; the tested Vercel Preview is deployment-protected and the participant uploader does not carry the smoke-test OIDC bypass token. Server-side answer validation, restart recovery, packaging, and protocol approval remain outstanding.

No real participant data should be collected using the current build.

## 2. Status summary

| Area | Status | Current note |
|---|---|---|
| Baseline tabular Q-learning | Implemented in source | Existing tests/results must be rerun against the current revision when needed. |
| Proposed memory/affect agent | Implemented in source | Do not change model design during the first participant-pipeline milestone. |
| Shared simulation/action environment | Implemented in source | Maintain identical conditions and shared physical variables. |
| Participant executable/menu | Implemented in source | One executable; menu selects Free Roam or Research Session. |
| Guided study sequence | Partially implemented | Consent, warm-up, 12 prompts, bounded free period, per-condition rating phase. |
| Stable participant identity | Partially verified | Credentials parser and A/B assignment tests pass; valid credentials loading is confirmed by the owner. Invalid-file handling, packaged release path, and restart recovery remain. |
| Questionnaire content/data model | Implemented — unverified | Active participant UI uses `companion-believability-short-v2`: 10 items per condition and 7 comparison items; shortened instrument still requires review/approval. |
| Embedded questionnaires | Implemented — unverified | Condition A/B and final comparison forms are rendered in-app and save local JSON drafts; interactive testing remains. |
| Participant response persistence | Partially implemented | Per-form drafts are atomically written; restart restoration and full-session recovery remain. |
| Local submission queue | Connected in source — unverified | Completed questionnaire results are adapted and queued; session 1 follows rating A, session 2 follows rating B plus comparison; startup retry exists. |
| Authenticated web API | Implemented in separate repo branch | IS-proj-web/test passes nine API smoke tests and synthetic Drive persistence checks; participant-client access to the protected Preview is not established. |
| Server-side questionnaire validation | Incomplete | API validates envelope metadata, authentication and idempotency, but not active questionnaire IDs/types/required fields/version. |
| Packaging and live integration | Unverified | Bundled Python, assets, endpoint, Vercel, OAuth and Drive need tests. |
| Local Google Drive questionnaire retrieval | Implemented — unverified on `Dev` | Researcher-side Python downloader and focused unit tests exist; local tests and live Drive dry-run/download remain outstanding. |
| Training and behavioural-data retrieval | Separate / not included in this feature | Training records, interaction logs, memory/affect traces and Q-table data are not downloaded by the questionnaire retrieval utility. |
| Research protocol/ethics | Not ready | Questionnaire/protocol approval and ethics gate remain. |

## 3. Milestone order

The implementation order is intentionally pipeline-first. Do not return to agent-behaviour tuning until the participant study can reliably collect, preserve, and submit the intended dataset.

1. **M0 — Documentation and shared context:** establish the source of truth. This documentation update records the current state and intended structure.
2. **M1 — Local participant identity and questionnaire workflow:** load assigned credentials; implement deterministic A/B allocation; embed the per-condition and final comparison questionnaires; define response data; persist and recover completed/partial sessions offline.
3. **M2 — Authenticated backend delivery:** connect questionnaire completion to the local queue and uploader; configure the real endpoint; strengthen API validation; test retry/idempotency.
4. **M3 — Packaging and end-to-end technical pilot:** verify clean-machine Linux/Windows packages and test both A/B orders, interruption recovery, offline retry, and Drive storage.
5. **M4 — Research protocol readiness:** approve and pilot the questionnaire, consent, experimental controls, data retention and ethics process before recruiting participants.

Detailed task checklists, acceptance criteria, unresolved decisions, and verification log are maintained in [Docs/21-participant-study-milestones.md](21-participant-study-milestones.md).

## 4. Intended component boundaries

- ParticipantMenu: choose Free Roam or Research Session.
- ParticipantConfig: load assigned participant number and private token.
- StudyController: own study phases, A/B assignment, sequencing and recovery.
- CompanionUI: render the simulation and handle companion interaction.
- QuestionnaireUI: render questions and collect validated answers.
- StudySessionData: own structured response data and local persistence.
- SubmissionManager: queue, upload, retry, and report delivery state without depending on SFML.
- Web API: authenticate, validate questionnaire schema and store submissions.

New study-specific code should be grouped under Components/Study/. Do not perform a broad refactor of the simulation. The existing submission manager currently resides in Components/Simulation/; move it only when needed for the separation above.

## 5. Important project decisions

- One participant-facing executable, with Free Roam and Research Session selected from an in-app menu.
- Participant number is assigned externally and loaded from a private per-participant credentials.json placed beside AnimalAgent in the extracted application root; it is not incremented locally.
- Odd participant numbers: A = baseline, B = proposed. Even participant numbers: A = proposed, B = baseline.
- Both conditions use the same environment, action space, scenarios and frozen policy settings.
- Upload questionnaire responses and required metadata only. Do not upload raw interaction text, memory records, affect traces, Q-tables or diagnostics by default.
- Persist locally before attempting network delivery. “Queued locally” is not the same as “stored by server”.
- Store final comparison answers in a separate comparison_questionnaire object in session 2 (Pxx-S2); session 1 carries null. This preserves the current two-submission API pattern.
- Do not collect real participant data until the supervisor/institutional ethics process, approved consent, and questionnaire/protocol requirements are satisfied.
- Synthetic rewards and policy metrics are engineering diagnostics, not perceived-believability results.

## 6. Verification rules

- A source-level implementation is not marked verified until its acceptance tests pass on the current revision.
- Historical Docs/test-results.json must not be represented as a fresh test run.
- No live Vercel deployment, Google OAuth/Drive connection, or end-to-end participant submission has been verified by the current documentation review.
- Update this roadmap and the milestone tracker whenever a milestone status changes.
- Record exact changed files, branch/revision, commands, outcomes, and known limitations.

See [README.md](../README.md) for build and launch instructions, and [project-context.md](project-context.md) for the full project hand-off.


**Build target clarification:** `IS-2` is the sole participant application CMake target and produces the executable file `AnimalAgent`. Free Roam and Research Session are runtime menu choices in that executable. `participant_config_test` and other optional targets enabled by `ANIMAL_AGENT_BUILD_RESEARCH_TESTS` are developer/test executables, not additional participant application builds.

**Questionnaire styling:** the project owner will supply a styling template for later integration. Until then, proceed with questionnaire content/data contract and functional flow; do not assume final visual styling. The embedded SFML questionnaire is source-integrated; its response adapter and SubmissionManager connection are now source-integrated but unverified. The endpoint remains a placeholder.


### M1.2 progress

The generic response model is now populated by `companion-believability-v1` in Components/Study/questionnaire-definition.h/.cpp. It defines 26 per-condition items and 8 final comparison items from `Docs/Google Form Template Companion Believability Study.md`, with stable IDs, prompts, types, required flags, options, 1–5 bounds, text limits, and scale endpoint labels. Section 2 demographic/context questions are omitted; consent is a separate gate and session metadata comes from the app. Engagement remains secondary/exploratory. The CMake application and test targets include the definition, and the focused test checks its structure. This is still an unverified draft transcription: no local build/CTest run has been reported, no SFML questionnaire UI exists yet, and supervisor/ethics approval and piloting remain necessary. The web API contract has not yet been updated; that belongs to M2.

### Embedded questionnaire UI progress — 5 November 2026

The project owner's supplied questionnaire UI has now been integrated into `Components/Questionnaire/` and connected to the guided study flow. It replaces the browser-form launch for the two condition ratings and final A/B comparison. Each completed questionnaire is saved as a separate JSON file in the participant study output directory. Demographic/familiarity items remain omitted. Conditional technical-issue and comparison-difference follow-ups are supported.

This is source-integrated but not yet verified: the project owner must build the application and run `questionnaire_content_test`, then manually test text entry, pagination, conditional follow-ups, saving, and the complete A/B/comparison sequence. The new JSON format is not yet adapted to the existing submission schema and is not uploaded. The menu's separate-window ownership and full app-level StudyController are also not solved by this change.

### Questionnaire UX and persistence revision — 5 November 2026

The questionnaire has been reduced to 10 questions per condition (A/B) and 7 comparison questions. Rating forms use two pages of four Likert items plus one overall/feedback page; comparison uses two pages. A shared aspect-preserving 1280x900 logical viewport now scales the participant menu, simulation UI, and questionnaire consistently on window resize, with centered letterboxing for mismatched aspect ratios.

Questionnaire JSON is now intended to be written on open and after every answer/navigation change, not only after final submission. A temporary-file/rename strategy protects the last saved snapshot; JSON records distinguish `in_progress` from `completed`. The participant output directory is the app's user-data `study-linkage` folder (Linux default: `~/.local/share/animal-agent/study-linkage`, unless `XDG_DATA_HOME` is set). Automatic draft restoration into the UI and backend upload are not implemented.

Current status: source changes and focused test updates are committed, but compile/runtime verification remains outstanding. Do not mark responsive layout or draft durability as verified until tested on the project owner's machine.


### Participant questionnaire submission integration — 5 November 2026

The active questionnaire result is now adapted by `Components/Study/questionnaire-submission.*` into the current API envelope. Item IDs are normalised to lowercase for compatibility with the drafted client JSON Schema; Likert, scale, choice, and text answers are mapped to the API's `likert`, `integer`, `single_choice`, and `text` answer types. Non-completed forms and unsupported answer types are rejected by the adapter.

`CompanionUI` now owns a `SubmissionManager` for Research Session:
- Rating A is atomically queued as Pxx-S1 with `comparison_questionnaire: null`.
- Rating B is retained until the final comparison is completed; then both are submitted together as Pxx-S2.
- The existing manager writes the token-bearing local queue record before upload, strips `_upload_token` from the transmitted JSON, sends Bearer authentication and the matching `Idempotency-Key`, and retains pending files after upload failure.
- Existing application startup retry remains in place. The participant event log distinguishes server acknowledgement from locally queued / pending upload.
- Developer `--study` runs without participant credentials remain local-only.

A focused `questionnaire_submission_test` target was added. **No build, CTest, SFML interaction, or participant-client HTTP test has yet been run on this revision.** The endpoint remains the placeholder in `Data/Study/upload-config.json`. The Vercel Preview used for smoke tests is protected by Deployment Protection; the participant uploader deliberately does not send the local Vercel OIDC bypass token. Do not point participant builds at that protected Preview. A participant-accessible, approved deployment endpoint is required before network submission can be verified.

The backend currently accepts arbitrary questionnaire response properties after validating the submission metadata. It must be updated to enforce the active shortened questionnaire version, question IDs, answer types, required fields, ranges/options, and payload limits before real collection. This work is not implied by the successful nine-case API smoke test.


### Research data separation and local retrieval — 5 November 2026

The research evaluation contains two distinct evidence streams. They must remain separate in collection, retrieval, processing, and reporting.

1. **Training and behavioural analysis:** concerns the agent's learning and computational behaviour, including training experiences, Q-table/learning progression, rewards, action selection, memory formation and retrieval, affective-state changes, behavioural consistency, and adaptability. These are engineering or behavioural measures used to examine how the agent operates.
2. **Perceived-believability evaluation:** concerns participants' subjective assessments of the companion, collected through the condition-specific and comparison questionnaires. Believability is a human-evaluation outcome; it is not a training metric or a proxy for learning performance.

A change in training performance must not be reported as a change in perceived believability, and a change in believability ratings must not be interpreted as evidence of improved learning. The two streams may be compared or interpreted alongside one another during the research analysis, but their measures, datasets, and conclusions must remain identifiable and distinct.

#### Local Google Drive questionnaire retrieval

A researcher-side retrieval utility has been added on `Dev`:

- `scripts/download_study_submissions.py` authenticates directly to Google OAuth using researcher credentials.
- It lists and downloads questionnaire submission JSON files from the configured Drive folder.
- It supports dry-run, participant/session filters, local output selection, and explicit overwrite.
- It validates the submission envelope, participant/session assignment, questionnaire version, answer structure, and session-specific comparison-questionnaire rule.
- It writes a retrieval report and stores downloaded data outside the repository by default.
- It does not use participant upload tokens or the participant-facing submission endpoint.
- It performs read-only operations against Google Drive; it does not modify or delete Drive files.

**Status: Implemented — unverified.** The downloader and focused unit-test file exist on `Dev`, but the unit tests have not yet been run on the current revision and no live Google Drive dry-run or download has been verified. Do not treat the feature as operational until those checks pass.

#### Explicit scope boundary

The current retrieval utility downloads **participant questionnaire submissions only**. It does not retrieve training datasets, interaction logs, Q-tables, memory records, affect traces, or diagnostic output. Those belong to a separate training/behavioural data pipeline and require their own retrieval specification if needed.

The downloader preserves the submitted JSON and does not calculate, redefine, combine, or transform the believability metric. Any adapter that makes questionnaire JSON consumable by an analysis script is a separate analysis-layer task. It must preserve the questionnaire's response values and keep perceived-believability results separate from training and behavioural measures.

The detailed retrieval specification is [22-local-study-data-retrieval.md](22-local-study-data-retrieval.md). Its acceptance criteria remain open until local tests and a live read-only Drive verification have been completed.
