import importlib.util
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "download_study_submissions.py"
SPEC = importlib.util.spec_from_file_location("download_study_submissions", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(MODULE)


def submission(participant=20, session=1):
    condition = ("B" if session == 1 else "A") if participant % 2 == 0 else ("A" if session == 1 else "B")
    return {
        "schema_version": 1,
        "submission_id": f"P{participant:02d}-S{session}",
        "participant_number": participant,
        "session_index": session,
        "condition": condition,
        "questionnaire_version": "v1",
        "responses": {
            "condition_questionnaire": {"answers": [
                {"question_id": "believability", "answer_type": "likert", "value": 4}
            ]},
            "comparison_questionnaire": (
                None if session == 1 else {"answers": [
                    {"question_id": "preferred_agent", "answer_type": "single_choice", "value": "A"}
                ]}
            ),
        },
    }


class SubmissionValidationTests(unittest.TestCase):
    def test_valid_even_participant_session_one(self):
        data = submission(20, 1)
        self.assertEqual(MODULE.validate_submission(data, "P20-S1.json"), [])

    def test_valid_even_participant_session_two(self):
        data = submission(20, 2)
        self.assertEqual(MODULE.validate_submission(data, "P20-S2.json"), [])

    def test_rejects_condition_mismatch(self):
        data = submission(20, 1)
        data["condition"] = "A"
        self.assertIn(
            "condition does not match the participant counterbalancing assignment",
            MODULE.validate_submission(data, "P20-S1.json"),
        )

    def test_rejects_session_one_comparison_payload(self):
        data = submission(20, 1)
        data["responses"]["comparison_questionnaire"] = {"answers": []}
        self.assertIn(
            "session 1 comparison_questionnaire must be null",
            MODULE.validate_submission(data, "P20-S1.json"),
        )

    def test_rejects_submission_id_filename_mismatch(self):
        data = submission(20, 1)
        data["submission_id"] = "P20-S2"
        self.assertIn(
            "submission_id does not match filename",
            MODULE.validate_submission(data, "P20-S1.json"),
        )

    def test_filters_participant_and_session(self):
        file = {"name": "P20-S2.json"}
        self.assertTrue(MODULE.matching_submission(file, 20, 2))
        self.assertFalse(MODULE.matching_submission(file, 20, 1))
        self.assertFalse(MODULE.matching_submission(file, 19, 2))

    def test_existing_local_copy_requires_explicit_overwrite(self):
        import tempfile
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            original = root / "P20-S1.json"
            original.write_text("existing", encoding="utf-8")
            self.assertEqual(
                MODULE.output_path_for(root, "P20-S1.json", "driveid123", False).name,
                "P20-S1.duplicate-driveid.json",
            )
            self.assertEqual(
                MODULE.output_path_for(root, "P20-S1.json", "driveid123", True),
                original,
            )


if __name__ == "__main__":
    unittest.main()
