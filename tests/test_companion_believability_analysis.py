#!/usr/bin/env python3
"""Unit tests for the specification-aligned Google Forms analysis pipeline."""
from __future__ import annotations

import csv
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT / "scripts"))
import analyze_companion_believability_study as analysis  # noqa: E402


class CompanionBelievabilityAnalysisTests(unittest.TestCase):
    def test_duplicate_grid_headers_select_the_correct_condition_section(self):
        headers = [
            analysis.SESSION_COL, analysis.CONDITION_COL, analysis.RUN_COL,
            analysis.PARTICIPANT_COL, analysis.CONSENT_COL,
            *analysis.ITEMS, *analysis.ITEMS,
            analysis.SUMMARY_ITEM, analysis.SUMMARY_ITEM,
            analysis.ISSUE_COL, analysis.ISSUE_COL,
        ]
        first_items = ["Agree"] * 21
        first_items[-1] = "Agree"
        second_items = ["Strongly disagree"] * 21
        second_items[-1] = "Agree"
        values = [
            "1", "A", "run-1", "P01", "Yes, I agree",
            *first_items, *second_items,
            "4", "2", "No", "Yes",
        ]
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "form.csv"
            with path.open("w", newline="", encoding="utf-8") as handle:
                writer = csv.writer(handle)
                writer.writerow(headers)
                writer.writerow(values)
            table = analysis.FormTable(path)
            row = table.rows[0]
            first, first_errors = analysis.score_form_row(table, row, 1)
            second, second_errors = analysis.score_form_row(table, row, 2)
            self.assertEqual(first_errors, [])
            self.assertEqual(second_errors, [])
            self.assertEqual(first["item_responses_raw"]["1"], 4)
            self.assertEqual(second["item_responses_raw"]["1"], 1)
            self.assertEqual(first["attention_check_passed"], True)
            self.assertEqual(second["attention_check_passed"], True)
            self.assertEqual(first["summary_believability_item"], 4)
            self.assertEqual(second["summary_believability_item"], 2)

    def test_attention_check_failure_is_flagged(self):
        headers = [analysis.SESSION_COL, *analysis.ITEMS]
        values = ["1", *(["Agree"] * 20), "Disagree"]
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "form.csv"
            with path.open("w", newline="", encoding="utf-8") as handle:
                writer = csv.writer(handle)
                writer.writerow(headers)
                writer.writerow(values)
            table = analysis.FormTable(path)
            _, errors = analysis.score_form_row(table, table.rows[0], 1)
            self.assertIn("attention_check_failed", errors)

    def test_behaviour_metrics_extracts_probe_changes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            steps = []
            for step_id in range(1, 13):
                action = "SIT"
                if step_id == 3:
                    action = "FETCH"
                elif step_id == 7:
                    action = "NUZZLE"
                elif step_id == 5:
                    action = "BARK"
                elif step_id == 11:
                    action = "SIT"
                steps.append({"step_id": step_id, "selected_action": action})
            (root / "run-steps.jsonl").write_text(
                "".join(json.dumps(row) + "\n" for row in steps), encoding="utf-8"
            )
            (root / "interaction-events-run.jsonl").write_text("", encoding="utf-8")
            metrics, errors = analysis.behaviour_metrics("run", root)
            self.assertEqual(errors, [])
            self.assertTrue(metrics["memory_probe_shift"]["ball_preference_probe"]["action_changed"])
            self.assertTrue(metrics["memory_probe_shift"]["noise_dislike_probe"]["action_changed"])
            self.assertEqual(metrics["guided_steps_observed"], 12)

    def test_action_entropy_is_zero_for_one_repeated_action(self):
        self.assertEqual(analysis.action_entropy(["SIT", "SIT", "SIT"]), 0.0)


if __name__ == "__main__":
    unittest.main()
