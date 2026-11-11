#!/usr/bin/env python3
"""Deterministic unit tests for the NLP keyword fallback and response mapping."""
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SERVICE = ROOT / "Components" / "NLP" / "nlp_intent_service.py"
SPEC = importlib.util.spec_from_file_location("nlp_intent_service", SERVICE)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)

CASES = {
    "I prefer the rope toy": "preference",
    "My favourite toy is the stick": "preference",
    "My favorite is the ball": "preference",
    "I like treats": "preference",
    "I love playing": "preference",
    "I don't like the ball": "negative_feedback",
    "I do not prefer loud noise": "negative_feedback",
    "Stop, that is annoying": "negative_feedback",
    "Please fetch the ball": "request",
    "Bring me the rope": "request",
    "Good dog, well done": "praise",
    "Hello, how are you": "greeting",
    "The thunder is loud": "disturbance",
    "I am tired and need rest": "rest",
    "A sentence with no supported intent": "unknown",
    "Like this": "unknown",
}

for utterance, expected in CASES.items():
    actual = MODULE.classify_keyword(utterance)
    if actual != expected:
        raise SystemExit(f"{utterance!r}: expected {expected}, got {actual}")

for intent, expected_status in (
    ("negative_feedback", 2),
    ("praise", 1),
    ("preference", 0),
):
    response = MODULE._response(intent, "unit-test", 1.0)
    if response["user_status"] if "user_status" in response else expected_status != expected_status:
        raise SystemExit(f"Unexpected response mapping for {intent}: {response}")

print(f"NLP intent classification tests: PASS ({len(CASES)} cases)")
