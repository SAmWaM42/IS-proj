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


def behaviour_metrics(baseline_condition: dict[str, Any], proposed_condition: dict[str, Any]) -> dict[str, Any]:
    """Return honest diagnostics; do not equate action agreement with believability."""
    def action_rows(condition: dict[str, Any]) -> list[dict[str, Any]]:
        rows = condition.get("trials", [])
        return [row for row in rows if isinstance(row, dict)]

    baseline_rows = action_rows(baseline_condition)
    proposed_rows = action_rows(proposed_condition)
    baseline_by_scenario = {row.get("scenario_id"): row.get("action_internal")
                            for row in baseline_rows if row.get("scenario_id")}
    proposed_by_scenario = {row.get("scenario_id"): row.get("action_internal")
                            for row in proposed_rows if row.get("scenario_id")}
    shared_ids = sorted(set(baseline_by_scenario) & set(proposed_by_scenario))
    agreement = (
        sum(baseline_by_scenario[key] == proposed_by_scenario[key] for key in shared_ids) / len(shared_ids)
        if shared_ids else None
    )

    def repeated_consistency(rows: list[dict[str, Any]]) -> dict[str, Any]:
        grouped: dict[str, list[str]] = {}
        for row in rows:
            sid, action = row.get("scenario_id"), row.get("action_internal")
            if sid and isinstance(action, str):
                grouped.setdefault(sid, []).append(action)
        repeats = [actions for actions in grouped.values() if len(actions) > 1]
        if not repeats:
            return {"score": None, "repeated_scenario_groups": 0,
                    "status": "not_estimable_no_repeated_scenario_ids"}
        total = sum(len(actions) for actions in repeats)
        modal = sum(max(actions.count(action) for action in set(actions)) for actions in repeats)
        return {"score": modal / total, "repeated_scenario_groups": len(repeats),
                "status": "descriptive_repeated_scenario_action_agreement"}

    # A causal/directional adaptation metric needs an explicitly matched event/control pair.
    all_rows = baseline_rows + proposed_rows
    pair_groups: dict[str, dict[str, dict[str, Any]]] = {}
    for row in all_rows:
        pair_id, variant = row.get("adaptation_pair_id"), row.get("adaptation_variant")
        if pair_id and variant in {"event", "control"}:
            pair_groups.setdefault(str(pair_id), {})[variant] = row
    valid_pairs = [pair for pair in pair_groups.values()
                   if "event" in pair and "control" in pair]
    if valid_pairs:
        adaptation_change_rate = sum(
            pair["event"].get("action_internal") != pair["control"].get("action_internal")
            for pair in valid_pairs
        ) / len(valid_pairs)
        adaptation = {"status": "matched_event_control_pairs_available",
                      "matched_pairs": len(valid_pairs),
                      "action_change_rate": adaptation_change_rate}
    else:
        adaptation = {"status": "not_estimable_missing_matched_event_control_pairs",
                      "matched_pairs": 0, "action_change_rate": None}

    return {
        "cross_condition_action_agreement": agreement,
        "matched_scenarios_compared": len(shared_ids),
        "baseline_repeated_scenario_consistency": repeated_consistency(baseline_rows),
        "proposed_repeated_scenario_consistency": repeated_consistency(proposed_rows),
        "adaptation": adaptation,
        "interpretation_note": "Cross-condition action agreement is not a believability score; adaptation requires matched event/control scenarios."
    }


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
        if data.get("protocol_id") == "believability-v1":
            event_path = path.with_suffix(".jsonl")
            if not event_path.is_file():
                exclusions.append({"file": str(path), "reason": "missing_jsonl_event_export"})
                continue
            try:
                event_rows = [json.loads(line) for line in event_path.read_text(encoding="utf-8").splitlines() if line.strip()]
            except (OSError, json.JSONDecodeError) as exc:
                exclusions.append({"file": str(path), "reason": f"invalid_jsonl_event_export:{exc}"})
                continue
            expected_event_count = sum(
                len(condition.get("trials", []))
                for condition in data.get("conditions", [])
                if isinstance(condition, dict)
            )
            event_error = None
            if len(event_rows) != expected_event_count:
                event_error = "jsonl_event_count_mismatch"
            else:
                for event in event_rows:
                    if event.get("run_id") != data.get("session_id") or event.get("schema_version") != 1:
                        event_error = "jsonl_event_run_or_schema_mismatch"
                        break
                    if not event.get("scenario_id") or not isinstance(event.get("action_internal"), str):
                        event_error = "jsonl_event_missing_scenario_or_action"
                        break
            if event_error:
                exclusions.append({"file": str(path), "reason": event_error})
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
        metrics = behaviour_metrics(baseline[0], proposed[0])
        sessions.append({
            "session_id": str(data["session_id"]),
            "baseline_score": baseline[1],
            "proposed_score": proposed[1],
            "difference_proposed_minus_baseline": proposed[1] - baseline[1],
            "cross_condition_action_agreement": metrics["cross_condition_action_agreement"],
            "baseline_repeat_consistency": metrics["baseline_repeated_scenario_consistency"]["score"],
            "proposed_repeat_consistency": metrics["proposed_repeated_scenario_consistency"]["score"],
            "adaptation_status": metrics["adaptation"]["status"],
            "behaviour_metrics": metrics,
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
            "difference_proposed_minus_baseline", "cross_condition_action_agreement",
            "baseline_repeat_consistency", "proposed_repeat_consistency", "adaptation_status",
            "condition_order", "scenario_version", "questionnaire_version", "software_revision"
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
        "mean_cross_condition_action_agreement": statistics.mean(
            [r["cross_condition_action_agreement"] for r in sessions
             if r["cross_condition_action_agreement"] is not None]
        ) if any(r["cross_condition_action_agreement"] is not None for r in sessions) else None,
        "behaviour_metric_note": "Repeated-scenario consistency and adaptation are marked not estimable when repeated scenario IDs or matched event/control pairs are absent.",
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
