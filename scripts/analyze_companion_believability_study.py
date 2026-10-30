#!/usr/bin/env python3
"""Analyse Google Forms exports and guided companion-study logs.

Inputs must be stored outside the repository when they contain real participant data.
Example:
  python3 scripts/analyze_companion_believability_study.py \
      --form-csv /secure/study/form-responses.csv \
      --logs-dir /secure/study/application-logs \
      --out-dir /secure/study/analysis
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import random
import re
import statistics
from collections import Counter
from datetime import datetime
from pathlib import Path
from typing import Any

ITEMS = [
    "The companion's personality felt stable throughout the session.",
    "The companion reacted in similar ways to similar things I did.",
    "The companion's behaviour seemed random or contradictory.",
    "The companion's behaviour changed in sensible ways depending on how I treated it.",
    "The companion seemed to remember how I had treated it earlier.",
    "The companion responded the same way no matter what had happened before.",
    "When I told the companion something about my preferences, its later behaviour seemed to reflect it.",
    "The companion did things on its own, not only in response to my buttons.",
    "The companion's behaviour was varied and did not repeat the same responses.",
    "The companion felt scripted, like a machine running through commands.",
    "I could tell what kind of mood the companion was in from how it behaved.",
    "The companion's reactions suited the situation.",
    "The companion's reactions seemed flat or emotionless.",
    "When the companion did not do what I asked, its behaviour still made sense.",
    "I wanted to keep interacting with the companion.",
    "I kept paying attention to what the companion was doing.",
    "I became bored during the session.",
    "The companion seemed like a believable animal.",
    "I could imagine this companion having its own personality.",
    "Overall, the companion behaved in a lifelike way.",
    "To show you are reading carefully, please select Agree for this row.",
]
REVERSE_ROWS = {3, 6, 10, 13, 17}
SUBSCALES = {
    "behavioural_consistency": [1, 2, 3],
    "adaptability": [4, 5, 6],
    "autonomy_variation": [8, 9, 10],
    "emotional_expression": [11, 12, 13],
    "engagement": [15, 16, 17],
    "overall_believability": [18, 19, 20],
}
SUMMARY_ITEM = "Overall, how believable was this companion as an animal?"
SESSION_COL = "Session (filled in for you)"
CONDITION_COL = "Companion you just used (filled in for you)"
RUN_COL = "Run ID (filled in for you)"
PARTICIPANT_COL = "Participant code (given by your researcher, for example P07)"
CONSENT_COL = "I am 18 or older, I have read the information above, and I agree to take part."
ISSUE_COL = "Did anything not work properly during this session (for example a freeze or a delay)?"
COMPARISON_QUESTIONS = {
    "comparison_consistency": "Which companion behaved more consistently?",
    "comparison_adaptability": "Which companion adapted better to how you treated it?",
    "comparison_engagement": "Which companion was more engaging to interact with?",
    "comparison_believability": "Which companion was more believable as an animal overall?",
    "comparison_confidence": "How confident are you in your answer to 4.4?",
    "blinding_check": "Did you think the two companions were built differently?",
}


def norm(value: str) -> str:
    return re.sub(r"\s+", " ", value.strip().casefold())


def find_indices(headers: list[str], query: str) -> list[int]:
    q = norm(query)
    # Google Sheets/CSV exports sometimes suffix duplicate form questions with .1, .2, etc.
    exact_or_duplicate = [
        i for i, header in enumerate(headers)
        if norm(header) == q or re.match(r"^" + re.escape(q) + r"\.\d+$", norm(header))
    ]
    if exact_or_duplicate:
        return exact_or_duplicate
    return [i for i, header in enumerate(headers) if q in norm(header)]


class FormTable:
    """Preserve duplicate Google Forms grid headers so session 1/2 remain distinct."""
    def __init__(self, path: Path):
        with path.open(newline="", encoding="utf-8-sig") as handle:
            rows = list(csv.reader(handle))
        if not rows:
            raise ValueError(f"Form CSV is empty: {path}")
        self.headers = rows[0]
        self.rows = rows[1:]

    def cell(self, row: list[str], query: str, session: int = 1, required: bool = False) -> str:
        indexes = find_indices(self.headers, query)
        if not indexes:
            if required:
                raise ValueError(f"Required form column not found: {query}")
            return ""
        # Grid items and per-companion questions occur twice. Pick the first set
        # for session 1 and the last set for session 2.
        index = indexes[0] if session == 1 else indexes[-1]
        return row[index].strip() if index < len(row) else ""


def parse_rating(value: str) -> int | None:
    if not value:
        return None
    labels = {
        "strongly disagree": 1, "disagree": 2,
        "neutral": 3, "neither agree nor disagree": 3,
        "agree": 4, "strongly agree": 5,
    }
    key = norm(value)
    if key in labels:
        return labels[key]
    try:
        n = int(float(value))
        return n if 1 <= n <= 5 else None
    except ValueError:
        leading = re.match(r"^\\s*([1-5])(?:\\s|[-–:.]|$)", value)
        return int(leading.group(1)) if leading else None


def read_jsonl(path: Path) -> tuple[list[dict[str, Any]], str | None]:
    if not path.is_file():
        return [], "missing_log_file"
    rows = []
    try:
        for line_no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if not line.strip():
                continue
            value = json.loads(line)
            if isinstance(value, dict):
                rows.append(value)
    except (OSError, json.JSONDecodeError) as exc:
        return [], f"invalid_jsonl:{exc}"
    return rows, None


def load_manifests(log_dir: Path) -> dict[str, dict[str, Any]]:
    result: dict[str, dict[str, Any]] = {}
    for path in log_dir.glob("*-manifest.json"):
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
            run_id = data.get("run_id")
            if run_id:
                result[str(run_id)] = data
        except (OSError, json.JSONDecodeError):
            continue
    return result


def score_form_row(table: FormTable, row: list[str], session: int) -> tuple[dict[str, Any], list[str]]:
    errors: list[str] = []
    item_values: dict[int, int | None] = {}
    for i, question in enumerate(ITEMS, 1):
        item_values[i] = parse_rating(table.cell(row, question, session=session))
    for i in range(1, 22):
        if item_values[i] is None:
            errors.append(f"missing_or_invalid_item_{i}")
    attention = item_values[21]
    if attention != 4:
        errors.append("attention_check_failed")

    scored_items: dict[int, int | None] = {}
    for i, value in item_values.items():
        scored_items[i] = (6 - value) if value is not None and i in REVERSE_ROWS else value
    subscales: dict[str, float | None] = {}
    for name, item_numbers in SUBSCALES.items():
        values = [scored_items[n] for n in item_numbers]
        subscales[name] = sum(values) / len(values) if all(v is not None for v in values) else None

    summary = parse_rating(table.cell(row, SUMMARY_ITEM, session=session))
    result: dict[str, Any] = {
        "item_responses_raw": {str(i): item_values[i] for i in item_values},
        "item_responses_scored": {str(i): scored_items[i] for i in scored_items},
        "subscales": subscales,
        "summary_believability_item": summary,
        "attention_check_passed": attention == 4,
        "technical_issue_reported": norm(table.cell(row, ISSUE_COL, session=session)) == "yes",
    }
    for key, question in COMPARISON_QUESTIONS.items():
        if session == 2:
            result[key] = table.cell(row, question, session=session)
    return result, errors


def parse_timestamp(value: Any) -> datetime | None:
    if not isinstance(value, str) or not value:
        return None
    try:
        return datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError:
        return None


def action_entropy(actions: list[str]) -> float | None:
    actions = [a for a in actions if a and a not in {"NOT_RUN", "IDLE", "NO_DIRECT_ACTION"}]
    if not actions:
        return None
    counts = Counter(actions)
    total = len(actions)
    return -sum((n / total) * math.log2(n / total) for n in counts.values())


def behaviour_metrics(run_id: str, log_dir: Path) -> tuple[dict[str, Any], list[str]]:
    steps, step_error = read_jsonl(log_dir / f"{run_id}-steps.jsonl")
    events, event_error = read_jsonl(log_dir / f"interaction-events-{run_id}.jsonl")
    errors = [e for e in (step_error, event_error) if e]
    step_rows = [r for r in steps if isinstance(r.get("step_id"), int) and 1 <= r["step_id"] <= 12]
    seen = {r["step_id"] for r in step_rows}
    missing_steps = sorted(set(range(1, 13)) - seen)
    if missing_steps:
        errors.append("guided_steps_missing:" + ",".join(map(str, missing_steps)))

    by_step: dict[int, dict[str, Any]] = {}
    for r in step_rows:
        by_step.setdefault(r["step_id"], r)
    actions = [str(by_step[i].get("selected_action", "")) for i in sorted(by_step)]
    valid_actions = [a for a in actions if a and a not in {"NOT_RUN", "IDLE", "NO_DIRECT_ACTION"}]
    probes = {}
    for left, right, name in [(3, 7, "ball_preference_probe"), (5, 11, "noise_dislike_probe")]:
        a = by_step.get(left, {}).get("selected_action")
        b = by_step.get(right, {}).get("selected_action")
        probes[name] = {
            "before_action": a, "after_action": b,
            "action_changed": (a != b) if a and b else None,
            "status": "observed" if a and b else "not_estimable_missing_step_action",
        }

    idle_actions = [r.get("selected_action") for r in events
                    if r.get("event_type") == "autonomous_action" and r.get("step_id") == 9]
    free_interactions = [r for r in events if r.get("phase") == "free" and r.get("event_type") != "autonomous_action"]
    phase_events = [r for r in steps if r.get("event_type") in {"phase_started", "phase_ended"}]
    free_start = next((parse_timestamp(r.get("timestamp")) for r in phase_events
                       if r.get("event_type") == "phase_started" and r.get("phase") == "free"), None)
    free_end = next((parse_timestamp(r.get("timestamp")) for r in phase_events
                     if r.get("event_type") == "phase_ended" and r.get("phase") == "free"), None)
    free_seconds = (free_end - free_start).total_seconds() if free_start and free_end else None
    latency_values = [
        float(r["response_latency_ms"]) for r in events
        if isinstance(r.get("response_latency_ms"), (int, float))
        and r.get("phase") == "guided" and 1 <= int(r.get("step_id", 0) or 0) <= 12
    ]

    metrics = {
        "guided_steps_observed": len(seen),
        "guided_steps_missing": missing_steps,
        "action_variety": len(set(valid_actions)) if valid_actions else None,
        "action_entropy_bits": action_entropy(valid_actions),
        "memory_probe_shift": probes,
        "idle_action_count_step_9": len(idle_actions),
        "idle_actions_step_9": idle_actions,
        "free_interaction_count": len(free_interactions),
        "free_interaction_seconds": free_seconds,
        "response_latency_ms_mean": statistics.mean(latency_values) if latency_values else None,
        "response_latency_ms_median": statistics.median(latency_values) if latency_values else None,
        "compliance_rate": None,
        "compliance_rate_note": "Not calculated: the specification's plausibility rule for justified deviations is still unresolved.",
        "response_latency_note": "Measured from interaction enqueue to the start of the selected action; it excludes animation completion.",
    }
    return metrics, errors


def cronbach_alpha(rows: list[dict[str, Any]], item_numbers: list[int]) -> float | None:
    complete = []
    for row in rows:
        values = [row["rating"]["item_responses_scored"].get(str(i)) for i in item_numbers]
        if all(type(v) is int for v in values):
            complete.append(values)
    n = len(complete)
    k = len(item_numbers)
    if n < 2 or k < 2:
        return None
    variances = []
    for j in range(k):
        col = [r[j] for r in complete]
        variances.append(statistics.variance(col))
    total_scores = [sum(r) for r in complete]
    total_variance = statistics.variance(total_scores)
    if total_variance == 0:
        return None
    return (k / (k - 1)) * (1 - sum(variances) / total_variance)


def bootstrap_median_ci(values: list[float], seed: int = 20261030, iterations: int = 10000) -> list[float] | None:
    if not values:
        return None
    if len(values) == 1:
        return [values[0], values[0]]
    rng = random.Random(seed)
    samples = []
    for _ in range(iterations):
        sample = [values[rng.randrange(len(values))] for _ in values]
        samples.append(statistics.median(sample))
    samples.sort()
    return [samples[int(0.025 * (iterations - 1))], samples[int(0.975 * (iterations - 1))]]


def wilcoxon_summary(pairs: list[tuple[float, float]]) -> dict[str, Any]:
    differences = [proposed - baseline for baseline, proposed in pairs]
    result: dict[str, Any] = {
        "n_pairs": len(pairs),
        "median_difference_proposed_minus_baseline": statistics.median(differences) if differences else None,
        "mean_difference_proposed_minus_baseline": statistics.mean(differences) if differences else None,
        "median_difference_bootstrap_95pct_ci": bootstrap_median_ci(differences),
        "wilcoxon_p_value": None,
        "matched_pairs_rank_biserial": None,
        "test_status": "not_run",
    }
    if len(pairs) < 2:
        result["test_status"] = "insufficient_paired_data"
        return result
    try:
        from scipy.stats import rankdata, wilcoxon
        nonzero = [d for d in differences if d != 0]
        if not nonzero:
            result["wilcoxon_p_value"] = 1.0
            result["matched_pairs_rank_biserial"] = 0.0
        else:
            result["wilcoxon_p_value"] = float(wilcoxon(differences, zero_method="wilcox",
                                                         alternative="two-sided", method="auto").pvalue)
            ranks = rankdata([abs(d) for d in nonzero])
            positive = sum(rank for rank, d in zip(ranks, nonzero) if d > 0)
            negative = sum(rank for rank, d in zip(ranks, nonzero) if d < 0)
            result["matched_pairs_rank_biserial"] = (positive - negative) / (positive + negative)
        result["test_status"] = "completed"
    except ImportError:
        result["test_status"] = "scipy_unavailable_install_scipy_to_compute_wilcoxon"
    return result


def holm_adjust(p_values: dict[str, float | None]) -> dict[str, float | None]:
    valid = sorted(((name, p) for name, p in p_values.items() if p is not None), key=lambda x: x[1])
    m = len(valid)
    adjusted: dict[str, float | None] = {name: None for name in p_values}
    running = 0.0
    for rank, (name, p) in enumerate(valid):
        value = min(1.0, (m - rank) * p)
        running = max(running, value)
        adjusted[name] = running
    return adjusted


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--form-csv", required=True, type=Path, help="Google Forms CSV export")
    parser.add_argument("--logs-dir", required=True, type=Path, help="directory containing manifests and JSONL logs")
    parser.add_argument("--out-dir", required=True, type=Path, help="analysis output directory (outside the repository for real data)")
    args = parser.parse_args()
    table = FormTable(args.form_csv)
    manifests = load_manifests(args.logs_dir)
    accepted: list[dict[str, Any]] = []
    exclusions: list[dict[str, Any]] = []
    seen_submissions: set[tuple[str, int, str]] = set()

    for index, form_row in enumerate(table.rows, start=2):
        try:
            session_text = table.cell(form_row, SESSION_COL, required=True)
            session = int(session_text)
            if session not in {1, 2}:
                raise ValueError(f"invalid_session_number:{session_text}")
            participant = table.cell(form_row, PARTICIPANT_COL, session=session, required=True)
            condition_code = table.cell(form_row, CONDITION_COL, session=session, required=True).upper()
            run_id = table.cell(form_row, RUN_COL, session=session, required=True)
            if not participant or not run_id or condition_code not in {"A", "B"}:
                raise ValueError("missing_participant_run_or_condition")
            key = (participant, session, run_id)
            if key in seen_submissions:
                exclusions.append({"csv_row": index, "participant_code": participant, "run_id": run_id, "reason": "duplicate_submission_key"})
                continue
            seen_submissions.add(key)
            if session == 1:
                consent = norm(table.cell(form_row, CONSENT_COL, session=session))
                if consent not in {"yes, i agree", "yes"}:
                    raise ValueError("consent_not_confirmed")
            manifest = manifests.get(run_id)
            if not manifest:
                raise ValueError("matching_application_manifest_missing")
            if manifest.get("condition_code") != condition_code:
                raise ValueError("form_condition_code_does_not_match_manifest")
            if str(manifest.get("participant_code", "")) != participant:
                raise ValueError("participant_code_does_not_match_manifest")
            if manifest.get("condition") not in {"baseline", "proposed"}:
                raise ValueError("manifest_condition_mapping_invalid")
            rating, rating_errors = score_form_row(table, form_row, session)
            if rating_errors:
                raise ValueError(";".join(rating_errors))
            behaviour, behaviour_errors = behaviour_metrics(run_id, args.logs_dir)
            if behaviour_errors:
                raise ValueError(";".join(behaviour_errors))
            background = {}
            if session == 1:
                for key, question in {
                    "age_range": "What is your age range?",
                    "pet_ownership": "Have you owned or regularly cared for a pet?",
                    "virtual_pet_familiarity": "How familiar are you with virtual pets or pet-style games?",
                    "computer_comfort": "How comfortable are you using a computer with a mouse and keyboard?",
                }.items():
                    background[key] = table.cell(form_row, question, session=session)
            accepted.append({
                "csv_row": index, "participant_code": participant, "session_index": session,
                "condition_code": condition_code, "condition": manifest["condition"], "run_id": run_id,
                "rating": rating, "behaviour": behaviour, "background": background,
                "software_revision": manifest.get("software_revision", "unknown"),
                "policy_checksums": manifest.get("policy_checksums", {}),
                "technical_issue_reported": rating["technical_issue_reported"],
                "timestamp": table.cell(form_row, "Timestamp", session=session),
            })
        except (ValueError, IndexError) as exc:
            exclusions.append({
                "csv_row": index,
                "participant_code": table.cell(form_row, PARTICIPANT_COL) if form_row else "",
                "run_id": table.cell(form_row, RUN_COL) if form_row else "",
                "reason": str(exc),
            })

    # Pair only participants with exactly one valid form for each session and condition.
    by_participant: dict[str, list[dict[str, Any]]] = {}
    for row in accepted:
        by_participant.setdefault(row["participant_code"], []).append(row)
    paired: list[dict[str, Any]] = []
    for participant, rows in sorted(by_participant.items()):
        session_indexes = [row["session_index"] for row in rows]
        conditions_present = {row["condition"] for row in rows}
        if len(rows) != 2 or set(session_indexes) != {1, 2} or conditions_present != {"baseline", "proposed"}:
            exclusions.append({"participant_code": participant, "reason": "no_unique_complete_baseline_proposed_session_pair"})
            continue
        session1 = next(row for row in rows if row["session_index"] == 1)
        session2 = next(row for row in rows if row["session_index"] == 2)
        baseline = next(row for row in rows if row["condition"] == "baseline")
        proposed = next(row for row in rows if row["condition"] == "proposed")
        paired.append({
            "participant_code": participant,
            "baseline_run_id": baseline["run_id"], "proposed_run_id": proposed["run_id"],
            "condition_order": [session1["condition_code"], session2["condition_code"]],
            "session1_condition": session1["condition"], "session2_condition": session2["condition"],
            "baseline_rating": baseline["rating"], "proposed_rating": proposed["rating"],
            "baseline_behaviour": baseline["behaviour"], "proposed_behaviour": proposed["behaviour"],
        })

    args.out_dir.mkdir(parents=True, exist_ok=True)
    with (args.out_dir / "valid_companion_ratings.jsonl").open("w", encoding="utf-8") as out:
        for row in accepted:
            out.write(json.dumps(row, ensure_ascii=False) + "\n")
    with (args.out_dir / "paired_participant_comparison.jsonl").open("w", encoding="utf-8") as out:
        for row in paired:
            out.write(json.dumps(row, ensure_ascii=False) + "\n")
    (args.out_dir / "exclusions.json").write_text(json.dumps(exclusions, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    primary: dict[str, Any] = {}
    for measure in ("adaptability", "overall_believability"):
        pairs = [(p["baseline_rating"]["subscales"][measure], p["proposed_rating"]["subscales"][measure])
                 for p in paired]
        primary[measure] = wilcoxon_summary(pairs)
    secondary_names = ("behavioural_consistency", "autonomy_variation", "emotional_expression", "engagement")
    secondary = {}
    secondary_p: dict[str, float | None] = {}
    for measure in secondary_names:
        pairs = [(p["baseline_rating"]["subscales"][measure], p["proposed_rating"]["subscales"][measure])
                 for p in paired]
        result = wilcoxon_summary(pairs)
        secondary[measure] = result
        secondary_p[measure] = result["wilcoxon_p_value"]
    adjusted = holm_adjust(secondary_p)
    for measure in secondary:
        secondary[measure]["holm_adjusted_p_value"] = adjusted[measure]

    reliability = {}
    for condition in ("baseline", "proposed"):
        condition_rows = [row for row in accepted if row["condition"] == condition]
        reliability[condition] = {
            name: cronbach_alpha(condition_rows, items)
            for name, items in SUBSCALES.items()
        }

    direct_comparison = {}
    session2 = [row for row in accepted if row["session_index"] == 2]
    for key in COMPARISON_QUESTIONS:
        direct_comparison[key] = dict(Counter(row["rating"].get(key, "") for row in session2))

    order_effects = {}
    for session_index in (1, 2):
        rows = [row for row in accepted if row["session_index"] == session_index]
        order_effects[f"session_{session_index}"] = {
            "valid_ratings": len(rows),
            "mean_adaptability": statistics.mean(
                row["rating"]["subscales"]["adaptability"] for row in rows
                if row["rating"]["subscales"]["adaptability"] is not None
            ) if any(row["rating"]["subscales"]["adaptability"] is not None for row in rows) else None,
            "mean_overall_believability": statistics.mean(
                row["rating"]["subscales"]["overall_believability"] for row in rows
                if row["rating"]["subscales"]["overall_believability"] is not None
            ) if any(row["rating"]["subscales"]["overall_believability"] is not None for row in rows) else None,
            "condition_counts": dict(Counter(row["condition"] for row in rows)),
        }
    background_summary = {
        "pet_ownership_counts": dict(Counter(
            row["background"].get("pet_ownership", "") for row in accepted if row["background"].get("pet_ownership")
        )),
        "virtual_pet_familiarity_counts": dict(Counter(
            row["background"].get("virtual_pet_familiarity", "") for row in accepted if row["background"].get("virtual_pet_familiarity")
        )),
        "age_range_counts": dict(Counter(
            row["background"].get("age_range", "") for row in accepted if row["background"].get("age_range")
        )),
    }

    summary = {
        "protocol_id": "companion-believability-v0.1",
        "valid_companion_ratings": len(accepted),
        "valid_paired_participants": len(paired),
        "excluded_rows_or_participants": len(exclusions),
        "primary_hypotheses": {
            "H1_adaptability": primary["adaptability"],
            "H2_overall_believability": primary["overall_believability"],
        },
        "secondary_subscales_holm_adjusted": secondary,
        "cronbach_alpha_by_condition": reliability,
        "direct_comparison_counts": direct_comparison,
        "order_effect_descriptives": order_effects,
        "background_covariate_descriptives": background_summary,
        "behaviour_metrics_note": "Action variety, entropy, probe shifts, idle counts and free-interaction use are descriptive; they do not measure believability. Compliance and latency remain unestimated until their operational rules/events are implemented.",
        "interpretation": "Exploratory within-participant analysis. Report effect sizes, intervals, missingness and limitations; do not infer causality from a small pilot.",
        "exclusions": exclusions,
    }
    (args.out_dir / "summary.json").write_text(json.dumps(summary, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps({
        "valid_companion_ratings": len(accepted),
        "valid_paired_participants": len(paired),
        "excluded_rows_or_participants": len(exclusions),
        "H1_adaptability": primary["adaptability"],
        "H2_overall_believability": primary["overall_believability"],
    }, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
