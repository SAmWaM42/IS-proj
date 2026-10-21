#!/usr/bin/env python3
"""Regression tests for the interactive-simulation JSONL validator."""
from __future__ import annotations

import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from validate_interaction_log import validate  # noqa: E402


def action_record() -> dict:
    return {
        "schema_version": 1,
        "run_id": "run-test",
        "interaction_id": 1,
        "sequence_index": 0,
        "condition_code": "A",
        "input_summary": "Ask the companion to fetch the ball",
        "processing_status": "completed",
        "observation": {"goal_state": 1},
        "baseline": {"action": "FETCH"},
        "proposed": {
            "action": "FETCH",
            "memory_count_before": 0,
            "memory_count_after": 1,
            "affect_before": {},
            "affect_after": {},
        },
    }


def preference_record() -> dict:
    return {
        "schema_version": 2,
        "run_id": "run-test",
        "interaction_id": 1,
        "sequence_index": 0,
        "condition_code": "A",
        "input_summary": "I like ball",
        "processing_status": "recorded_no_action",
        "utterance_analysis": {
            "kind": "preference_statement",
            "relation": "likes",
            "object": "ball",
            "memory_updated": True,
            "response_required": False,
            "response_action": "NONE",
        },
        "learning_update_enabled": False,
    }


def unclassified_record() -> dict:
    return {
        "schema_version": 2,
        "run_id": "run-test",
        "interaction_id": 1,
        "sequence_index": 0,
        "condition_code": "A",
        "input_summary": "The sky is blue",
        "processing_status": "unclassified_no_action",
        "utterance_analysis": {
            "kind": "unclassified",
            "memory_updated": False,
            "response_required": False,
            "response_action": "NONE",
        },
        "learning_update_enabled": False,
    }


class InteractionLogValidatorTests(unittest.TestCase):
    def validate_rows(self, rows: list[dict]) -> tuple[int, str]:
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "events.jsonl"
            path.write_text("".join(json.dumps(row) + "\n" for row in rows), encoding="utf-8")
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                result = validate(path)
            return result, output.getvalue()

    def test_accepts_schema_one_action_interaction(self) -> None:
        result, output = self.validate_rows([action_record()])
        self.assertEqual(result, 0, output)

    def test_accepts_preference_statement_without_action_fields(self) -> None:
        result, output = self.validate_rows([preference_record()])
        self.assertEqual(result, 0, output)

    def test_accepts_unclassified_utterance_without_action_fields(self) -> None:
        result, output = self.validate_rows([unclassified_record()])
        self.assertEqual(result, 0, output)

    def test_rejects_malformed_no_action_event(self) -> None:
        row = preference_record()
        del row["utterance_analysis"]["response_action"]
        result, output = self.validate_rows([row])
        self.assertEqual(result, 1)
        self.assertIn("response_action", output)

    def test_rejects_unsupported_schema_version(self) -> None:
        row = unclassified_record()
        row["schema_version"] = 3
        result, output = self.validate_rows([row])
        self.assertEqual(result, 1)
        self.assertIn("unsupported schema_version", output)

    def test_accepts_mixed_action_and_no_action_records_in_sequence(self) -> None:
        action = action_record()
        action["interaction_id"] = 3
        action["sequence_index"] = 2
        preference = preference_record()
        preference["interaction_id"] = 1
        preference["sequence_index"] = 0
        unclassified = unclassified_record()
        unclassified["interaction_id"] = 2
        unclassified["sequence_index"] = 1
        result, output = self.validate_rows([preference, unclassified, action])
        self.assertEqual(result, 0, output)


if __name__ == "__main__":
    unittest.main()
