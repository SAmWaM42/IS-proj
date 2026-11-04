# Implementation Status and Roadmap

**Current branch under review:** deployment/participant-client  
**Last documentation review:** 4 November 2026  
**Authoritative milestone tracker:** [21-participant-study-milestones.md](21-participant-study-milestones.md)

## 1. Current project state

The core simulated companion and its baseline/proposed agent architecture exist. The current participant application is a single CMake executable whose output name is AnimalAgent. It opens an SFML menu with **Free Roam** and **Research Session** choices.

The participant research workflow is a prototype, not a completed data-collection pipeline. Participant credentials are parsed from credentials.json beside AnimalAgent and the guided-study local counter has been removed. The project owner reports successful builds of IS-2 and participant_config_test, a passing focused CTest run, and valid credentials loading in the application; negative-path and packaged-release checks remain. The application still opens an external browser questionnaire, gates study event/manifest persistence behind researcher mode, and does not call the submission manager when a participant completes a questionnaire. The upload endpoint configuration is a placeholder.

No real participant data should be collected using the current build.

## 2. Status summary

| Area | Status | Current note |
|---|---|---|
| Baseline tabular Q-learning | Implemented in source | Existing tests/results must be rerun against the current revision when needed. |
| Proposed memory/affect agent | Implemented in source | Do not change model design during the first participant-pipeline milestone. |
| Shared simulation/action environment | Implemented in source | Maintain identical conditions and shared physical variables. |
| Participant executable/menu | Implemented in source | One executable; menu selects Free Roam or Research Session. |
| Guided study sequence | Partially implemented | Consent, warm-up, 12 prompts, bounded free period, per-condition rating phase. |
| Stable participant identity | Partially verified | Credentials parser and A/B assignment tests pass; valid credentials loading appears to work. Invalid-file UI, packaged release path, and restart recovery remain. |
| Embedded questionnaires | Missing | Current flow launches external browser forms. |
| Final comparison questionnaire | Missing | Must be added to the in-app sequence. |
| Participant response persistence | Incomplete | Participant mode does not currently persist study responses/manifests. |
| Local submission queue | Implemented, unconnected | Queue/retry code exists; questionnaire completion does not call it. |
| Authenticated web API | Implemented in separate repo branch | IS-proj-web/test contains HMAC token validation and Drive storage code; deployment is unverified. |
| Server-side questionnaire validation | Incomplete | Metadata is validated; arbitrary response fields are currently accepted. |
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
- Keep the final comparison questionnaire in the data contract; whether it is bundled into session 2 or sent separately remains to be confirmed.
- Do not collect real participant data until the supervisor/institutional ethics process, approved consent, and questionnaire/protocol requirements are satisfied.
- Synthetic rewards and policy metrics are engineering diagnostics, not perceived-believability results.

## 6. Verification rules

- A source-level implementation is not marked verified until its acceptance tests pass on the current revision.
- Historical Docs/test-results.json must not be represented as a fresh test run.
- No live Vercel deployment, Google OAuth/Drive connection, or end-to-end participant submission has been verified by the current documentation review.
- Update this roadmap and the milestone tracker whenever a milestone status changes.
- Record exact changed files, branch/revision, commands, outcomes, and known limitations.

See [README.md](../README.md) for build and launch instructions, and [project-context.md](project-context.md) for the full project hand-off.
