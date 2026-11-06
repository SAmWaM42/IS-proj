#!/usr/bin/env python3
"""Analyse downloaded Animal Agent questionnaire submission JSON files.

This tool analyses participant-reported questionnaire data only. It does not
read training logs, Q-tables, interaction traces, memory records, or affect data.

Example:
  python3 scripts/analyze_questionnaire_submissions.py
  python3 scripts/analyze_questionnaire_submissions.py \
      --input-dir "$HOME/AnimalAgentStudyData/submissions" \
      --out-dir "$HOME/AnimalAgentStudyData/analysis"

Optional scoring configuration:
  {
    "reverse_scored_question_ids": ["negative_worded_item"],
    "subscales": {
      "overall_believability": ["believability_1", "believability_2"]
    }
  }

Subscale IDs must be supplied from the approved questionnaire definition.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import re
import statistics
import sys
import xml.sax.saxutils as xml_escape
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

DEFAULT_INPUT = Path.home() / "AnimalAgentStudyData" / "submissions"
DEFAULT_OUTPUT = Path.home() / "AnimalAgentStudyData" / "analysis"
FILE_RE = re.compile(r"^P(?P<participant>[0-9]{2,4})-S(?P<session>[12])\.json$")
NUMERIC_TYPES = {"likert", "integer", "number"}

# Active shortened pilot instrument, transcribed from
# Components/Questionnaire/questionnaire_content.cpp. Submission IDs are
# lowercased by questionnaire-submission.cpp before they are uploaded.
DEFAULT_REVERSE_SCORED = ["c3", "c13"]
DEFAULT_SUBSCALES = {
    "behavioural_consistency": ["c1", "c3"],
    "emotional_expression": ["c11", "c13"],
}
QUESTION_LABELS = {
    "c1": "Personality felt stable",
    "c3": "Behaviour seemed random or contradictory (reverse-scored)",
    "c4": "Behaviour changed sensibly with treatment",
    "c5": "Companion seemed to remember earlier treatment",
    "c11": "Mood was visible in behaviour",
    "c13": "Reactions seemed flat or emotionless (reverse-scored)",
    "c18": "Companion seemed like a believable animal",
    "c21": "Attention check",
    "summary_believability": "Overall believability (summary scale)",
    "session_feedback": "Optional session feedback (not analysed)",
    "cmp_consistent": "A/B comparison: consistency",
    "cmp_adapted": "A/B comparison: adaptability",
    "cmp_believable": "A/B comparison: believability",
    "cmp_confidence": "Confidence in believability comparison",
    "cmp_built_differently": "Blinding check",
    "cmp_difference_detail": "Blinding-check explanation (not analysed)",
    "cmp_other_comments": "Additional comments (not analysed)",
}


class AnalysisError(RuntimeError):
    pass


def expected_condition(participant: int, session: int) -> str:
    """Match the project's counterbalancing contract."""
    if participant % 2:
        return "A" if session == 1 else "B"
    return "B" if session == 1 else "A"


def load_scoring_config(path: Path | None) -> dict[str, Any]:
    config = {
        "reverse_scored_question_ids": list(DEFAULT_REVERSE_SCORED),
        "subscales": dict(DEFAULT_SUBSCALES),
    }
    if path is None:
        return config
    try:
        supplied = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise AnalysisError(f"Cannot read scoring config: {exc}") from None
    if not isinstance(supplied, dict):
        raise AnalysisError("Scoring config must be a JSON object.")
    reverse = supplied.get("reverse_scored_question_ids", config["reverse_scored_question_ids"])
    scales = supplied.get("subscales", config["subscales"])
    if not isinstance(reverse, list) or not all(isinstance(x, str) for x in reverse):
        raise AnalysisError("reverse_scored_question_ids must be an array of strings.")
    if not isinstance(scales, dict):
        raise AnalysisError("subscales must be an object mapping names to question ID arrays.")
    for name, ids in scales.items():
        if not isinstance(name, str) or not isinstance(ids, list) or not ids or not all(isinstance(x, str) for x in ids):
            raise AnalysisError("Each subscale must have a name and a non-empty array of question IDs.")
    config["reverse_scored_question_ids"] = supplied.get("reverse_scored_question_ids", reverse)
    config["subscales"] = supplied.get("subscales", scales)
    return config


def validate_submission(data: Any, filename: str) -> list[str]:
    errors: list[str] = []
    match = FILE_RE.fullmatch(filename)
    if not match:
        return ["filename must match Pxx-Sx.json"]
    if not isinstance(data, dict):
        return ["root must be a JSON object"]
    required = {"schema_version", "submission_id", "participant_number", "session_index",
                "condition", "questionnaire_version", "responses"}
    if set(data) != required:
        errors.append("top-level fields do not match submission schema v1")
    participant = int(match.group("participant"))
    session = int(match.group("session"))
    if data.get("schema_version") != 1 or isinstance(data.get("schema_version"), bool):
        errors.append("schema_version must be 1")
    if data.get("submission_id") != filename[:-5]:
        errors.append("submission_id does not match filename")
    if type(data.get("participant_number")) is not int or data.get("participant_number") != participant:
        errors.append("participant_number does not match filename")
    if type(data.get("session_index")) is not int or data.get("session_index") != session:
        errors.append("session_index does not match filename")
    if data.get("condition") != expected_condition(participant, session):
        errors.append("condition does not match odd/even counterbalancing contract")
    if not isinstance(data.get("questionnaire_version"), str) or not data.get("questionnaire_version"):
        errors.append("questionnaire_version must be a non-empty string")
    responses = data.get("responses")
    if not isinstance(responses, dict) or set(responses) != {"condition_questionnaire", "comparison_questionnaire"}:
        errors.append("responses must contain condition_questionnaire and comparison_questionnaire")
        return errors
    for key in ("condition_questionnaire",):
        errors.extend(validate_questionnaire(responses.get(key), key))
    comparison = responses.get("comparison_questionnaire")
    if session == 1 and comparison is not None:
        errors.append("session 1 comparison_questionnaire must be null")
    elif session == 2:
        errors.extend(validate_questionnaire(comparison, "comparison_questionnaire"))
    return errors


def validate_questionnaire(value: Any, label: str) -> list[str]:
    errors: list[str] = []
    if not isinstance(value, dict) or set(value) != {"answers"} or not isinstance(value.get("answers"), list):
        return [f"{label} must contain an answers array"]
    seen: set[str] = set()
    for i, answer in enumerate(value["answers"]):
        prefix = f"{label}.answers[{i}]"
        if not isinstance(answer, dict) or set(answer) != {"question_id", "answer_type", "value"}:
            errors.append(f"{prefix} must contain question_id, answer_type and value")
            continue
        qid, kind, answer_value = answer["question_id"], answer["answer_type"], answer["value"]
        if not isinstance(qid, str) or not re.fullmatch(r"[a-z][a-z0-9_]{0,63}", qid):
            errors.append(f"{prefix} has invalid question_id")
        elif qid in seen:
            errors.append(f"{prefix} duplicates question_id {qid}")
        else:
            seen.add(qid)
        if not isinstance(kind, str) or kind not in {"likert", "integer", "number", "single_choice", "multiple_choice", "text", "boolean"}:
            errors.append(f"{prefix} has unsupported answer_type")
            continue
        valid = True
        if kind in {"likert", "integer"}:
            valid = type(answer_value) is int
        elif kind == "number":
            valid = type(answer_value) in (int, float) and math.isfinite(answer_value)
        elif kind == "single_choice":
            valid = isinstance(answer_value, str)
        elif kind == "multiple_choice":
            valid = isinstance(answer_value, list) and all(isinstance(x, str) for x in answer_value)
        elif kind == "text":
            valid = isinstance(answer_value, str) and len(answer_value) <= 2000
        elif kind == "boolean":
            valid = isinstance(answer_value, bool)
        if not valid:
            errors.append(f"{prefix}.value does not match {kind}")
    return errors


def read_input_files(input_dir: Path) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    if not input_dir.is_dir():
        raise AnalysisError(f"Input directory does not exist: {input_dir}")
    files = sorted(p for p in input_dir.glob("P*-S*.json") if p.is_file())
    if not files:
        raise AnalysisError(f"No Pxx-Sx.json submissions found in {input_dir}")
    loaded: list[dict[str, Any]] = []
    issues: list[dict[str, Any]] = []
    seen_keys: Counter[tuple[int, int]] = Counter()
    parsed_files: list[tuple[Path, dict[str, Any], int, int]] = []
    for path in files:
        match = FILE_RE.fullmatch(path.name)
        if not match:
            issues.append({"file": path.name, "status": "ignored", "issues": ["filename does not match Pxx-Sx.json"]})
            continue
        participant, session = int(match.group("participant")), int(match.group("session"))
        seen_keys[(participant, session)] += 1
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            issues.append({"file": path.name, "status": "invalid", "issues": [f"cannot parse JSON: {exc}"]})
            continue
        errors = validate_submission(data, path.name)
        if errors:
            issues.append({"file": path.name, "status": "invalid", "issues": errors})
            continue
        parsed_files.append((path, data, participant, session))
    duplicates = {key for key, count in seen_keys.items() if count > 1}
    for path, data, participant, session in parsed_files:
        if (participant, session) in duplicates:
            issues.append({"file": path.name, "status": "excluded", "issues": ["duplicate participant/session file; resolve manually"]})
            continue
        questionnaire = data["responses"]["condition_questionnaire"]
        attention = attention_check_passed(questionnaire)
        loaded.append({
            "file": path.name, "data": data, "participant": participant,
            "session": session, "attention_check_passed": attention,
        })
        status = "included" if attention is True else "included_attention_check_failed_or_missing"
        issues.append({"file": path.name, "status": status, "issues": [] if attention is True else ["attention check failed or missing; condition ratings excluded from primary rating summaries"]})
    return loaded, issues


def answer_map(questionnaire: dict[str, Any]) -> dict[str, dict[str, Any]]:
    return {row["question_id"]: row for row in questionnaire["answers"]}


def attention_check_passed(questionnaire: dict[str, Any]) -> bool | None:
    """Return None when the active instrument's attention item is absent."""
    answer = answer_map(questionnaire).get("c21")
    if answer is None or answer.get("answer_type") != "likert":
        return None
    return answer.get("value") == 4


def numeric_answers(questionnaire: dict[str, Any], scoring: dict[str, Any]) -> dict[str, float]:
    values: dict[str, float] = {}
    reverse = set(scoring["reverse_scored_question_ids"])
    for qid, answer in answer_map(questionnaire).items():
        if answer["answer_type"] not in NUMERIC_TYPES:
            continue
        value = answer["value"]
        if type(value) not in (int, float) or not math.isfinite(value):
            continue
        if qid in reverse:
            if not 1 <= value <= 5:
                raise AnalysisError(f"Reverse-scored item '{qid}' has value outside the 1–5 scale.")
            value = 6 - value
        values[qid] = float(value)
    return values


def summarize(values: list[float]) -> dict[str, Any]:
    return {
        "n": len(values),
        "mean": statistics.mean(values) if values else None,
        "median": statistics.median(values) if values else None,
        "sd": statistics.stdev(values) if len(values) >= 2 else None,
        "min": min(values) if values else None,
        "max": max(values) if values else None,
    }


def write_csv(path: Path, rows: list[dict[str, Any]], fields: list[str]) -> None:
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def svg_count_chart(path: Path, title: str, counts: dict[str, int]) -> None:
    width, height = 760, 440
    left, right, top, bottom = 90, 30, 70, 80
    plot_w, plot_h = width - left - right, height - top - bottom
    maximum = max(counts.values(), default=1)
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
             '<rect width="100%" height="100%" fill="white"/>',
             f'<text x="{width/2}" y="34" text-anchor="middle" font-family="sans-serif" font-size="18">{xml_escape.escape(title)}</text>']
    entries = list(sorted(counts.items()))
    slot = plot_w / max(1, len(entries))
    bar_w = min(150, slot * 0.55)
    for i, (label, count) in enumerate(entries):
        bar_h = (count / maximum) * plot_h if maximum else 0
        x = left + slot * i + (slot - bar_w) / 2
        y = top + plot_h - bar_h
        parts.append(f'<rect x="{x}" y="{y}" width="{bar_w}" height="{bar_h}" fill="#557c9b"/>')
        parts.append(f'<text x="{x+bar_w/2}" y="{y-9}" text-anchor="middle" font-family="sans-serif" font-size="13">{count}</text>')
        parts.append(f'<text x="{x+bar_w/2}" y="{height-45}" text-anchor="middle" font-family="sans-serif" font-size="12">{xml_escape.escape(label)}</text>')
    parts.append('</svg>')
    path.write_text("\\n".join(parts), encoding="utf-8")


def svg_bar_chart(path: Path, title: str, labels: list[str], values: list[float | None],
                  y_min: float = 1.0, y_max: float = 5.0) -> None:
    width, height = 760, 440
    left, right, top, bottom = 90, 30, 70, 80
    plot_w, plot_h = width - left - right, height - top - bottom
    def y(value: float) -> float:
        return top + (y_max - value) / (y_max - y_min) * plot_h
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
             '<rect width="100%" height="100%" fill="white"/>',
             f'<text x="{width/2}" y="34" text-anchor="middle" font-family="sans-serif" font-size="18">{xml_escape.escape(title)}</text>']
    for tick in range(int(y_min), int(y_max) + 1):
        yy = y(float(tick))
        parts.append(f'<line x1="{left}" y1="{yy}" x2="{width-right}" y2="{yy}" stroke="#dddddd"/>')
        parts.append(f'<text x="{left-15}" y="{yy+5}" text-anchor="end" font-family="sans-serif" font-size="12">{tick}</text>')
    slot = plot_w / max(1, len(labels))
    bar_w = min(110, slot * 0.5)
    for i, (label, value) in enumerate(zip(labels, values)):
        if value is None:
            continue
        x = left + slot * i + (slot - bar_w) / 2
        yy = y(value)
        parts.append(f'<rect x="{x}" y="{yy}" width="{bar_w}" height="{bottom + top + plot_h - yy - bottom}" fill="#557c9b"/>')
        parts.append(f'<text x="{x+bar_w/2}" y="{yy-9}" text-anchor="middle" font-family="sans-serif" font-size="13">{value:.2f}</text>')
        parts.append(f'<text x="{x+bar_w/2}" y="{height-45}" text-anchor="middle" font-family="sans-serif" font-size="13">{xml_escape.escape(label)}</text>')
    parts.append('</svg>')
    path.write_text("\n".join(parts), encoding="utf-8")


def run(args: argparse.Namespace) -> int:
    input_dir = args.input_dir.expanduser().resolve()
    output_dir = args.out_dir.expanduser().resolve()
    repo_root = Path(__file__).resolve().parents[1]
    if output_dir == repo_root or repo_root in output_dir.parents:
        raise AnalysisError("Choose an output directory outside the repository to protect participant data.")
    if output_dir == input_dir or input_dir in output_dir.parents:
        raise AnalysisError("Output directory must not be the input directory or a child of it.")
    scoring = load_scoring_config(args.scoring_config)
    records, quality = read_input_files(input_dir)

    # Keep only participants with one valid submission per session and matching questionnaire versions.
    by_participant: dict[int, dict[int, dict[str, Any]]] = defaultdict(dict)
    for record in records:
        by_participant[record["participant"]][record["session"]] = record
    complete_pairs: dict[int, tuple[dict[str, Any], dict[str, Any]]] = {}
    pair_issues = []
    for participant, sessions in sorted(by_participant.items()):
        if 1 not in sessions or 2 not in sessions:
            pair_issues.append({"participant": participant, "issue": "missing_session_1_or_2"})
            continue
        first, second = sessions[1], sessions[2]
        if first["data"]["questionnaire_version"] != second["data"]["questionnaire_version"]:
            pair_issues.append({"participant": participant, "issue": "questionnaire_version_mismatch"})
            continue
        complete_pairs[participant] = (first, second)

    # Condition-level question summaries; each participant contributes one rating per treatment.
    observations: dict[tuple[str, str], list[float]] = defaultdict(list)
    comparison_counts: dict[str, Counter[str]] = defaultdict(Counter)
    comparison_numeric: dict[str, list[float]] = defaultdict(list)
    attention_passed_count = sum(record["attention_check_passed"] is True for record in records)
    for record in records:
        # Keep comparison-choice counts, but apply the instrument's explicit
        # attention-check rule to condition-rating summaries.
        baseline_code = "A" if record["participant"] % 2 else "B"
        treatment = "baseline" if record["data"]["condition"] == baseline_code else "proposed"
        if record["attention_check_passed"] is True:
            values = numeric_answers(record["data"]["responses"]["condition_questionnaire"], scoring)
            for qid, value in values.items():
                if qid != "c21":
                    observations[(treatment, qid)].append(value)
        comparison = record["data"]["responses"]["comparison_questionnaire"]
        if comparison:
            for qid, answer in answer_map(comparison).items():
                if answer["answer_type"] == "single_choice":
                    comparison_counts[qid][str(answer["value"])] += 1
                elif answer["answer_type"] in NUMERIC_TYPES and type(answer["value"]) in (int, float):
                    comparison_numeric[qid].append(float(answer["value"]))

    summary_rows = []
    question_ids = sorted({qid for _, qid in observations})
    for qid in question_ids:
        for treatment in ("baseline", "proposed"):
            stats = summarize(observations.get((treatment, qid), []))
            summary_rows.append({"question_id": qid, "condition": treatment, **stats})

    paired_rows = []
    paired_differences: dict[str, list[float]] = defaultdict(list)
    attention_eligible_pairs = 0
    for participant, (first, second) in sorted(complete_pairs.items()):
        if first["attention_check_passed"] is not True or second["attention_check_passed"] is not True:
            pair_issues.append({"participant": participant, "issue": "attention_check_failed_or_missing_in_one_or_both_sessions"})
            continue
        attention_eligible_pairs += 1
        treatment_records = {}
        for record in (first, second):
            p = record["participant"]
            condition = record["data"]["condition"]
            baseline_code = "A" if p % 2 else "B"
            treatment = "baseline" if condition == baseline_code else "proposed"
            treatment_records[treatment] = {
                qid: value for qid, value in numeric_answers(record["data"]["responses"]["condition_questionnaire"], scoring).items()
                if qid != "c21"
            }
        for qid in sorted(set(treatment_records.get("baseline", {})) & set(treatment_records.get("proposed", {}))):
            baseline, proposed = treatment_records["baseline"][qid], treatment_records["proposed"][qid]
            difference = proposed - baseline
            paired_differences[qid].append(difference)
            paired_rows.append({"participant": f"P{participant:02d}", "question_id": qid,
                                "baseline": baseline, "proposed": proposed,
                                "proposed_minus_baseline": difference})

    paired_summary = []
    for qid, diffs in sorted(paired_differences.items()):
        paired_summary.append({"question_id": qid, **summarize(diffs)})

    comparison_rows = [{"question_id": qid, "response": choice, "count": count}
                       for qid, counts in sorted(comparison_counts.items())
                       for choice, count in sorted(counts.items())]
    comparison_numeric_rows = [
        {"question_id": qid, **summarize(values)}
        for qid, values in sorted(comparison_numeric.items())
    ]

    output_dir.mkdir(parents=True, exist_ok=True)
    write_csv(output_dir / "questionnaire_condition_summary.csv", summary_rows,
              ["question_id", "condition", "n", "mean", "median", "sd", "min", "max"])
    write_csv(output_dir / "paired_question_differences.csv", paired_rows,
              ["participant", "question_id", "baseline", "proposed", "proposed_minus_baseline"])
    write_csv(output_dir / "paired_question_summary.csv", paired_summary,
              ["question_id", "n", "mean", "median", "sd", "min", "max"])
    write_csv(output_dir / "comparison_response_counts.csv", comparison_rows,
              ["question_id", "response", "count"])
    write_csv(output_dir / "comparison_numeric_summary.csv", comparison_numeric_rows,
              ["question_id", "n", "mean", "median", "sd", "min", "max"])

    charts = []
    for qid in question_ids:
        if not qid:
            continue
        base = summarize(observations.get(("baseline", qid), []))["mean"]
        proposed = summarize(observations.get(("proposed", qid), []))["mean"]
        if base is None and proposed is None:
            continue
        safe_name = re.sub(r"[^a-zA-Z0-9_-]+", "_", qid)
        filename = f"question_{safe_name}.svg"
        label = QUESTION_LABELS.get(qid, qid)
        svg_bar_chart(output_dir / filename, f"{label}: mean response by agent condition",
                      ["Baseline", "Proposed"], [base, proposed])
        charts.append(filename)
    for qid, counts in sorted(comparison_counts.items()):
        safe_name = re.sub(r"[^a-zA-Z0-9_-]+", "_", qid)
        filename = f"comparison_{safe_name}.svg"
        svg_count_chart(output_dir / filename, QUESTION_LABELS.get(qid, qid), dict(counts))
        charts.append(filename)

    subscale_rows = []
    for name, ids in scoring["subscales"].items():
        for participant, (first, second) in sorted(complete_pairs.items()):
            treatment_scores = {}
            for record in (first, second):
                answers = numeric_answers(record["data"]["responses"]["condition_questionnaire"], scoring)
                if all(qid in answers for qid in ids):
                    treatment = "baseline" if record["data"]["condition"] == ("A" if participant % 2 else "B") else "proposed"
                    treatment_scores[treatment] = statistics.mean(answers[qid] for qid in ids)
            if set(treatment_scores) == {"baseline", "proposed"}:
                subscale_rows.append({"participant": f"P{participant:02d}", "subscale": name,
                                      "baseline": treatment_scores["baseline"],
                                      "proposed": treatment_scores["proposed"],
                                      "proposed_minus_baseline": treatment_scores["proposed"] - treatment_scores["baseline"]})

    write_csv(output_dir / "configured_subscale_scores.csv", subscale_rows,
              ["participant", "subscale", "baseline", "proposed", "proposed_minus_baseline"])

    report = {
        "analysis_scope": "questionnaire submissions only",
        "input_directory": str(input_dir),
        "valid_submissions_included": len(records),
        "attention_check_passed_submissions": attention_passed_count,
        "attention_check_failed_or_missing_submissions": len(records) - attention_passed_count,
        "complete_participant_pairs": len(complete_pairs),
        "attention_eligible_complete_pairs": attention_eligible_pairs,
        "participants_missing_or_mismatched_sessions": pair_issues,
        "quality_checks": quality,
        "comparison_response_counts": comparison_rows,
        "scoring_config_used": str(args.scoring_config.resolve()) if args.scoring_config else None,
        "reverse_scored_question_ids": scoring["reverse_scored_question_ids"],
        "configured_subscales": list(scoring["subscales"].keys()),
        "active_question_labels": QUESTION_LABELS,
        "attention_check_rule": "C21 must equal 4 (Agree); failed or missing checks are excluded from condition-rating summaries and paired rating comparisons.",
        "charts": charts,
        "interpretation_note": (
            "Questionnaire results measure participant perceptions. They are not training rewards, "
            "Q-table metrics, or direct measurements of learning. No statistical significance tests "
            "or causal claims are generated by this descriptive script."
        ),
        "privacy_note": "Free-text answers are not copied into analysis outputs.",
    }
    (output_dir / "analysis_report.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps({
        "valid_submissions_included": len(records),
        "complete_participant_pairs": len(complete_pairs),
        "question_ids_summarised": len(question_ids),
        "charts_created": len(charts),
        "output_directory": str(output_dir),
    }, indent=2))
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-dir", type=Path, default=DEFAULT_INPUT)
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--scoring-config", type=Path,
                        help="optional JSON defining reverse-scored question IDs and subscales")
    return parser


def main() -> int:
    try:
        return run(build_parser().parse_args())
    except AnalysisError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
