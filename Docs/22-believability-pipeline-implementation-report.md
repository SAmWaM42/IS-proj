# Believability Pipeline Implementation Report

**Branch:** `test/believability-evaluation-modelling`  
**Status:** Implementation attempt committed; build/runtime verification pending.  
**Purpose:** Record what was implemented, what remains partial, and how to demonstrate the pipeline locally.

## Summary

The participant-study path has been updated to use JSON fixtures, the five-item questionnaire, active Q-learning updates, session-isolated policy copies, and machine-readable exports. Python validation and analysis utilities plus CTest registrations were added.

This is not yet evidence of a successful end-to-end run: the code has not been compiled or executed in this environment. Treat it as a demonstrable implementation candidate until local build and pilot checks pass.

## Implementation status

| Pipeline stage | Status | Evidence / limitation |
|---|---|---|
| 1. Audit current logging and study flow | **Partially complete** | Existing study UI, policy persistence, paired-agent tests, and interaction-log validator were inspected. The new study path is distinct from the existing simulation JSONL logger. |
| 2. Versioned scenario definitions | **Implemented in source** | `Data/Study/scenarios.json` contains nine synthetic scenarios. `study.cpp` loads the fixture rather than hardcoding the scenarios. Fixture validation is registered with CTest. |
| 3. Automated technical checks | **Partial** | Fixture validation and unit tests for score calculation, condition mapping, repeated-scenario metrics, and cautious adaptation reporting were added. Existing C++ paired-agent regression tests remain available. Dedicated automated BEL-03 memory persistence, BEL-05 affect response, BEL-06 geometry, and BEL-07 restart tests have not all been wired to this study pipeline. |
| 4. Event validation and export | **Implemented in source; runtime unverified** | Completed sessions write a JSON session summary and a JSONL trial-event file. The event records include condition, scenario ID/tags, action, participant feedback/reward, learning-update status, and proposed-agent memory/affect snapshots. |
| 5. Behaviour metrics | **Partial and deliberately conservative** | Analysis calculates paired believability scores and cross-condition action agreement. Repeated-scenario consistency is only calculated when scenario IDs repeat. Adaptation is marked not estimable unless matched event/control pairs exist; the current nine-scenario fixture does not provide those pairs. |
| 6. Participant-rating capture | **Implemented in source; runtime unverified** | The first screen requests consent; declining exits without saving responses. The end-of-condition survey loads five JSON-defined items, and a primary mean score is produced only when all five ratings are valid. No names/contact details are requested. |
| 7. Pilot | **Not run** | Requires a local build, trained policy files, an interactive display, and a human pilot. Do not collect formal study data until the UI and protocol have been checked. |

## Key implementation decisions

- The study randomly assigns the baseline/proposed conditions to labels A/B.
- The baseline and proposed agents load separate trained Q-table files. Their state encodings may differ, so this implementation does not claim that their initial Q-values are identical.
- Only the Q-table files are copied into a unique session policy directory. Training-time memory and affect sidecars are not imported into the participant session.
- Each live Q-table updates independently from per-action participant feedback. Exploration is set to zero during the study, so updates are enabled but random exploratory actions are not deliberately introduced.
- The end-of-condition five-item questionnaire is evaluation data, not a learning reward. The UI now discloses that per-action feedback updates learning.
- The session JSON and JSONL exports contain a generated session code but do not request direct identifiers. The code currently includes a timestamp component; it is not derived from participant identity.
- Proposed-agent state snapshots are written only to that session's policy directory. They are not shared with the baseline or another session.

## Run locally

From the repository root on a machine with the required SFML 3 development dependencies and Python:

```bash
python3 scripts/validate_study_fixtures.py
python3 tests/test_believability_pipeline.py

cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

If trained policies are not present, train them first using the project's supported training command. For example, review the CLI options with `./build/IS-2 --help`, then run a bounded training job that writes `baseline.qtable` and `proposed.qtable` to the selected policy directory.

Launch the study only after checking the build and confirming those policy files exist:

```bash
./build/IS-2 --study --policy-dir artifacts/training --study-out artifacts/study
```

After one or more completed, consented sessions, generate a descriptive analysis export:

```bash
python3 scripts/analyze_believability_study.py artifacts/study --out artifacts/study-analysis
```

The analysis command writes `paired_scores.csv` and `summary.json`. It excludes incomplete, unconsented, malformed, or unpaired sessions and records exclusion reasons. It is a descriptive pilot summary, not a statistical-significance or causal analysis.

## Required verification before calling this complete

1. Compile the updated C++ study code and resolve any build errors.
2. Run CTest and the two Python commands above; retain their actual output.
3. Complete one consented test session and one declined-consent test; verify no response data are saved for the declined session.
4. Inspect the session JSON, JSONL events, and session-specific policy files. Confirm no data are written into the shared training policy files and that memory/affect sidecars are scoped to the session.
5. Confirm `policy_updates_enabled` is true and each observed per-action response has an accurate `policy_update_applied` value. A “no response” entry must not trigger a reward update.
6. Run the analysis on the completed test session and confirm the condition mapping is correct even when proposed is assigned label A.
7. Add matched event/control and repeated-context scenarios before interpreting adaptability or repeated-context consistency.
8. Review the consent text, storage/retention, and department/supervisor requirements before recruiting participants for the actual study.

## Known scientific limitations

- The five items form a project-specific draft measure; they have not been validated as a psychometric scale.
- The study uses scripted observations and participant feedback, not a naturalistic long-term animal interaction.
- Active learning means actions later in a condition can depend on earlier participant feedback. The update count and condition order should be considered when interpreting ratings.
- Because baseline and proposed Q-tables are currently loaded from their own trained files, differences cannot be attributed exclusively to memory/affect without accounting for their training history and state-encoding differences.
- Cross-condition action agreement is not the same as believability. Behavioural consistency and adaptation need appropriately matched scenario designs.

