import argparse
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "analyze_questionnaire_submissions.py"
SPEC = importlib.util.spec_from_file_location("questionnaire_analysis", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(MODULE)


def submission(participant=20, session=1):
    condition = MODULE.expected_condition(participant, session)
    return {
        "schema_version": 1,
        "submission_id": f"P{participant:02d}-S{session}",
        "participant_number": participant,
        "session_index": session,
        "condition": condition,
        "questionnaire_version": "test-v1",
        "responses": {
            "condition_questionnaire": {"answers": [
                {"question_id": "c1", "answer_type": "likert", "value": 4},
                {"question_id": "c3", "answer_type": "likert", "value": 2},
                {"question_id": "c21", "answer_type": "likert", "value": 4},
                {"question_id": "summary_believability", "answer_type": "integer", "value": 4},
                {"question_id": "session_feedback", "answer_type": "text", "value": "private free text"}
            ]},
            "comparison_questionnaire": None if session == 1 else {"answers": [
                {"question_id": "preferred_agent", "answer_type": "single_choice", "value": "A"}
            ]}
        }
    }


class QuestionnaireAnalysisTests(unittest.TestCase):
    def test_counterbalancing_matches_odd_even_contract(self):
        self.assertEqual(MODULE.expected_condition(7, 1), "A")
        self.assertEqual(MODULE.expected_condition(7, 2), "B")
        self.assertEqual(MODULE.expected_condition(8, 1), "B")
        self.assertEqual(MODULE.expected_condition(8, 2), "A")

    def test_valid_session_one_and_two(self):
        self.assertEqual(MODULE.validate_submission(submission(20, 1), "P20-S1.json"), [])
        self.assertEqual(MODULE.validate_submission(submission(20, 2), "P20-S2.json"), [])

    def test_rejects_condition_mismatch(self):
        value = submission(20, 1)
        value["condition"] = "A"
        self.assertTrue(MODULE.validate_submission(value, "P20-S1.json"))

    def test_rejects_duplicate_question_ids(self):
        value = submission(20, 1)
        value["responses"]["condition_questionnaire"]["answers"].append(
            {"question_id": "believability", "answer_type": "likert", "value": 3}
        )
        errors = MODULE.validate_submission(value, "P20-S1.json")
        self.assertTrue(any("duplicates question_id" in error for error in errors))

    def test_reverse_scoring_and_numeric_filter(self):
        scoring = {"reverse_scored_question_ids": ["negative_item"], "subscales": {}}
        q = {"answers": [
            {"question_id": "negative_item", "answer_type": "likert", "value": 2},
            {"question_id": "comment", "answer_type": "text", "value": "not analysed"}
        ]}
        self.assertEqual(MODULE.numeric_answers(q, scoring), {"negative_item": 4.0})

    def test_analysis_outputs_aggregate_files_without_copying_free_text(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "input"
            output = root / "analysis"
            source.mkdir()
            (source / "P20-S1.json").write_text(json.dumps(submission(20, 1)), encoding="utf-8")
            (source / "P20-S2.json").write_text(json.dumps(submission(20, 2)), encoding="utf-8")
            args = argparse.Namespace(input_dir=source, out_dir=output, scoring_config=None)
            self.assertEqual(MODULE.run(args), 0)
            self.assertTrue((output / "analysis_report.json").is_file())
            self.assertTrue((output / "questionnaire_condition_summary.csv").is_file())
            self.assertTrue((output / "paired_question_differences.csv").is_file())
            self.assertTrue((output / "comparison_response_counts.csv").is_file())
            self.assertTrue((output / "question_c1.svg").is_file())
            report = json.loads((output / "analysis_report.json").read_text(encoding="utf-8"))
            self.assertEqual(report["attention_check_passed_submissions"], 2)
            self.assertEqual(report["attention_eligible_complete_pairs"], 1)
            generated = "\\n".join(p.read_text(encoding="utf-8") for p in output.iterdir() if p.is_file())
            self.assertNotIn("private free text", generated)

    def test_duplicate_participant_session_is_flagged(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp)
            (source / "P20-S1.json").write_text(json.dumps(submission(20, 1)), encoding="utf-8")
            (source / "P020-S1.json").write_text(json.dumps(submission(20, 1)), encoding="utf-8")
            records, issues = MODULE.read_input_files(source)
            self.assertEqual(records, [])
            self.assertTrue(any(row["status"] == "excluded" for row in issues))


if __name__ == "__main__":
    unittest.main()
