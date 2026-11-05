# Implementation Status and Roadmap

**Current branch under review:** deployment/participant-client  
**Last documentation review:** 4 November 2026  
**Authoritative milestone tracker:** [21-participant-study-milestones.md](21-participant-study-milestones.md)

## 1. Current project state

The core simulated companion and its baseline/proposed agent architecture exist. The current participant application is a single CMake executable whose output name is AnimalAgent. It opens an SFML menu with **Free Roam** and **Research Session** choices.

The participant research workflow is a prototype, not a completed data-collection pipeline. Participant credentials are parsed from credentials.json beside AnimalAgent and the guided-study local counter has been removed. The project owner confirms valid credentials.json loading; IS-2 and participant_config_test build successfully and the focused CTest passes. Negative-path and packaged-release checks remain. The application still opens an external browser questionnaire, gates study event/manifest persistence behind researcher mode, and does not call the submission manager when a participant completes a questionnaire. The upload endpoint configuration is a placeholder.

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
| Questionnaire content/data model | Implemented — unverified | `companion-believability-v1` transcribes 26 per-condition items and 8 comparison items from the Docs template; demographic/context questions are omitted. No SFML questionnaire UI yet. |
| Final comparison questionnaire content | Defined — unverified | Eight comparison items are defined in the same versioned questionnaire source; rendering and workflow integration remain. |
| Participant response persistence | Incomplete | Participant mode does not currently persist study responses/manifests. |
| Local submission queue | Implemented, unconnected | Queue/retry code exists; questionnaire completion does not call it. |
| Authenticated web API | Implemented in separate repo branch | IS-proj-web/test contains HMAC token validation and Drive storage code; deployment is unverified. |
| Server-side questionnaire validation | Incomplete | Client-side response model/schema drafted; current API still accepts arbitrary response fields. Align in M2. |
| Packaging and live integration | Unverified | Bundled Python, assets, endpoint, Vercel, OAuth and Drive need tests. |
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

**Questionnaire styling:** the project owner will supply a styling template for later integration. Until then, proceed with questionnaire content/data contract and functional flow; do not assume final visual styling. The external browser form launcher remains to be replaced by an embedded SFML questionnaire.


### M1.2 progress

The generic response model is now populated by `companion-believability-v1` in Components/Study/questionnaire-definition.h/.cpp. It defines 26 per-condition items and 8 final comparison items from `Docs/Google Form Template Companion Believability Study.md`, with stable IDs, prompts, types, required flags, options, 1–5 bounds, text limits, and scale endpoint labels. Section 2 demographic/context questions are omitted; consent is a separate gate and session metadata comes from the app. Engagement remains secondary/exploratory. The CMake application and test targets include the definition, and the focused test checks its structure. This is still an unverified draft transcription: no local build/CTest run has been reported, no SFML questionnaire UI exists yet, and supervisor/ethics approval and piloting remain necessary. The web API contract has not yet been updated; that belongs to M2.

### Embedded questionnaire UI progress — 5 November 2026

The project owner's supplied questionnaire UI has now been integrated into `Components/Questionnaire/` and connected to the guided study flow. It replaces the browser-form launch for the two condition ratings and final A/B comparison. Each completed questionnaire is saved as a separate JSON file in the participant study output directory. Demographic/familiarity items remain omitted. Conditional technical-issue and comparison-difference follow-ups are supported.

This is source-integrated but not yet verified: the project owner must build the application and run `questionnaire_content_test`, then manually test text entry, pagination, conditional follow-ups, saving, and the complete A/B/comparison sequence. The new JSON format is not yet adapted to the existing submission schema and is not uploaded. The menu's separate-window ownership and full app-level StudyController are also not solved by this change.

### Questionnaire UX and persistence revision — 5 November 2026

The questionnaire has been reduced to 10 questions per condition (A/B) and 7 comparison questions. Rating forms use two pages of four Likert items plus one overall/feedback page; comparison uses two pages. A shared aspect-preserving 1280x900 logical viewport now scales the participant menu, simulation UI, and questionnaire consistently on window resize, with centered letterboxing for mismatched aspect ratios.

Questionnaire JSON is now intended to be written on open and after every answer/navigation change, not only after final submission. A temporary-file/rename strategy protects the last saved snapshot; JSON records distinguish `in_progress` from `completed`. The participant output directory is the app's user-data `study-linkage` folder (Linux default: `~/.local/share/animal-agent/study-linkage`, unless `XDG_DATA_HOME` is set). Automatic draft restoration into the UI and backend upload are not implemented.

Current status: source changes and focused test updates are committed, but compile/runtime verification remains outstanding. Do not mark responsive layout or draft durability as verified until tested on the project owner's machine.
