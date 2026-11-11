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

EXPECTED_MAPPINGS = {
    "negative_feedback": (0, 1, 2, 0.45, -1.0),
    "praise": (0, 0, 1, 0.75, 1.0),
    "preference": (0, 0, 0, 0.50, 0.60),
}
for intent, expected in EXPECTED_MAPPINGS.items():
    response = MODULE._response(intent, "unit-test", 1.0)
    actual = (
        response["goal_state"], response["target_proximity"],
        response["task_status"], response["signal_strength"], response["reward"],
    )
    if actual != expected:
        raise SystemExit(f"Unexpected response mapping for {intent}: {actual}")

print(f"NLP intent classification tests: PASS ({len(CASES)} cases)")
