# Implementation Status and Roadmap

**Current branch under review:** `Dev` (researcher-side data retrieval work is being reviewed)  
**Last documentation review:** 12 November 2026  
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
| Questionnaire JSON analysis and visualisation | Implemented — unverified on `Dev` | Researcher-side analysis script validates downloaded envelopes, pairs sessions, reports question-level descriptive summaries/A-B comparisons, applies the active attention check and emits SVG charts; synthetic tests must be run locally. |
| Training and behavioural-data retrieval | Separate / not included in this feature | Training records, interaction logs, memory/affect traces and Q-table data are not downloaded or analysed by the questionnaire JSON utility. |
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

## 7. Training experience redesign — planned, not implemented

The training interaction generator is currently a bounded, scripted sequence of 14 observation contexts. Some fields and outcomes are randomised, but this is not equivalent to randomising the order and composition of interactions, and it does not provide a synthetic user that reacts to the companion's behaviour. The current training-coverage plot measures the proportion of decisions with stored valid Q-values; it does not establish broad state-space or state-action coverage.

The next training work is deliberately split into two stages. **Complete and verify Stage A and Stage B before beginning the synthetic-user implementation.** Do not expand the Q-learning architecture or introduce a separate memory selector as a substitute for improving the training experience.

### Stage A — Scenario composition and shared environment constraints

**Status: Planned. No implementation or test is implied by this roadmap entry.**

Retain the existing scenario contexts initially, but replace the single fixed 14-step ordering with configurable episode composition:

- Randomise scenario order rather than relying on a fixed `stepIndex % 14` cycle.
- Randomise which eligible scenarios appear in an episode and vary episode length within defined bounds.
- Vary initial environment conditions and scenario parameters within valid ranges, including proximity, social context, energy-related circumstances, and feedback/outcome variation.
- Include positive, negative, and neutral feedback opportunities, fetch/play/social/rest contexts, ignored requests, and competing needs.
- Use reproducible random seeds and record the seed plus scenario composition for each episode so runs can be reproduced and compared.
- Keep training exposure comparable between Baseline and Proposed: both receive the same scenario schedule, initial shared environment conditions, and random outcome stream where practical. Differences in the agents' internal memory/affect must not accidentally give them different scenario opportunities.
- Record scenario frequencies, episode composition, unique states, unique valid state-action pairs, per-pair visit counts, and Q-table-supported versus fallback decisions. Report global unique-state unions across the run separately from sums of per-episode unique counts. Treat decision coverage, state coverage, and state-action coverage as distinct measures.

#### Repeated behaviour, ignored requests, and INSIST

Add an explicit scenario for repeated agent behaviour when the environment or user provides no intended response:

1. The companion initiates or repeats an interaction/action.
2. The environment/user supplies no acknowledgement, reward, task progress, or other intended response.
3. The companion may retry within a defined limit or interval.
4. Continued non-response makes an `INSIST` response eligible through the shared action-validity/interaction contract.
5. The scenario ends or changes when a response arrives, the retry limit is reached, or another environment constraint takes priority.

Define precisely what counts as a repeated attempt, what constitutes a response, how long the agent waits before retrying, and when `INSIST` becomes valid. Avoid making `INSIST` an unconditional consequence of any repeated action. Verify the complete path from observation generation through action validity and selection to the visible behaviour in the simulation. Both agents must have the same opportunity to encounter this scenario; their policies may still choose different valid actions.

#### Repetition penalty / environmental boredom constraint

Introduce a shared, environment-owned repetition/boredom signal that reduces the reward for repeatedly performing actions of the same type without meaningful variation or an intervening response.

- Apply the same environmental rule and parameters to Baseline and Proposed. It must not depend on access to the Proposed agent's semantic memory or affective state.
- Track repetition by a documented action category (for example, repeated social bids or repeated fetch attempts), using a defined recent-history window or consecutive-repeat count.
- Increase the penalty as repetition continues, subject to a bounded maximum; define how the signal recovers after a different action, a meaningful user/environment response, or elapsed time.
- Distinguish an action that is repeated because the environment has not responded from a genuinely successful repeated interaction. Do not penalise productive repetition indiscriminately.
- Keep this penalty in the shared environment/reward calculation, not as a Proposed-only affect or memory reward adjustment. Log the repetition count/signal and penalty contribution so its effect can be inspected.
- Test that identical action histories under identical environment conditions receive identical repetition penalties for both agents, and that the penalty does not override essential survival/energy constraints or make all repeated actions unrewarding.

This is an environmental constraint intended to model diminishing returns or boredom from repetitive interaction. It is not itself evidence that the agent has developed an internal emotional state.

**Stage A acceptance checks**

- Scenario order and episode composition vary across seeds, while remaining reproducible for a fixed seed.
- Both agents receive matched scenario opportunities and shared environment conditions.
- Ignored-request sequences can reach the intended `INSIST` validity/behaviour path, and the path is tested end-to-end.
- The repetition penalty is shared, bounded, observable in diagnostics, and identical for equivalent histories in both conditions.
- Coverage reporting distinguishes decisions with Q-values, unique states, and unique valid state-action pairs across the full run.
- Existing training, transition, reward, and agent tests pass; a controlled run confirms the new scenario distribution and diagnostics.

### Stage B — Synthetic user model

**Status: Planned. Begin only after Stage A is implemented, tested, and its training outputs reviewed.**

Add a small, explicit synthetic user model that reacts to companion behaviour and generates subsequent interaction/feedback. It should be separate from the companion agent and should not expose its hidden profile directly to either policy.

The initial user profile should be deliberately small and interpretable, with configurable characteristics such as:

- Preferred and disliked activities or objects.
- Positive, negative, and neutral feedback tendencies.
- Likelihood of responding to social bids or repeated requests.
- Tolerance for repeated unwanted behaviour.
- Optional preference changes or conflicting statements, introduced only where the scenario requires them.

The interaction loop should become:

`user profile and current user context → user/environment interaction → companion observation → companion action → synthetic-user response → shared environment update → agent memory/affect update → next observation`

Requirements:

- Both agents interact with the same synthetic user profile, user-response rules, scenario seed, and equivalent opportunity stream.
- The user model responds to the selected action and relevant context; it must not simply replay a fixed answer sequence.
- The Proposed agent may store and use semantic preference statements and action-outcome memories. The Baseline must not receive those internal memory/affect features.
- User preferences should be introduced through explicit, logged interaction events. Include repeated statements and, in a controlled subset of scenarios, changed or conflicting preferences so memory handling can be evaluated.
- Keep synthetic-user preferences and response state separate from the companion's internal memory and affect. The user model is the source of user-side behaviour; the agent must infer relevant information from observations.
- Preserve the current reward model initially where possible. If user-response modelling requires reward changes, document and test those changes separately rather than changing the user model, reward model, and agent state representation simultaneously.
- Define episode reset boundaries explicitly: specify which user-profile properties persist, which user-session properties reset, and whether the companion's memory/affect persists. Avoid accidental persistence mismatches between the user and companion.

**Stage B acceptance checks**

- The same profile, seed, and action history produce reproducible user responses.
- Different profiles can produce meaningfully different feedback and follow-up interactions.
- The synthetic user reacts to companion actions rather than following a predetermined interaction list.
- Both agents receive matched external experiences under equivalent actions and conditions.
- Preference statements are observable to the agent but hidden profile fields are not directly exposed.
- Tests cover preference formation, negative preferences, non-response, repeated behaviour, preference changes/conflicts, and episode reset/persistence rules.
- Training diagnostics demonstrate that the new interaction loop generates varied, multi-step histories and that memory/affect-relevant situations occur at measurable frequencies.

### Implementation order and boundaries

1. Complete Stage A scenario randomisation/composition and reproducible logging.
2. Implement and verify the ignored-response / repeated-behaviour / `INSIST` scenario.
3. Implement and verify the shared environmental repetition/boredom reward constraint.
4. Extend coverage and scenario-distribution reporting; run controlled training comparisons and review the resulting traces.
5. Only after Stage A acceptance checks pass, implement Stage B synthetic users.
6. Re-run training and validation for both agents using matched profiles and seeds. Preserve a clear distinction between training reward/behavioural diagnostics and participant-perceived-believability outcomes.

These are researcher-side training changes on `Dev`. Any change affecting agent behaviour, state transitions, reward, action vocabulary, or simulation structure must also be reflected in `deployment/participant-client` and built/tested there under the project's deployment-parity requirement. Do not modify or redeploy the production Vercel project as part of this work without explicit approval. The synthetic-user model is a training tool unless a separate, approved participant-study change is specified.

### Stage A implementation progress — 6 November 2026

**Status: Source-integrated on `Dev`; build/runtime validation outstanding. Deployment branch unchanged.**

The first four Stage A work items have now been implemented in the researcher-side training path:

1. **Scenario randomisation and composition**
   - Replaced the fixed `stepIndex % 14` schedule with seeded shuffled scenario blocks.
   - Scenario blocks can be probabilistically omitted, while multi-step fetch retry and ignored-request sequences are preserved.
   - Episode lengths vary between approximately 75% and 125% of the requested step count (bounded to at least one step).
   - Episode starting energy, stress, and user-satisfaction values vary within bounded ranges and are shared by both agents.
   - Each episode records its scenario seed, starting environment values, and complete observation composition. Transition randomness is also seeded per run and paired by episode/step.
   - Both agents are run against the same observations, elapsed intervals, initial conditions, and transition random stream.

2. **Repeated no-response scenario and delayed INSIST eligibility**
   - Added a three-observation pending-task sequence with no user response.
   - Added `no_response_count` to the shared environment observation and encoded state for both agents.
   - `INSIST` is unavailable on the first two no-response observations and becomes valid when the count reaches two.
   - This creates an opportunity for either policy to select INSIST after persistent non-response; it does not force the action.
   - The visible simulation mapping already exists, but end-to-end runtime behaviour still needs validation.

3. **Shared environmental repetition/boredom reward constraint**
   - Added a shared training-environment penalty based on consecutive action categories when no meaningful user/environment response occurs.
   - First occurrence is unpenalised; repeated categories incur an additional 0.04 penalty per repeat, capped at 0.20.
   - A meaningful response or change of action category resets the consecutive-repeat counter.
   - The same reward calculation is applied to Baseline and Proposed. Per-step logs record category, consecutive count, penalty, and response flag.
   - This is an environmental reward constraint, not a Proposed-only affect or memory update.

4. **Coverage and scenario diagnostics**
   - Episode metrics now report unique selected state-action pairs, valid state-action pairs available on encountered states, and their coverage ratio.
   - Run summaries report global unions of unique states and state-action pairs, separately for each agent, rather than summing episode-level unique counts.
   - Per-episode scenario composition and seeds are retained for reproducibility.
   - The existing Q-table-supported decision rate remains a separate metric; it must not be described as state-space coverage.

Focused self-tests were added to the existing training/agent self-test paths for deterministic scenario generation, delayed INSIST eligibility, shared bounded repetition penalties, and state encoding of the no-response count. **These tests have not yet been compiled or executed on the current revision.** The code must not be considered validated until the researcher runs a clean development build, `--self-test`, the paired-agent scenario tests, and controlled train/validate runs with multiple seeds.

**Q-table compatibility:** adding `no_response_count` changes the encoded state key. Existing Q-table checkpoints were created under the previous state encoding and must not be reused for the updated training experiment. Start with fresh policies using `--train`; do not use `--resume-training` with pre-change checkpoints.

**Deployment boundary:** all changes are currently limited to `Dev`. Do not cherry-pick or port them to `deployment/participant-client` until build, self-test, paired-condition, scenario-distribution, reward-parity, and runtime INSIST validation have passed and the project owner explicitly authorises the deployment update. No production deployment was changed.


---

## Cross-branch documentation and implementation status — 11 November 2026

This document tracks researcher-side implementation on `Dev`. It does not describe the participant release or web backend as if they were part of this branch. See [Current Implementation Map](00-current-implementation-map.md) for the repository split and current verification caveats.

- **Dev:** researcher-side C++ agent/simulation, Q-learning, randomized scripted training, paired evaluation, detailed training traces, offline training dashboard, researcher questionnaire retrieval and descriptive questionnaire analysis are present in source. Verify the current build/tests and run artifacts before describing a specific experiment as validated.
- **Participant runtime:** maintained separately on `deployment/participant-client`. It contains the menu, General Interaction and Believability Study modes, participant credentials, in-app questionnaires, local submission queue/uploader and platform packaging scripts. Do not merge researcher training/analysis tools into that package.
- **Web collection/distribution:** maintained in `SAmWaM42/IS-proj-web`. The current API validates the versioned submission envelope, deterministic submission ID, odd/even A/B assignment, idempotency header and HMAC participant token, then stores questionnaire JSON in Google Drive. Full per-question validation is still incomplete; Drive lookup/create is not atomic under concurrent duplicates.
- **Windows CI and participant package:** PR #29 has been merged into `deployment/participant-client`. CI passed, and the packaged Windows application completed the documented manual participant workflow on a separate Windows computer. See the 12 November milestone below and the participant branch validation record.

### Evidence language

Use distinct labels: **implemented in source**, **built**, **tests passed**, **packaged**, **clean-machine tested**, **interactive participant flow validated**, and **end-to-end collection verified**. Do not collapse them into a single “complete” status. A successful synthetic training run is not participant evidence; a successful CI package is not desktop GUI validation; an API smoke test is not a complete participant-to-Drive validation.


## Windows participant release milestone — 12 November 2026

This milestone records the completed participant-client Windows build and deployment work. It does not imply that researcher-side training or analysis tasks on Dev are complete.

- [x] Windows MSVC/vcpkg GitHub Actions workflow merged through PR #29 into `deployment/participant-client`.
- [x] Windows build, configured C++/NLP tests, standalone Python/spaCy provisioning and package creation passed in CI.
- [x] Downloaded package extracted and launched on a separate Windows computer without the project development toolchain or separately installed application dependencies.
- [x] Participant login and assigned participant-number entry verified.
- [x] Both questionnaire conditions (A and B) verified.
- [x] Session 1 and session 2 generated distinct submission IDs.
- [x] Questionnaire submission persisted to Google Drive; the stored JSON contained the latest answers entered during the test.
- [x] Windows package published as `AnimalAgent-Windows.zip` in release [v0.1.0-rc.1](https://github.com/SAmWaM42/IS-proj/releases/tag/v0.1.0-rc.1), alongside `AnimalAgent-Linux.zip`.
- [x] Participant website updated and deployed with Windows and Linux download links: [https://is-proj-web.vercel.app/](https://is-proj-web.vercel.app/).

Manual evidence is recorded in [the participant branch validation report](https://github.com/SAmWaM42/IS-proj/blob/deployment/participant-client/Docs/25-windows-participant-manual-validation.md). This validates the tested Windows machine and workflow, not every Windows configuration. The release's Linux asset is not a substitute for a separately documented clean-machine Linux validation.

### Remaining boundaries

The software validation milestone does not itself grant ethics/supervisor approval for participant recruitment. The web API validates the submission envelope, deterministic ID, condition mapping, idempotency header and participant authentication; it does not perform complete validation of every questionnaire answer against the active questionnaire schema. Google Drive lookup-before-create is not atomic under concurrent duplicate requests.
