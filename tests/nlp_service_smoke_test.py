#!/usr/bin/env python3
"""Smoke-test the persistent JSON-lines NLP service with or without spaCy installed."""
import json
import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
service = root / "Components" / "NLP" / "nlp_intent_service.py"
requests = [
    {"text": "Please fetch the ball"},
    {"text": "Stop, you are annoying"},
    {"text": "hello"},
]
payload = "".join(json.dumps(item) + "\n" for item in requests)
completed = subprocess.run(
    [sys.executable, str(service)],
    input=payload,
    text=True,
    capture_output=True,
    timeout=15,
    cwd=root,
    check=False,
)
if completed.returncode != 0:
    raise SystemExit(f"NLP service exited {completed.returncode}: {completed.stderr}")
lines = [line for line in completed.stdout.splitlines() if line.strip()]
if len(lines) != len(requests):
    raise SystemExit(f"Expected {len(requests)} JSON responses, got {len(lines)}; stderr={completed.stderr!r}")
responses = []
for line in lines:
    try:
        response = json.loads(line)
    except json.JSONDecodeError as exc:
        raise SystemExit(f"Invalid JSON response {line!r}: {exc}") from exc
    if "intent" not in response or "method" not in response:
        raise SystemExit(f"Response missing intent/method: {response!r}")
    if response["method"] not in {"spacy", "keyword-fallback", "service-error"}:
        raise SystemExit(f"Unexpected NLP method: {response['method']!r}")
    responses.append(response)
expected = ["request", "negative_feedback", "greeting"]
actual = [item["intent"] for item in responses]
if actual != expected:
    raise SystemExit(f"Unexpected intents: expected {expected}, got {actual}; stderr={completed.stderr!r}")
print("NLP JSON-lines smoke test: PASS")
print("Methods: " + ", ".join(item["method"] for item in responses))
