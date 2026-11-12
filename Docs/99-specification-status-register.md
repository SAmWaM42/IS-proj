# Specification and README Status Register

**Review date:** 12 November 2026  
**Authority:** This register classifies the documentation currently present on `Dev`. It preserves the original design history; it does not rewrite historical requirements as though they were always the current implementation.  
**Status rule:** “Implemented” means source implementation is identified in the current project record; it does not automatically mean a fresh build, tests, clean-machine run, or participant validation passed.

## How to read this register

- **Implemented** — the described capability or contract is present in the current implementation record. Any verification caveat is stated.
- **Superseded** — a later implementation or specification now owns the operational truth. Retain this document for design history.
- **Experimental** — a branch, hypothesis, training mode, or investigation that is not the selected production/participant default.
- **Proposed** — design or study requirements remain unimplemented, unresolved, or awaiting approval.
- **Mixed** — the document contains both implemented work and open requirements; the note identifies the boundary.

The branch responsibilities are:
- `Dev`: researcher-side agent, training, evaluation, diagnostics, retrieval and analysis.
- `deployment/participant-client`: participant desktop runtime, study flow, questionnaires, local queue/uploader and platform packaging.
- `IS-proj-web`: web API, participant authentication, Drive storage and release-download redirects.
- `main`: project-wide architecture, pipeline, build and toolchain overview.

## Current README references

| Document | Scope / authority |
|---|---|
| [Main README](../README.md) | Cross-project architecture, pipeline, build, packaging and release discipline. |
| [Dev README](../README.md) on `Dev` | Researcher-side implementation. |
| [Participant README](https://github.com/SAmWaM42/IS-proj/blob/deployment/participant-client/README.md) | Participant executable and runtime. |
| [Deployment README](https://github.com/SAmWaM42/IS-proj/blob/deployment/participant-client/Deployment/README.md) | Participant packaging scripts and prerequisites. |
| [Web README](https://github.com/SAmWaM42/IS-proj-web/blob/main/README.md) | API, authentication, storage and release redirects. |
| [Project context](project-context.md) | Architectural hand-off; read with this status register and the implementation tracker. |
| [Implementation status and roadmap](07-implementation-status-and-roadmap.md) | Researcher-side implementation and evidence tracker. |

## Numbered specification dispositions

| Document | Disposition | Current interpretation |
|---|---|---|
| `02-shared-types-and-state.md` | **Superseded** | Initial prototype contract. Later action, environment, memory/affect and participant specifications define the evolved state and observation model. Keep for the original design rationale. |
| `03-baseline-learning-loop.md` | **Superseded** | Initial seven-action baseline learning-loop plan. Current baseline is defined by the shared current action/environment contracts and researcher implementation. |
| `04-baseline-learning-tests.md` | **Superseded** | Initial deterministic baseline test plan. Retain as test-design history; current CMake/CTest suites and later regression specifications are the operational test references. |
| `05-memory-affect-scaffolding.md` | **Implemented** | Contract/scaffolding stage. Later specifications and implementation records define the expanded semantic memory and affect behaviour. |
| `06-memory-affect-state-integration.md` | **Implemented** | Bounded memory/affect state integration design has been carried forward. Do not treat this document as the complete current runtime architecture. |
| `08-live-memory-retrieval-and-ranking.md` | **Implemented** | Bounded retrieval/ranking and memory-bias integration are recorded as implemented. Fresh build and regression evidence must still be checked before claiming current verification. |
| `08-research-validity-memory-affect-and-logging-spec.md` | **Proposed** | Broader research-validity, logging and paired-evaluation requirements. Some component work exists, but the complete requirements and evidence are not established by this document. |
| `09-persistence-and-rest-aggregation.md` | **Mixed** | The file labels persistence as implemented/validated but its final section still describes implementation as a future stage. Treat the status as internally inconsistent; verify current persistence code and tests before relying on the “validated” label. |
| `09-research-validity-implementation-progress.md` | **Mixed** | Records implementation of memory/affect sidecars, run manifests and interaction logs, but explicitly requires compilation, runtime, identity-isolation and further validation. It is a progress record, not sign-off. |
| `10-runtime-state-lifecycle.md` | **Superseded** | Draft pre-simulation lifecycle proposal. The later backyard environment and integrated runtime specifications supersede its “not yet environment” premise. |
| `11-live-memory-activation-and-ranking.md` | **Superseded** | Draft runtime memory-stage proposal that overlaps the earlier retrieval implementation and later integrated runtime. Retain as staged design history. |
| `12-runtime-affect-and-rest-state-orchestration.md` | **Proposed** | The full autonomous affect-decay/rest aggregation design remains a separate proposal/experiment; do not assume the entire stage is in the participant runtime. |
| `13-runtime-persistence-and-continuity-contract.md` | **Proposed** | Draft continuity contract. It is not evidence that every listed persistence domain or recovery behaviour is implemented. |
| `14-pre-simulation-environment-contract.md` | **Superseded** | The project has moved beyond the pre-simulation stage; see the backyard environment and current runtime specifications. |
| `15-backyard-environment-model.md` | **Implemented** | Deterministic backyard world-state model and regression scenarios are described as implemented. The same document records important limitations: no full physics, spatial collision, time-based energy depletion or complete action effects. |
| `15-proposed-agent-comparison-spec.md` | **Superseded** | Early proposed-agent comparison design. Current paired training/evaluation and participant protocol specifications take precedence. |
| `16-baseline-agent-comparison-spec.md` | **Superseded** | Early baseline comparison design. Retain as the original control-condition rationale; use current shared contracts and evaluation implementation for operational behaviour. |
| `16-paired-agent-scenario-tests.md` | **Implemented** | Describes deterministic paired regression scenarios. These are engineering tests, not statistical evidence or a believability study. Confirm current test registration and execution on the target branch. |
| `17-believability-testing-pipeline.md` | **Mixed** | Study and measurement requirements remain relevant, while the implementation and submission route have evolved. The participant branch and web README define current technical boundaries; ethics, questionnaire validation and end-to-end verification remain open. |
| `17-stage-a1-training-diagnostics.md` | **Experimental** | Researcher-side diagnostic investigation. Its reported metrics are observations to reproduce, not independently verified facts or a mandate to change policy. |
| `17-training-validation-implementation.md` | **Mixed** | Headless training, validation and comparison capabilities are recorded as implemented; the document itself distinguishes synthetic reward from believability and requires fresh build/test and experiment verification. |
| `18-sfml-companion-ui.md` | **Superseded** | Earlier top-down SFML UI replacement plan. The participant client now owns the operational UI and guided study path. Retain its asset/integration checklist as historical context only. |
| `18-stage-a1-training-refactor-experiments.md` | **Experimental** | A reversible experiment menu, not approval to apply all candidate refactors. Preserve baseline checkpoints and require diagnostics before behavioural changes. |
| `19-synthetic-user-training-experiment.md` | **Experimental** | Approved as a development experiment, but randomized scripted scenarios are the active training default recorded in current project status. Do not describe synthetic-user training as the default participant or training path. |
| `19-training-and-evaluation-redesign-spec.md` | **Mixed** | Current training/evaluation principles remain useful; the exact active runner and schedule have evolved. The status tracker and current scripts are authoritative for what is actually selected and available. |
| `20-stage-minus-one-action-architecture.md` | **Implemented** | Shared action vocabulary and validity-mask architecture are recorded in source. Its own verification checklist requires a fresh build, self-test and action-coverage checks. |
| `20-training-visualisation-spec.md` | **Implemented** | Per-decision diagnostics and offline dashboard generator are recorded in source. Dashboard execution against current artifacts and output validation remain required. |
| `20-wp1-training-instrumentation.md` | **Implemented** | Diagnostics/provenance instrumentation is described, with no intended behavioural change. Run the specified build/tests and inspect generated manifests/logs before calling it verified. |
| `21-affect-decay-autonomous-update-test-plan.md` | **Experimental** | Explicitly belongs to the separate `test/affect-decay-autonomous-updates` branch. Not automatically part of Dev or participant deployment. |
| `21-fetch-object-preference-spec.md` | **Implemented** | Preference-to-fetch behaviour is present in Dev and selectively ported to the participant client. Preference biases object selection; it is not an arbitrary action command. See the participant parity note for the bounded implementation and verification caveats. |
| `21-participant-study-milestones.md` | **Mixed** | Live participant implementation tracker. Its 5 November review date is stale; current participant branch source, deployment README and the latest technical status take precedence. Update milestone rows only with current evidence. |
| `22-believability-pipeline-implementation-report.md` | **Superseded** | Describes an earlier test-branch guided-study/Google Forms implementation. The current participant branch's in-app questionnaire and web submission pipeline supersede it as the operational implementation. Preserve it as migration history. |
| `22-local-study-data-retrieval.md` | **Mixed** | Researcher-side Drive retrieval utility is described as implemented/in progress; the document explicitly says its JSON envelope is not yet adapted to the older Google Forms CSV + interaction-log analysis input. Do not assume full retrieval-to-analysis integration. |
| `22-nlp-intent-classification-spec.md` | **Implemented** | Deterministic intent categories and the Python/spaCy process boundary are implemented on the researcher and participant sides. This is not general semantic understanding; explicit preference handling remains bounded/rule-based. |
| `23-participant-client-deployment-spec.md` | **Mixed** | On Dev, this is a historical deployment specification. The canonical current runtime specification is on `deployment/participant-client`; questionnaire integration is present in source but full build, interactive, queue-recovery and end-to-end checks remain. |
| `23-questionnaire-json-analysis.md` | **Mixed** | Researcher-side questionnaire JSON analysis is implemented in source with synthetic tests described. Run tests and inspect outputs; keep participant-reported measures separate from training rewards and behavioural diagnostics. |

## Unnumbered study documents

| Document | Disposition | Current interpretation |
|---|---|---|
| `Believability Testing Pipeline Specification.md` | **Proposed** | Version 0.1 draft for supervisor review. Ethics route, sample size, free-interaction timing and plausibility rules remain open. |
| `Google Form Template Companion Believability Study.md` | **Proposed** | Draft form template. Supervisor/ethics approval, final question wording and preview/dry-run checks remain required. |
| `project-context.md` | **Implemented / living hand-off** | Architectural source of context. Its latest dated cross-repository hand-off section takes precedence over older status text earlier in the file. |

## Windows CI and release update — 12 November 2026

The Windows workflow and packaging changes from PR #29 have been merged into `deployment/participant-client`. The MSVC/vcpkg workflow passed its build, configured C++/NLP tests, standalone Python/spaCy provisioning and package creation. The resulting package was manually extracted and run on a separate Windows computer without the project development toolchain or separately installed application dependencies.

Manual validation confirmed participant login, both A/B questionnaire conditions, distinct session 1/session 2 submission IDs, and Google Drive persistence of the latest submitted answers. See [the participant Windows validation record](https://github.com/SAmWaM42/IS-proj/blob/deployment/participant-client/Docs/25-windows-participant-manual-validation.md).

The published [release candidate v0.1.0-rc.1](https://github.com/SAmWaM42/IS-proj/releases/tag/v0.1.0-rc.1) contains both `AnimalAgent-Linux.zip` and `AnimalAgent-Windows.zip`. The website frontend is deployed at [is-proj-web.vercel.app](https://is-proj-web.vercel.app/) and provides platform-specific release download redirects.

The evidence validates the tested Windows environment and participant workflow; it does not establish universal Windows compatibility. The Linux asset's presence does not independently document a new clean-machine Linux test in this cycle.

## Documentation maintenance rule

When implementation changes:
1. Update the specification that owns that component, not every document that mentions it.
2. Update the current implementation tracker with commit/test evidence.
3. Update the branch README only when the branch's operational scope or instructions change.
4. Preserve historical proposals and experiment reports, but change their disposition here when superseded.
5. Never change a status to “verified” based only on source presence or a successful compile. Record test, package, clean-machine, interactive and end-to-end evidence separately.
