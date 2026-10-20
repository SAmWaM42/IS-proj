#!/usr/bin/env python3
"""Validate Animal Agent interactive-simulation JSONL logs.

Usage:
    python3 scripts/validate_interaction_log.py artifacts/simulation/interaction-events.jsonl
"""
from __future__ import annotations

import json
import sys
from pathlib import Path


REQUIRED = {
    "schema_version",
    "run_id",
    "interaction_id",
    "sequence_index",
    "condition_code",
    "input_summary",
    "observation",
    "baseline",
    "proposed",
}


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
            missing = REQUIRED - row.keys()
            if missing:
                errors.append(f"line {line_number}: missing fields: {', '.join(sorted(missing))}")
                continue
            interaction_id = row["interaction_id"]
            sequence_index = row["sequence_index"]
            if not isinstance(interaction_id, int) or not isinstance(sequence_index, int):
                errors.append(f"line {line_number}: interaction_id and sequence_index must be integers")
                continue
            if interaction_id in seen:
                errors.append(f"line {line_number}: duplicate interaction_id {interaction_id}")
            seen.add(interaction_id)
            sequence.append(sequence_index)
            if row["schema_version"] != 1:
                errors.append(f"line {line_number}: unsupported schema_version {row['schema_version']!r}")
            if not isinstance(row["observation"], dict):
                errors.append(f"line {line_number}: observation must be an object")
            for agent in ("baseline", "proposed"):
                details = row[agent]
                if not isinstance(details, dict) or not isinstance(details.get("action"), str):
                    errors.append(f"line {line_number}: {agent}.action is missing")
            proposed = row["proposed"]
            if isinstance(proposed, dict):
                for key in ("memory_count_before", "memory_count_after", "affect_before", "affect_after"):
                    if key not in proposed:
                        errors.append(f"line {line_number}: proposed.{key} is missing")

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
