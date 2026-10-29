#!/usr/bin/env python3
"""Unit tests for study-rating validation and paired analysis."""
from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from analyze_believability_study import behaviour_metrics, read_sessions, score_condition  # noqa: E402


class BelievabilityPipelineTests(unittest.TestCase):
    def test_five_item_mean(self):
        condition = {"questionnaire": {
            "plausibility": 4, "coherence": 4, "responsiveness": 3,
            "consistency": 5, "continuity": 4
        }}
        score, errors = score_condition(condition)
        self.assertEqual(errors, [])
        self.assertEqual(score, 4.0)

    def test_missing_rating_invalidates_primary_score(self):
        condition = {"questionnaire": {
            "plausibility": 4, "coherence": 4, "responsiveness": 3,
            "consistency": 5
        }}
        score, errors = score_condition(condition)
        self.assertIsNone(score)
        self.assertTrue(errors)

    def test_repeated_scenario_consistency_is_reported_when_available(self):
        baseline = {"trials": [
            {"scenario_id": "BEL-X", "action_internal": "SIT"},
            {"scenario_id": "BEL-X", "action_internal": "SIT"},
        ]}
        proposed = {"trials": [
            {"scenario_id": "BEL-X", "action_internal": "NUZZLE"},
            {"scenario_id": "BEL-X", "action_internal": "NUZZLE"},
        ]}
        metrics = behaviour_metrics(baseline, proposed)
        self.assertEqual(metrics["baseline_repeated_scenario_consistency"]["score"], 1.0)
        self.assertEqual(metrics["proposed_repeated_scenario_consistency"]["score"], 1.0)
        self.assertEqual(metrics["cross_condition_action_agreement"], 0.0)

    def test_adaptation_is_not_claimed_without_matched_control(self):
        metrics = behaviour_metrics(
            {"trials": [{"scenario_id": "BEL-01", "action_internal": "SIT"}]},
            {"trials": [{"scenario_id": "BEL-01", "action_internal": "NUZZLE"}]},
        )
        self.assertEqual(metrics["adaptation"]["status"],
                         "not_estimable_missing_matched_event_control_pairs")

    def test_paired_sessions_are_mapped_by_condition_not_label(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "session.json"
            items = ["plausibility", "coherence", "responsiveness", "consistency", "continuity"]
            baseline = {key: 3 for key in items}
            proposed = {key: 4 for key in items}
            payload = {
                "session_id": "anonymous-test-session", "consent": True, "completed": True,
                "condition_key": {"A": "proposed", "B": "baseline"},
                "condition_order": ["A", "B"],
                "conditions": [
                    {"condition_code": "A", "questionnaire": proposed},
                    {"condition_code": "B", "questionnaire": baseline}
                ]
            }
            path.write_text(json.dumps(payload), encoding="utf-8")
            sessions, exclusions = read_sessions([path])
            self.assertEqual(exclusions, [])
            self.assertEqual(len(sessions), 1)
            self.assertEqual(sessions[0]["baseline_score"], 3.0)
            self.assertEqual(sessions[0]["proposed_score"], 4.0)
            self.assertEqual(sessions[0]["difference_proposed_minus_baseline"], 1.0)


if __name__ == "__main__":
    unittest.main()
