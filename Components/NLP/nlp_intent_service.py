#!/usr/bin/env python3
"""Small, deterministic spaCy intent adapter for the SFML companion UI.

Input: one JSON object on stdin: {"text": "..."}
Output: one JSON object on stdout. Diagnostics are written to stderr.
Falls back to conservative keyword rules when spaCy/model is unavailable.
"""
import json
import re
import sys

INTENTS = {
    "greeting": (0, 1, 0, 0.55, 0.65),
    "request": (1, 1, 1, 0.85, 0.65),
    "disturbance": (2, 0, 0, 0.95, 0.0),
    "rest": (2, 2, 0, 0.20, 0.25),
    "negative_feedback": (0, 1, 2, 0.45, -1.0),
    "praise": (0, 0, 1, 0.75, 1.0),
    "unknown": (0, 1, 0, 0.45, 0.25),
}

def classify(tokens, raw):
    lemmas = {t.lemma_.lower() for t in tokens if not t.is_punct}
    words = {t.text.lower() for t in tokens if not t.is_punct}
    # Negative feedback takes priority over other terms, to avoid interpreting
    # "don't fetch that" as a positive fetch request.
    if lemmas & {"stop", "annoy", "bad", "wrong", "hate", "dislike"} or "don't" in raw.lower() or "do not" in raw.lower():
        return "negative_feedback"
    if lemmas & {"fetch", "bring", "get", "retrieve", "ball", "toy", "find"}:
        return "request"
    if lemmas & {"noise", "loud", "scare", "scared", "frighten", "bang", "thunder"}:
        return "disturbance"
    if lemmas & {"rest", "sleep", "tired", "quiet", "lie"}:
        return "rest"
    if lemmas & {"good", "great", "well", "clever", "praise", "excellent"}:
        return "praise"
    if lemmas & {"hello", "hi", "hey", "greet"} or "how are you" in raw.lower():
        return "greeting"
    return "unknown"

def handle_line(line):
    request = json.loads(line or "{}")
    raw = str(request.get("text", ""))[:500]
    if not raw.strip():
        print(json.dumps({"intent": "unknown", "goal_state": 0, "target_proximity": 1,
                          "task_status": 0, "signal_strength": 0.45, "reward": 0.25,
                          "confidence": 0.0, "method": "empty"}), flush=True)
        return
    try:
        import spacy
        nlp = spacy.load("en_core_web_sm")
        doc = nlp(raw)
        intent = classify(doc, raw)
        method = "spacy"
    except Exception as exc:
        # Keep this fallback deliberately transparent: the UI labels its source.
        print("spaCy unavailable; using keyword fallback: " + str(exc), file=sys.stderr)
        words = set(re.findall(r"[a-z']+", raw.lower()))
        if words & {"stop", "annoying", "bad", "wrong", "hate", "don't"}:
            intent = "negative_feedback"
        elif words & {"fetch", "bring", "ball", "toy", "retrieve"}:
            intent = "request"
        elif words & {"noise", "loud", "scared", "thunder", "bang"}:
            intent = "disturbance"
        elif words & {"rest", "sleep", "tired", "quiet"}:
            intent = "rest"
        elif words & {"good", "great", "praise", "excellent"}:
            intent = "praise"
        elif words & {"hello", "hi", "hey"}:
            intent = "greeting"
        else:
            intent = "unknown"
        method = "keyword-fallback"
    goal, proximity, task, signal, reward = INTENTS[intent]
    print(json.dumps({
        "intent": intent, "goal_state": goal, "target_proximity": proximity,
        "task_status": task, "signal_strength": signal, "reward": reward,
        "confidence": 0.8 if method == "spacy" else 0.45, "method": method
    }), flush=True)

def main():
    # Persistent line-delimited protocol: one JSON request in, one JSON response out.
    for line in sys.stdin:
        try:
            handle_line(line)
        except Exception as exc:
            print(json.dumps({
                "intent": "unknown", "goal_state": 0, "target_proximity": 1,
                "task_status": 0, "signal_strength": 0.45, "reward": 0.25,
                "confidence": 0.0, "method": "service-error", "error": str(exc)
            }), flush=True)

if __name__ == "__main__":
    main()
