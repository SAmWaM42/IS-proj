#!/usr/bin/env python3
"""Validate completed anonymous study sessions and summarise paired believability ratings.

Usage:
  python3 scripts/analyze_believability_study.py artifacts/study --out artifacts/study-analysis
"""
from __future__ import annotations

import argparse
import csv
import json
import statistics
from pathlib import Path
from typing import Any

ITEM_KEYS = ("plausibility", "coherence", "responsiveness", "consistency", "continuity")


def score_condition(condition: dict[str, Any]) -> tuple[float | None, list[str]]:
    errors: list[str] = []
    answers = condition.get("questionnaire")
    if not isinstance(answers, dict):
        return None, ["questionnaire_missing"]
    values: list[int] = []
    for key in ITEM_KEYS:
        value = answers.get(key)
        if type(value) is not int or value < 1 or value > 5:
            errors.append(f"invalid_or_missing_rating:{key}")
        else:
            values.append(value)
    if errors:
        return None, errors
    return sum(values) / len(values), []


def read_sessions(inputs: list[Path]) -> tuple[list[dict[str, Any]], list[dict[str, str]]]:
    sessions: list[dict[str, Any]] = []
    exclusions: list[dict[str, str]] = []
    files: list[Path] = []
    for source in inputs:
        if source.is_dir():
            files.extend(source.rglob("*.json"))
        elif source.is_file():
            files.append(source)
    seen: set[Path] = set()
    for path in sorted(files):
        if path in seen:
            continue
        seen.add(path)
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            exclusions.append({"file": str(path), "reason": f"unreadable_json:{exc}"})
            continue
        # Ignore sidecars and unrelated JSON artifacts in the output directory.
        if "session_id" not in data or "conditions" not in data:
            continue
        if data.get("consent") is not True:
            exclusions.append({"file": str(path), "reason": "consent_not_confirmed"})
            continue
        if data.get("completed") is not True:
            exclusions.append({"file": str(path), "reason": "session_incomplete"})
            continue
        conditions = data.get("conditions")
        mapping = data.get("condition_key", {})
        if not isinstance(conditions, list) or len(conditions) != 2:
            exclusions.append({"file": str(path), "reason": "expected_two_conditions"})
            continue
        by_kind: dict[str, tuple[dict[str, Any], float]] = {}
        bad = False
        for condition in conditions:
            code = condition.get("condition_code")
            kind = mapping.get(code)
            if kind not in {"baseline", "proposed"} or kind in by_kind:
                bad = True
                break
            score, issues = score_condition(condition)
            if score is None:
                exclusions.append({"file": str(path), "reason": ";".join(issues)})
                bad = True
                break
            by_kind[kind] = (condition, score)
        if bad or set(by_kind) != {"baseline", "proposed"}:
            if not bad:
                exclusions.append({"file": str(path), "reason": "condition_mapping_incomplete"})
            continue
        baseline, proposed = by_kind["baseline"], by_kind["proposed"]
        sessions.append({
            "session_id": str(data["session_id"]),
            "baseline_score": baseline[1],
            "proposed_score": proposed[1],
            "difference_proposed_minus_baseline": proposed[1] - baseline[1],
            "baseline_items": baseline[0]["questionnaire"],
            "proposed_items": proposed[0]["questionnaire"],
            "condition_order": data.get("condition_order", []),
            "scenario_version": data.get("scenario_version"),
            "questionnaire_version": data.get("questionnaire_version"),
            "software_revision": data.get("software_revision", "unknown"),
        })
    return sessions, exclusions


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inputs", nargs="+", type=Path, help="session JSON file(s) or directory/directories")
    parser.add_argument("--out", type=Path, required=True, help="output directory")
    args = parser.parse_args()
    sessions, exclusions = read_sessions(args.inputs)
    args.out.mkdir(parents=True, exist_ok=True)

    with (args.out / "paired_scores.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=[
            "session_id", "baseline_score", "proposed_score",
            "difference_proposed_minus_baseline", "condition_order",
            "scenario_version", "questionnaire_version", "software_revision"
        ])
        writer.writeheader()
        for row in sessions:
            clean = {key: row[key] for key in writer.fieldnames}
            clean["condition_order"] = json.dumps(clean["condition_order"])
            writer.writerow(clean)

    differences = [row["difference_proposed_minus_baseline"] for row in sessions]
    summary = {
        "schema_version": 1,
        "valid_paired_sessions": len(sessions),
        "excluded_records": len(exclusions),
        "mean_baseline_believability": statistics.mean(r["baseline_score"] for r in sessions) if sessions else None,
        "mean_proposed_believability": statistics.mean(r["proposed_score"] for r in sessions) if sessions else None,
        "mean_paired_difference_proposed_minus_baseline": statistics.mean(differences) if differences else None,
        "median_paired_difference_proposed_minus_baseline": statistics.median(differences) if differences else None,
        "interpretation": "Descriptive pilot summary only; does not establish causality or statistical significance.",
        "exclusions": exclusions,
    }
    (args.out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: summary[key] for key in (
        "valid_paired_sessions", "excluded_records", "mean_baseline_believability",
        "mean_proposed_believability", "mean_paired_difference_proposed_minus_baseline"
    )}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
