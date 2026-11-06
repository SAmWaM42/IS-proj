# Questionnaire JSON Analysis and Visualisation

**Status:** initial researcher-side implementation on `Dev`; synthetic tests are present but must be run locally before treating the tool as verified.

## Scope and research separation

`scripts/analyze_questionnaire_submissions.py` analyses the questionnaire JSON files downloaded by `scripts/download_study_submissions.py`. It does not read or derive results from Q-tables, training rewards, memory records, affective-state traces, or simulation interaction logs.

The output is evidence about **participant-reported perceptions**. It is not a measure of training performance and must not be merged with the training/behavioural analysis dataset.

## Run

No third-party Python package is required:

```bash
python3 scripts/analyze_questionnaire_submissions.py \
  --input-dir "$HOME/AnimalAgentStudyData/submissions" \
  --out-dir "$HOME/AnimalAgentStudyData/analysis"
```

The defaults use:
- Input: `~/AnimalAgentStudyData/submissions`
- Output: `~/AnimalAgentStudyData/analysis`

The output directory must be outside the repository and must not be the input directory or a child of it. Free-text answers are not copied into analysis outputs.

## Current outputs

- `analysis_report.json`: input-quality summary, included submission count, complete paired-participant count, missing/mismatched pairs, and analysis limitations.
- `questionnaire_condition_summary.csv`: per-question descriptive statistics (n, mean, median, standard deviation, min, max) split by baseline/proposed treatment.
- `paired_question_differences.csv`: per-participant baseline and proposed values, and proposed-minus-baseline difference for each numeric question answered in both sessions.
- `paired_question_summary.csv`: descriptive summaries of the paired differences.
- `comparison_response_counts.csv`: counts for categorical answers in the final comparison questionnaire.
- `configured_subscale_scores.csv`: paired subscale scores only for subscales explicitly defined in the scoring configuration.
- `question_*.svg`: per-question baseline/proposed mean charts, using readable labels for the active shortened questionnaire.

## Questionnaire scoring configuration

Question IDs are stable machine identifiers. The script is aligned with the active shortened pilot instrument in `Components/Questionnaire/questionnaire_content.cpp` and the lowercasing performed by `questionnaire-submission.cpp`. It defaults to reverse-scoring `c3` and `c13`, and to the two multi-item construct groupings defined by the active questionnaire: behavioural consistency (`c1`, `c3`) and emotional expression (`c11`, `c13`). It also keeps the single-item adaptability (`c4`), memory (`c5`), and believability (`c18`) responses distinct, and keeps the separate overall summary rating (`summary_believability`) separate. These defaults should be rechecked if the questionnaire wording or IDs change. A private JSON config file outside the repository can override these defaults, for example:

```json
{
  "reverse_scored_question_ids": ["replace_with_approved_reverse_item_id"],
  "subscales": {
    "replace_with_approved_scale_name": [
      "replace_with_question_id_1",
      "replace_with_question_id_2"
    ]
  }
}
```

Then run:

```bash
python3 scripts/analyze_questionnaire_submissions.py \
  --input-dir "$HOME/AnimalAgentStudyData/submissions" \
  --out-dir "$HOME/AnimalAgentStudyData/analysis" \
  --scoring-config "$HOME/AnimalAgentStudyData/scoring-config.json"
```

Only add IDs and scales after confirming them against the final approved questionnaire definition and scoring key. Reverse scoring assumes a 1–5 scale and uses 6 minus the raw response.

## Data quality and pairing

The script checks the submission envelope, filename/participant/session consistency, odd/even A/B assignment, response types, duplicate question IDs, and session-specific comparison questionnaire rule. It applies the active instrument's attention check: `c21` must equal 4 (Agree). A submission with a failed or missing attention item remains visible in the quality report and comparison-choice counts, but its condition ratings are excluded from primary rating summaries and paired rating comparisons. Invalid files are reported and excluded. Multiple files for the same participant/session are flagged as duplicates and excluded rather than arbitrarily selecting one.

A paired comparison is produced only when a participant has one valid session 1 and one valid session 2 submission with matching questionnaire versions. Participants with incomplete pairs remain in the condition-level descriptive summaries but are listed as missing/mismatched in the report.

The script currently produces descriptive statistics and visualisations. It does not run inferential significance tests, estimate causal effects, or decide participant exclusion based on consent, attention checks, completeness rules, or protocol criteria. Those require the approved study protocol and should be reviewed before the final analysis.

## Verification checklist

- [ ] Run `python3 -m unittest tests/test_analyze_questionnaire_submissions.py` from the repository root.
- [ ] Run the tool against a synthetic fixture directory and inspect every output.
- [ ] Compare generated question IDs with the active C++ questionnaire definition.
- [ ] Confirm reverse-scoring and subscale membership with the approved scoring key.
- [ ] Run against the downloaded test submissions and manually inspect pairing and condition assignment.
- [ ] Confirm output directory is private and outside Git.
- [ ] Only then perform the final pipeline dry run and live retrieval/analysis walkthrough.

No real participant data should be collected or analysed for research conclusions until the questionnaire, consent, protocol, and ethics requirements are approved.
