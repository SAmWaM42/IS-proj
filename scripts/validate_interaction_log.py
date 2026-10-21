#!/usr/bin/env python3
"""Validate Animal Agent interactive-simulation JSONL logs.

Usage:
    python3 scripts/validate_interaction_log.py artifacts/simulation/interaction-events.jsonl

Schema 1 contains completed action interactions. Schema 2 currently contains
no-action preference statements and unclassified utterances.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path


COMMON_REQUIRED = {
    "schema_version",
    "run_id",
    "interaction_id",
    "sequence_index",
    "condition_code",
    "input_summary",
}

ACTION_REQUIRED = {"observation", "baseline", "proposed"}
PROPOSED_REQUIRED = {
    "memory_count_before",
    "memory_count_after",
    "affect_before",
    "affect_after",
}

NO_ACTION_STATUS = {
    ("preference_statement", "recorded_no_action"),
    ("preference_statement", "statement_object_unresolved"),
    ("unclassified", "unclassified_no_action"),
}


def validate_action_record(row: dict, line_number: int, errors: list[str]) -> None:
    missing = ACTION_REQUIRED - row.keys()
    if missing:
        errors.append(
            f"line {line_number}: schema 1 action record missing fields: "
            f"{', '.join(sorted(missing))}"
        )
        return

    if not isinstance(row["observation"], dict):
        errors.append(f"line {line_number}: observation must be an object")

    for agent in ("baseline", "proposed"):
        details = row[agent]
        if not isinstance(details, dict) or not isinstance(details.get("action"), str):
            errors.append(f"line {line_number}: {agent}.action is missing or is not a string")

    proposed = row["proposed"]
    if isinstance(proposed, dict):
        missing_proposed = PROPOSED_REQUIRED - proposed.keys()
        for key in sorted(missing_proposed):
            errors.append(f"line {line_number}: proposed.{key} is missing")
        for key in ("memory_count_before", "memory_count_after"):
            if key in proposed and type(proposed[key]) is not int:
                errors.append(f"line {line_number}: proposed.{key} must be an integer")
        for key in ("affect_before", "affect_after"):
            if key in proposed and not isinstance(proposed[key], dict):
                errors.append(f"line {line_number}: proposed.{key} must be an object")


def validate_no_action_record(row: dict, line_number: int, errors: list[str]) -> None:
    status = row.get("processing_status")
    analysis = row.get("utterance_analysis")
    if not isinstance(analysis, dict):
        errors.append(f"line {line_number}: schema 2 no-action record requires utterance_analysis object")
        return

    kind = analysis.get("kind")
    if (kind, status) not in NO_ACTION_STATUS:
        errors.append(
            f"line {line_number}: unsupported schema 2 no-action event "
            f"(kind={kind!r}, processing_status={status!r})"
        )
        return

    if analysis.get("response_required") is not False:
        errors.append(f"line {line_number}: no-action event must set response_required to false")
    if analysis.get("response_action") != "NONE":
        errors.append(f"line {line_number}: no-action event must set response_action to 'NONE'")

    if kind == "preference_statement":
        if status == "recorded_no_action":
            if analysis.get("memory_updated") is not True:
                errors.append(f"line {line_number}: recorded preference must set memory_updated to true")
            if analysis.get("relation") not in {"likes", "dislikes"}:
                errors.append(f"line {line_number}: recorded preference relation must be likes or dislikes")
            if not isinstance(analysis.get("object"), str) or not analysis["object"].strip():
                errors.append(f"line {line_number}: recorded preference object must be a non-empty string")
        elif analysis.get("memory_updated") is not False:
            errors.append(f"line {line_number}: unresolved preference must set memory_updated to false")
    elif analysis.get("memory_updated") is not False:
        errors.append(f"line {line_number}: unclassified utterance must set memory_updated to false")


def validate(path: Path) -> int:
    errors: list[str] = []
    seen: set[int] = set()
    sequence: list[int] = []
    rows = 0

    try:
        stream = path.open("r", encoding="utf-8")
    except OSError as exc:
        print(f"ERROR: cannot open {path}: {exc}")
        return 2

    with stream:
        for line_number, line in enumerate(stream, 1):
            if not line.strip():
                errors.append(f"line {line_number}: blank lines are not valid JSONL records")
                continue
            try:
                row = json.loads(line)
            except json.JSONDecodeError as exc:
                errors.append(f"line {line_number}: invalid JSON: {exc}")
                continue
            rows += 1
            if not isinstance(row, dict):
                errors.append(f"line {line_number}: each JSONL record must be an object")
                continue

            missing = COMMON_REQUIRED - row.keys()
            if missing:
                errors.append(f"line {line_number}: missing common fields: {', '.join(sorted(missing))}")
                continue

            if type(row["schema_version"]) is not int:
                errors.append(f"line {line_number}: schema_version must be an integer")
                continue
            if not isinstance(row["run_id"], str) or not row["run_id"].strip():
                errors.append(f"line {line_number}: run_id must be a non-empty string")
            if not isinstance(row["condition_code"], str) or not row["condition_code"].strip():
                errors.append(f"line {line_number}: condition_code must be a non-empty string")
            if not isinstance(row["input_summary"], str):
                errors.append(f"line {line_number}: input_summary must be a string")

            interaction_id = row["interaction_id"]
            sequence_index = row["sequence_index"]
            if type(interaction_id) is not int or type(sequence_index) is not int:
                errors.append(f"line {line_number}: interaction_id and sequence_index must be integers")
                continue
            if interaction_id in seen:
                errors.append(f"line {line_number}: duplicate interaction_id {interaction_id}")
            seen.add(interaction_id)
            sequence.append(sequence_index)

            schema_version = row["schema_version"]
            if schema_version == 1:
                validate_action_record(row, line_number, errors)
            elif schema_version == 2:
                validate_no_action_record(row, line_number, errors)
            else:
                errors.append(f"line {line_number}: unsupported schema_version {schema_version!r}")

    if sequence and sequence != sorted(sequence):
        errors.append("sequence_index values are not in processing order")
    if sequence and sequence != list(range(min(sequence), min(sequence) + len(sequence))):
        errors.append("sequence_index values contain gaps")
    if rows == 0:
        errors.append("log contains no valid records")

    if errors:
        print(f"FAIL: {len(errors)} issue(s) in {path}")
        for error in errors:
            print(f"- {error}")
        return 1

    print(f"PASS: {rows} interaction record(s) validated in {path}")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(__doc__.strip())
        raise SystemExit(2)
    raise SystemExit(validate(Path(sys.argv[1])))
