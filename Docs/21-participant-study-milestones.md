# Participant Study Milestones

**Project:** Animal Agent — simulated companion research prototype  
**Research title:** Investigating the Effect of Memory and Affective State on the Perceived Believability of a Simulated Companion Agent  
**Tracking branch:** SAmWaM42/IS-proj — deployment/participant-client  
**Last status review:** 4 November 2026  
**Document role:** Live implementation tracker. Update this file as work is completed; do not mark an item complete without evidence from the current revision.

---

## 1. How to use this tracker

This document is the working plan for completing the participant research pipeline. It complements, rather than replaces, the detailed component specifications.

For each task:

1. Inspect the current implementation and relevant specification.
2. Confirm the expected behaviour and data contract.
3. Implement one small increment.
4. Build and run focused tests.
5. Record the changed files, test command, result, and any remaining limitation.
6. Update the status here and update the README/context document if a lasting decision changed.

**Status meanings**

- **Not started:** no implementation work has been completed.
- **In progress:** implementation has begun but acceptance criteria are not all met.
- **Blocked:** cannot proceed until a named dependency or decision is resolved.
- **Implemented — unverified:** code exists, but the required checks have not passed on the current revision.
- **Verified:** acceptance criteria passed on the current revision and evidence is recorded below.
- **Deferred:** intentionally moved outside the current project scope.

Do not equate “code exists” with “works”, “queued” with “uploaded”, or “engineering metric” with “research outcome”.

## 2. Project goal and study protocol

The participant-facing application must support one complete, recoverable research session in one executable:

~~~text
Launch AnimalAgent
  |
  +-- Free Roam
  |     +-- Ordinary companion interaction
  |
  +-- Research Session
        +-- Load participant credentials
        +-- Consent
        +-- Condition A
        |     +-- Guided interactions
        |     +-- Bounded free interaction
        |     +-- Embedded questionnaire A
        +-- Condition B
        |     +-- Same guided interactions
        |     +-- Bounded free interaction
        |     +-- Embedded questionnaire B
        +-- Embedded A/B comparison questionnaire
        +-- Persist completed response data
        +-- Queue and submit questionnaire dataset
        +-- Show acknowledged / pending result
~~~

### 2.1 Experimental controls

- Odd participant IDs: A = baseline, B = proposed.
- Even participant IDs: A = proposed, B = baseline.
- The assigned participant ID is stable across the entire session and is loaded from private credentials, not generated locally.
- Both conditions use the same simulation environment, action vocabulary, scripted interactions, instructions, and scenario timing.
- Learning is disabled and exploration is zero during participant sessions.
- The proposed agent starts from a clean per-participant memory state; no participant inherits another participant's memory.
- Shared physical/environmental variables (including energy) must be treated identically by both conditions.
- Internal agent state, memory contents, Q-values, and condition labels must not be exposed in the participant-facing UI.
- The uploaded dataset contains questionnaire responses and required metadata only. Raw interaction text, memory records, affect traces, Q-table data, and debug logs are excluded unless the research protocol is explicitly revised and approved.

### 2.2 Decisions still requiring confirmation

| ID | Decision | Current recommendation | Status |
|---|---|---|---|
| D-01 | Should the final A/B comparison be stored as a separate submission or included with session 2? | Include a separate comparison object in Pxx-S2 to preserve the current two-session API contract. | **Proposed — confirm before backend schema work** |
| D-02 | Should any derived behavioural metrics be uploaded in addition to questionnaires? | No. Keep uploads questionnaire-only unless a specific metric, analysis purpose, consent basis, and retention rule are agreed. | **Default recommendation; confirm if scope changes** |
| D-03 | Exact questionnaire items, response scales, and mandatory/optional fields. | Use a versioned questionnaire definition approved by the supervisor/protocol; do not invent items during implementation. | **Blocked on approved questionnaire content** |
| D-04 | Where should participant credentials live? | userDataRoot/credentials.json, installed individually and excluded from the repository/common package. | **Design recommendation; validate against distribution workflow** |

These decisions do not block implementing the generic questionnaire renderer or participant-credential parser, but they must be resolved before final backend validation and real data collection.

## 3. Intended architecture

The goal is to remove study orchestration and data ownership from the rendering class without rewriting the simulation.

~~~text
main.c++
  |
  +-- RuntimePaths
  |
  +-- SubmissionManager
  |     +-- retry pending local submissions on startup
  |
  +-- ParticipantMenu
        |
        +-- Free Roam
        |     +-- CompanionUI
        |
        +-- Research Session
              |
              +-- ParticipantConfig
              |     +-- participant number
              |     +-- private upload token
              |
              +-- StudyController
              |     +-- consent / phase transitions
              |     +-- deterministic A/B assignment
              |     +-- condition sequencing
              |     +-- resume / withdrawal handling
              |
              +-- CompanionUI
              |     +-- render simulation
              |     +-- expose interaction events/results
              |
              +-- QuestionnaireUI
              |     +-- condition A ratings
              |     +-- condition B ratings
              |     +-- final comparison
              |
              +-- StudySessionData
                    +-- validated response model
                    +-- local persistence / recovery
                    |
                    +-- SubmissionManager
                          +-- atomic pending JSON
                          +-- upload_submission.py
                                +-- IS-proj-web API
                                      +-- HMAC participant auth
                                      +-- questionnaire validation
                                      +-- Google Drive storage
~~~

### 3.1 Target file layout

New study-specific responsibilities should be grouped under Components/Study/:

~~~text
Components/
  Simulation/
    participant-menu.h/.cpp
    companion-ui.h/.cpp
    runtime-paths.h
    backyard-environment.h/.cpp
  Study/
    participant-config.h/.cpp
    study-controller.h/.cpp
    questionnaire-ui.h/.cpp
    study-session-data.h/.cpp
    submission-manager.h/.cpp       # relocate only when justified
  NLP/
    nlp_intent_service.py
    upload_submission.py
  Memory/
  State-Action/
Data/
  Study/
    upload-config.json              # endpoint only; no secrets
    questionnaire-config.json       # approved question definitions/version
    credentials.example.json        # placeholder format only, no real token
    (real credentials installed in OS user-data directory)
~~~

This is an intended structure, not a list of files already present. The current SubmissionManager is under Components/Simulation/; do not move it as a standalone cleanup task. New components should be added only when their responsibility is clear and independently testable.

### 3.2 Responsibility boundaries

- **StudyController:** owns sequence and condition assignment. It must not draw SFML controls.
- **QuestionnaireUI:** renders and validates answers. It must not know about HTTP, tokens, or Google Drive.
- **StudySessionData:** owns response values, schema version, validation, and serialisation. It must not own rendering.
- **SubmissionManager:** owns durable local queueing and delivery. It must not depend on SFML.
- **CompanionUI:** owns the simulation scene and interaction experience. It should expose structured events to the study controller rather than writing research submissions itself.
- **Web API:** owns server-side authentication, strict schema validation, idempotency handling, and storage.

Avoid coupling these components through global variables or direct file writes from UI drawing/event code.

## 4. Current baseline status (before Milestone 1)

The following reflects static source inspection of deployment/participant-client. No build, runtime, deployment, or end-to-end submission test was performed as part of this documentation update.

| Area | Current status | Evidence / limitation |
|---|---|---|
| Participant executable and menu | Implemented in source | CMake target IS-2 outputs AnimalAgent; participant build starts the menu without CLI arguments. |
| Free Roam path | Implemented in source | Menu launches ordinary CompanionUI. |
| Guided Research Session | Implemented in source | Consent, warm-up, 12 guided prompts, bounded free-interaction phase, per-condition rating phase. |
| Counterbalancing | Partially implemented | Uses a machine-local counter file; not a stable distributed participant ID. |
| Participant credentials | Missing | No participant credentials loader is wired into participant startup. |
| Embedded questionnaires | Missing | openStudyForm() launches an external browser form. |
| Final comparison questionnaire | Missing | No embedded final comparison questionnaire in current flow. |
| Participant response persistence | Incomplete | Participant mode uses researcherMode=false; study event/manifest writes are gated on researcher mode. |
| Submission queue | Implemented, unconnected | SubmissionManager can queue and upload JSON; no questionnaire-completion call currently connects it to the study flow. |
| Startup retry | Implemented in source | Participant startup calls uploadPendingSubmissions(). |
| Upload configuration | Incomplete | Data/Study/upload-config.json contains a placeholder endpoint. |
| Backend authentication | Implemented in web test branch | HMAC-SHA256 digest map and bearer token validation exist; production deployment is unverified. |
| Backend questionnaire validation | Incomplete | API validates metadata/assignment but accepts arbitrary extra response properties. |
| Google Drive storage | Implemented in source, unverified live | OAuth/Drive code exists; no live credentials/deployment test performed. |
| Packaging | Unverified | Participant uploader expects bundled Python runtime; assets/configuration/credentials need packaging checks. |
| Ethics and study readiness | Not complete | Approval, final questionnaire, and pilot remain prerequisites. |

## 5. Milestone tracker

### M0 — Documentation and shared project context

**Status: completed and verified on the target branch (documentation only).**

Goal: establish one consistent source of truth before implementation resumes.

- [x] Record current source-level implementation status.
- [x] Record intended participant workflow and component responsibilities.
- [x] Record A/B counterbalancing and data-handling constraints.
- [x] Separate current implementation from intended architecture.
- [x] Add milestone tracker and link it from README/context/roadmap.
- [x] Verify the documentation updates are visible on the target branch.

Completion evidence (all files re-fetched successfully from the target branch after writing):
- README.md: b607938e2087a9cca93fc32f2818e89960c482ee
- Docs/project-context.md: 66c2a3c76a5b0947f5a3b3bd463c0ea66fdf685c
- Docs/07-implementation-status-and-roadmap.md: 8cc1db3b37afe48d3e040d78c2c3e2458427adb4
- Docs/21-participant-study-milestones.md: 84feb1333516dba21bf154b6ee18b7eaca064430

The re-fetch confirms the files are present on deployment/participant-client. The milestone file was then updated to record this verification. No application build or runtime test is implied by M0.

### M1 — Complete local participant identity and questionnaire workflow

**Status: next milestone.**

Goal: a participant can complete the full research sequence offline, with the correct assigned identity and all questionnaire answers saved locally. No backend deployment is required to pass M1.

#### M1.1 — Participant configuration and deterministic assignment

- [ ] Define and document the exact credentials JSON schema.
- [ ] Load credentials from the agreed per-user data path.
- [ ] Validate participant number range and non-empty token; reject malformed/missing credentials with a clear participant-facing error.
- [ ] Stop using the local incrementing counter for participant allocation.
- [ ] Derive A/B mapping from participant number: odd = baseline first; even = proposed first.
- [ ] Keep the participant number and run/session identifier stable through both conditions and restart recovery.
- [ ] Ensure no real credentials are written to logs, manifests, source control, or common packages.

**Acceptance criteria**
- Participant 1 loads baseline as A and proposed as B.
- Participant 2 loads proposed as A and baseline as B.
- Invalid/missing credentials prevent entry into Research Session but do not prevent Free Roam.
- Restarting the same participant does not allocate a new ID or reverse the condition order.

#### M1.2 — Questionnaire content and data contract

- [ ] Obtain the final approved per-condition questionnaire items and response scales.
- [ ] Obtain the final approved A/B comparison items.
- [ ] Define required/optional questions, allowed values, unanswered handling, and questionnaire version.
- [ ] Define a typed JSON representation in StudySessionData.
- [ ] Decide D-01 (comparison object bundled into session 2 or separate submission).
- [ ] Confirm D-02 (questionnaire-only upload; current recommendation is yes).

**Acceptance criteria**
- Every question has a stable key, type, prompt, allowed answer range/options, and required/optional rule.
- Invalid or missing required answers cannot be marked complete.
- The payload records the questionnaire version and distinguishes condition A, condition B, and comparison answers.
- The UI does not expose internal condition identity (baseline/proposed) to participants.

#### M1.3 — Embedded questionnaire UI

- [ ] Replace external browser form launch with an SFML questionnaire screen.
- [ ] Support one question or a small page of questions at a time, with progress indication.
- [ ] Support the approved response controls (e.g. Likert selection) and clear selected-state feedback.
- [ ] Validate required responses before allowing completion.
- [ ] Add condition A, condition B, and final comparison stages.
- [ ] Ensure the same per-condition questionnaire is used after both conditions.
- [ ] Ensure questionnaire completion returns structured data to the study controller rather than writing files from UI code.

**Acceptance criteria**
- The entire study can be completed without opening a browser.
- A participant cannot accidentally skip a required response.
- The comparison stage appears only after both condition questionnaires.
- Closing or withdrawing before completion does not create a falsely complete submission.

#### M1.4 — Study data ownership, local persistence, and recovery

- [ ] Introduce StudySessionData as the owner of participant metadata and responses.
- [ ] Persist a response draft atomically after each completed questionnaire/stage.
- [ ] Keep the draft under the OS user-data directory, not the repository working tree.
- [ ] Define safe resume behaviour after an application crash/restart.
- [ ] Keep questionnaire answers separate from simulation memory, affect state, Q-tables, and diagnostics.
- [ ] Define deletion/retention behaviour for incomplete or withdrawn sessions in line with the approved protocol.

**Acceptance criteria**
- Completing condition A and restarting does not lose A responses or reassign the participant.
- A partial questionnaire is not submitted as a complete response.
- A completed local dataset can be parsed and validated independently of SFML.
- Writes are atomic or recoverable; a truncated JSON file does not silently appear complete.

#### M1.5 — Offline milestone verification

- [ ] Add focused tests for credentials parsing and odd/even assignment.
- [ ] Add tests for questionnaire validation and serialisation.
- [ ] Add tests for partial save/reload and completed-session state.
- [ ] Build the participant executable.
- [ ] Manually complete a full offline session using test credentials and inspect the saved JSON.
- [ ] Confirm no external form/browser is opened.

**M1 exit gate:** all M1 acceptance criteria pass using synthetic/test credentials and no live participant data.

### M2 — Connect local responses to the authenticated web backend

**Goal:** a completed local questionnaire dataset is securely queued and delivered to the canonical backend without data loss.

- [ ] Confirm the backend branch and promote/review its implementation before production deployment. Current implementation is in IS-proj-web/test; its README says it is not yet the production deployment target.
- [ ] Configure and verify Vercel environment variables: Google OAuth values, Drive folder ID, HMAC SEED, and PARTICIPANT_TOKENS digest map.
- [ ] Generate participant tokens privately; distribute only each participant's own token.
- [ ] Configure the real endpoint in Data/Study/upload-config.json.
- [ ] Connect questionnaire completion to SubmissionManager::queueSubmission().
- [ ] Send session 1 after condition A questionnaire completion.
- [ ] Send session 2 after condition B and final comparison completion (if D-01 is confirmed).
- [ ] Preserve the pending JSON when network, authentication, or server errors occur.
- [ ] Retry pending submissions on startup and expose a clear status to the participant/researcher.
- [ ] Keep token out of the HTTP JSON body; send it only as bearer authentication.
- [ ] Restrict local pending-file permissions where supported and avoid token logging.
- [ ] Add strict backend validation for questionnaire keys, types, ranges, required fields, questionnaire version, and maximum payload size.
- [ ] Test invalid token, wrong A/B mapping, malformed payload, server unavailable, lost acknowledgement, and ordinary duplicate retry.

**Acceptance criteria**
- Valid session 1 and session 2 payloads are accepted for both odd/even assignment.
- Invalid tokens and malformed questionnaire payloads are rejected.
- A failed upload remains locally queued and succeeds on a later retry.
- A successful acknowledgement removes or marks the pending item as delivered.
- Repeating the same submission ID does not create an ordinary duplicate Drive record.
- UI distinguishes local queueing from server acknowledgement.

**Known backend limitation:** Drive lookup-before-create is not atomic; simultaneous duplicate requests may race. This is acceptable only as a documented pilot-scale limitation unless strict uniqueness is required.

### M3 — Packaging and end-to-end technical pilot

**Goal:** the packaged participant application works on a clean supported machine and the entire submission path is verified.

- [ ] Verify Linux package layout: executable, SFML runtime libraries, assets, fonts, uploader, Python runtime, and configuration.
- [ ] Verify Windows package layout and process invocation.
- [ ] Confirm credentials are installed separately per participant and are not included in the common ZIP.
- [ ] Test participant 1 (baseline → proposed) from launch through server acknowledgement.
- [ ] Test participant 2 (proposed → baseline) from launch through server acknowledgement.
- [ ] Test app restart during each study phase and verify recovery behaviour.
- [ ] Test offline completion, queued upload, and later retry.
- [ ] Test duplicate retry and server rejection.
- [ ] Verify stored Drive JSON contains only the approved response schema and metadata.
- [ ] Verify condition mapping can be decoded for analysis without revealing it in the participant UI.
- [ ] Record exact tested OS, build revision, package contents, endpoint revision, and test results.

**M3 exit gate:** repeatable technical pilot on each supported OS, with no real participant recruitment implied.

### M4 — Research protocol readiness and data collection gate

**Goal:** ensure the experiment is ethically approved, interpretable, and ready for controlled participant use.

- [ ] Define perceived believability and justify the questionnaire items/scales with relevant literature.
- [ ] Confirm behavioural consistency/adaptability measures are operationally defined if retained as outcomes.
- [ ] Obtain supervisor/institutional ethics approval and approved consent materials.
- [ ] Pilot the wording, timing, scenario equivalence, and participant burden.
- [ ] Verify both conditions receive equivalent instructions and interaction opportunities.
- [ ] Freeze software revision, policies, scenarios, questionnaire version, and backend schema before recruitment.
- [ ] Define anonymisation, access control, retention, deletion, and analysis procedures.
- [ ] Confirm no unapproved telemetry or sensitive data is collected.
- [ ] Run a small approved pilot and review missing data / failure cases before the main study.

**M4 exit gate:** explicit supervisor/institutional approval and a signed-off pilot protocol. Software readiness alone is not ethics approval.

## 6. Data and API contract notes

### 6.1 Current backend metadata contract

The current IS-proj-web/test API expects:

- schema_version: 1
- submission_id: "P{participant}-S{session}"
- participant_number: 1..9999
- session_index: 1 | 2
- condition: "A" | "B"
- Idempotency-Key header exactly matching submission_id
- Authorization: Bearer <participant-token>

The backend validates that odd participant numbers have session 1 = A and session 2 = B, while even numbers have session 1 = B and session 2 = A. The current server does not yet validate the questionnaire response fields beyond the metadata; that must be tightened in M2.

### 6.2 Proposed questionnaire payload shape

This is a conceptual shape, not yet an approved schema:

~~~json
{
  "schema_version": 1,
  "submission_id": "P03-S1",
  "participant_number": 3,
  "session_index": 1,
  "condition": "A",
  "questionnaire_version": "believability-v1",
  "condition_responses": {
    "q1": 4,
    "q2": 5
  }
}
~~~

If D-01 is confirmed, the second submission adds a separate comparison property:

~~~json
{
  "schema_version": 1,
  "submission_id": "P03-S2",
  "participant_number": 3,
  "session_index": 2,
  "condition": "B",
  "questionnaire_version": "believability-v1",
  "condition_responses": {
    "q1": 3,
    "q2": 4
  },
  "comparison": {
    "comparison_questionnaire_version": "comparison-v1",
    "responses": {
      "c1": "A",
      "c2": "B"
    }
  }
}
~~~

The exact keys, types, values, and required fields must be derived from the approved questionnaire. Do not upload raw free-text interaction logs or internal model state by default.

### 6.3 Credential handling

Proposed participant-local file:

~~~json
{
  "participant_number": 3,
  "token": "participant-private-token"
}
~~~

This is a format illustration only. Real credentials must be generated by the backend token-generation process, distributed privately, stored in the participant's OS user-data directory, and excluded from Git and the common application package.

## 7. Verification and decision log

### Verification log

| Date | Revision | Check | Result | Notes |
|---|---|---|---|---|
| 2026-11-04 | deployment/participant-client (source inspected; exact HEAD not recorded here) | Static source inspection of startup, menu, study flow, persistence, uploader, and config | Findings recorded; not a runtime test | No build, deployment, OAuth/Drive, or end-to-end test was run during this documentation update. |

Add one row per meaningful verification. Include command, environment, and exact revision wherever possible.

### Decision log

| Date | Decision | Rationale / effect |
|---|---|---|
| 2026-11-04 | One participant executable with menu-selected Free Roam and Research Session | Keeps participant distribution simple; mode selection is not a separate build target. |
| 2026-11-04 | Use assigned participant ID, not a per-machine counter | Makes counterbalancing stable across machines and reinstalls. |
| 2026-11-04 | Keep uploads questionnaire-only by default | Matches the current backend README and minimises unnecessary research data collection. |
| 2026-11-04 | Keep submission manager independent of SFML | Separates persistence/delivery from UI and improves offline recovery/testing. |
| 2026-11-04 | Treat embedded questionnaire and final comparison as required study-flow work | External browser launch cannot verify submission and breaks the intended single-app experience. |

## 8. Next implementation task

Start M1 with **M1.1 — Participant configuration and deterministic assignment**.

Before editing, inspect the current participant startup and guided-study initialization again. Implement the credential schema/loader and assignment tests as a small increment. Do not yet change the agent's memory/affect model, Q-learning policy, scenario design, or visual theme; those are outside the first milestone.
