#!/usr/bin/env python3
"""Focused regression tests for UI text-intent observation mapping."""
import contextlib
import importlib.util
import io
import json
import re
import unittest
from pathlib import Path

MODULE_PATH = Path(__file__).resolve().parents[1] / "Components" / "NLP" / "nlp_intent_service.py"
SPEC = importlib.util.spec_from_file_location("nlp_intent_service", MODULE_PATH)
SERVICE = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(SERVICE)


class FakeToken:
    def __init__(self, text):
        self.text = text
        self.lemma_ = text.lower()
        self.is_punct = False


class FakeNLP:
    def __call__(self, text):
        return [FakeToken(word) for word in re.findall(r"[a-z']+", text.lower())]


class IntentObservationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        SERVICE._NLP = FakeNLP()
        SERVICE._SPACY_LOAD_ATTEMPTED = True

    def parse(self, text):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            SERVICE.handle_line(json.dumps({"text": text}))
        return json.loads(output.getvalue())

    def test_fetch_is_an_active_task_not_feedback(self):
        result = self.parse("fetch the ball")
        self.assertEqual(result["intent"], "request")
        self.assertEqual(result["task_status"], 1)
        self.assertEqual(result["user_status"], 0)

    def test_feed_play_and_explore_are_recognised(self):
        feed = self.parse("feed the companion")
        play = self.parse("play with a toy")
        explore = self.parse("explore the yard")
        self.assertEqual(feed["intent"], "feeding")
        self.assertTrue(feed["food_available"])
        self.assertEqual(play["intent"], "play")
        self.assertTrue(play["play_available"])
        self.assertEqual(explore["intent"], "explore")

    def test_praise_and_scold_do_not_corrupt_task_status(self):
        praise = self.parse("praise the companion")
        scold = self.parse("scold the companion")
        self.assertEqual(praise["intent"], "praise")
        self.assertEqual(praise["user_status"], 1)
        self.assertEqual(praise["task_status"], 0)
        self.assertEqual(scold["intent"], "negative_feedback")
        self.assertEqual(scold["user_status"], 2)
        self.assertEqual(scold["task_status"], 0)


if __name__ == "__main__":
    unittest.main()
