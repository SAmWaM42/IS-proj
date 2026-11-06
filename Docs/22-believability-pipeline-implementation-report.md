# Believability Pipeline Implementation Report

**Target branch:** `test/believability-evaluation-modelling`  
**Authority:** `Docs/Believability Testing Pipeline Specification.md` and `Docs/Google Form Template Companion Believability Study.md`  
**Status:** Specification-aligned implementation committed; compilation and interactive verification are still required.

## Summary

The previous `--study` path launched a separate synthetic scenario/questionnaire exercise with five custom ratings. That did not match the intended experiment. The `--study` command now launches the existing animated `CompanionUI` in a guided-study mode and follows the 12-step participant script from the specification.

The new analysis utility consumes the Google Forms CSV export and application logs. The previous five-item analysis remains as a legacy test; it is not the intended analysis for this study.

## Implemented in source

| Specification requirement | Implementation |
|---|---|
| R1 — Guided script | Displays the 12 prompts, accepts only the expected control or statement at each guided step, and advances after the interaction is executed. Step 9 is a timed 30-second observation. |
| R2 — Session phases | Records consent, warm-up, guided, free, rating, and finished phase events in per-run JSONL logs and updates the manifest. |
| R3 — Traceable logs | Records participant code, session number, condition label and true condition, phase, step ID, software revision, timestamps, and policy checksums. Interaction and step logs are separate JSONL files. |
| R4 — Free interaction | Offers an optional free-interaction period with a Finish button and a three-minute maximum, followed by the form prompt. |
| R5 — Google Form linking | Opens the form using Google Forms `entry.<id>` parameters for session, condition code, and run ID. |
| R6 — Condition switching | The second companion cannot be loaded through the study navigation until the Answer Form button has been used for the current companion. |
| R7 — Counterbalancing and memory isolation | Persists a participant counter outside the repository and alternates the A/B mapping strictly by counter parity. Proposed memory and affect are reset in memory for each condition. |
| R8 — Fixed throws | Both guided ball probes use the same fixed landing position. |
| R9 — Autonomous actions | Autonomous decisions are suppressed during ordinary guided steps and enabled for the 30-second observation and free interaction. |
| R11 — Read-only policies | Requires trained baseline/proposed Q-tables, disables learning and exploration, records policy checksums, and does not save participant state over the shared training policy files. |
| R12 — Blinding | Participant mode hides internal action labels, memory, affect, statistics, and the A/B-to-model mapping. |
| Analysis | Added a Google Forms CSV + JSONL analysis script for reverse-scored subscales, attention-check exclusions, paired comparisons, Wilcoxon tests when SciPy is installed, effect sizes, bootstrap intervals, Holm adjustment for secondary subscales, reliability estimates, direct comparisons, order summaries, and descriptive behavioural metrics. |

## Partial or not implemented

- **R7:** Each consented `--study` launch commits the next participant code and counterbalance assignment; declining consent does not advance the persistent counter. There is no separate researcher UI for resetting the counter; use a fresh output directory for dry runs/pilots if you do not want them to consume the production counter.
- **R10:** Actual study logs and analysis report action distributions and unfamiliar-Q-table-state counts. The separate repeated scripted replay check before participant sessions remains unimplemented.
- **R13:** The application writes a manifest, step log, interaction log, and session summary outside the repository by default, but does not package them into a single archive bundle.
- **Compliance rate:** Deliberately remains unestimated until the plausibility rule for justified deviations is specified.
- **Behavioural interpretation:** Memory-probe shifts, action variety, latency, idle actions, and free-interaction use are descriptive supporting metrics. They do not replace participant ratings as the evidence for believability.
- **Consent wording:** The application has a pre-interaction consent gate. The researcher must still provide the full approved information sheet beforehand; the Google Form also retains its consent question in the first submission path. Confirm this duplicated consent flow with the supervisor before formal recruitment.

## Data handling and condition isolation

The default study output path is `../companion-study-data`, a sibling directory outside the repository. Use a restricted local directory for real participant data and keep the Google Forms export there as well. Do not commit participant CSVs, logs, or session files.

The study resets the proposed agent's in-memory memory/affect before each condition and does not delete or overwrite the shared training sidecar files. The baseline does not receive preference-memory writes. The two conditions are run sequentially under the same participant session, with independent condition state. Learning and exploration remain disabled during this participant study, in line with the newer specification. Free-text content is not written verbatim to the study logs: the logs use fixed scripted labels or a generic free-text marker instead. The agent may still process the text transiently to choose a response; this is not a substitute for advising participants not to enter identifying information.

## Configure the Google Form

Obtain the form's **Get pre-filled link** URL and entry IDs from Google Forms. Set:

```bash
export ANIMAL_AGENT_STUDY_FORM_URL='PASTE_BASE_PREFILLED_FORM_URL_HERE'
export ANIMAL_AGENT_FORM_ENTRY_SESSION='SESSION_ENTRY_ID'
export ANIMAL_AGENT_FORM_ENTRY_CONDITION='CONDITION_ENTRY_ID'
export ANIMAL_AGENT_FORM_ENTRY_RUN='RUN_ID_ENTRY_ID'
```

The base URL should end before the three `entry.<id>` parameters. The application appends the correct session (1 or 2), companion code (A or B), and run ID. Test the link with a dummy form submission before any participant session.

## Build and test locally

From the repository root:

```bash
python3 scripts/validate_study_fixtures.py
python3 tests/test_believability_pipeline.py
python3 tests/test_companion_believability_analysis.py

cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

Let the C++ build finish; an interrupted build is not a pass or a compiler failure. These changes have not been compiled or run in this environment, so the first local build is still a required verification step.

## Run the guided study

Ensure the trained files `artifacts/training/baseline.qtable` and `artifacts/training/proposed.qtable` exist. Launch:

```bash
./build/IS-2 --study \
  --policy-dir artifacts/training \
  --study-out ../companion-study-data
```

Expected participant flow:

1. Read the full study information sheet from the researcher and accept/decline the in-app gate.
2. Warm up using WASD/arrow keys, then press Enter.
3. Complete the 12 guided steps for Companion A.
4. Optionally interact freely, then click **Finish Free Phase**.
5. Click **Answer Form**, submit the first form path, return to the application, then click **Continue to Companion B**.
6. Complete the same script for Companion B, optionally interact freely, and submit the second form path.
7. Return to the application and click **Finish Study**.

The form link must be configured or the study cannot advance past the rating checkpoint. The policy files are required; the application now fails early if either trained Q-table is missing.

## Analyse collected data

Export Google Forms responses to a CSV file in the restricted study-data directory, for example `../companion-study-data/form-responses.csv`. Then run:

```bash
python3 scripts/analyze_companion_believability_study.py \
  --form-csv ../companion-study-data/form-responses.csv \
  --logs-dir ../companion-study-data \
  --out-dir ../companion-study-analysis
```

Outputs include valid companion ratings, paired participant comparisons, exclusions, and `summary.json`. The analysis requires the session/run/condition fields to join each form submission to its manifest and requires all 12 guided-step logs plus the matching interaction log. Attention-check failures and incomplete or mismatched records are excluded with a reason.

Install SciPy in the analysis environment to calculate Wilcoxon p-values. The script reports a clear status if SciPy is unavailable. Results are exploratory; the five-item legacy believability score is no longer the primary analysis. The current form's primary hypothesis tests follow the specification: Adaptability (H1) and Overall believability (H2), with the other subscales treated as secondary.

## Remaining readiness checks

1. Compile the branch and fix any compiler errors.
2. Run all CTest and Python checks and retain the actual output.
3. Verify form prefill fields arrive correctly for both sessions and link to the correct manifest.
4. Complete one consented dry run and one declined-consent run; verify no participant response logs are saved for the declined run.
5. Check that the shared Q-table and sidecar files have unchanged checksums before and after a study session.
6. Confirm step IDs 1–12, the fixed throw positions, idle-action logs, phase durations, and response-latency values.
7. Confirm both condition mappings occur in strict alternating order over consecutive participant launches.
8. Implement R10 and package the R13 bundle before claiming all requirements are complete.
9. Resolve the outstanding plausibility rule, and obtain supervisor/ethics approval for the final consent, form, and procedure before formal data collection.
