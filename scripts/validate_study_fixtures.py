#!/usr/bin/env python3
"""Validate versioned JSON fixtures used by the believability study."""
from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def validate() -> list[str]:
    errors: list[str] = []
    scenario_path = ROOT / "Data" / "Study" / "scenarios.json"
    questionnaire_path = ROOT / "Data" / "Study" / "questionnaire.json"

    try:
        scenarios = json.loads(scenario_path.read_text(encoding="utf-8"))
        if scenarios.get("schema_version") != 1:
            errors.append("scenarios.json: schema_version must be 1")
        rows = scenarios.get("scenarios")
        if not isinstance(rows, list) or not rows:
            errors.append("scenarios.json: scenarios must be a non-empty array")
        else:
            ids = [row.get("id") for row in rows]
            if any(not isinstance(value, str) or not value for value in ids):
                errors.append("scenarios.json: every scenario requires a non-empty id")
            if len(ids) != len(set(ids)):
                errors.append("scenarios.json: scenario ids must be unique")
            required = {"goal_state", "target_proximity", "user_status",
                        "task_status", "signal_strength"}
            for index, row in enumerate(rows):
                obs = row.get("observation")
                if not isinstance(obs, dict):
                    errors.append(f"scenarios.json: scenario {index} observation must be an object")
                    continue
                missing = required - obs.keys()
                if missing:
                    errors.append(f"scenarios.json: scenario {index} missing {sorted(missing)}")
                if "signal_strength" in obs and not 0 <= obs["signal_strength"] <= 1:
                    errors.append(f"scenarios.json: scenario {index} signal_strength must be 0..1")
                if "energy" in obs and not 0 <= obs["energy"] <= 1:
                    errors.append(f"scenarios.json: scenario {index} energy must be 0..1")
    except (OSError, json.JSONDecodeError, AttributeError) as exc:
        errors.append(f"scenarios.json: unable to parse fixture: {exc}")

    try:
        questionnaire = json.loads(questionnaire_path.read_text(encoding="utf-8"))
        if questionnaire.get("schema_version") != 1:
            errors.append("questionnaire.json: schema_version must be 1")
        items = questionnaire.get("items")
        if not isinstance(items, list) or len(items) != 5:
            errors.append("questionnaire.json: exactly five items are required")
        else:
            keys = [item.get("key") for item in items]
            if any(not isinstance(key, str) or not key for key in keys):
                errors.append("questionnaire.json: every item requires a key")
            if len(keys) != len(set(keys)):
                errors.append("questionnaire.json: item keys must be unique")
            if any(not isinstance(item.get("question"), str) or not item["question"].strip()
                   for item in items):
                errors.append("questionnaire.json: every item requires question text")
        scale = questionnaire.get("scale", {})
        if scale.get("min") != 1 or scale.get("max") != 5:
            errors.append("questionnaire.json: expected 1-5 rating scale")
    except (OSError, json.JSONDecodeError, AttributeError) as exc:
        errors.append(f"questionnaire.json: unable to parse fixture: {exc}")

    return errors


if __name__ == "__main__":
    issues = validate()
    if issues:
        print("\n".join(f"ERROR: {issue}" for issue in issues), file=sys.stderr)
        raise SystemExit(1)
    print("Believability study fixtures valid.")
