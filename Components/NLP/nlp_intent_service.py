#!/usr/bin/env python3
"""Deterministic spaCy intent adapter for the SFML companion UI.

Input: one JSON object per stdin line, e.g. {"text": "I prefer the rope toy"}.
Output: one JSON object per input line on stdout. Diagnostics go to stderr.
If spaCy or en_core_web_sm is unavailable, conservative keyword rules are used.
"""
import json
import re
import sys

_NLP = None
_SPACY_LOAD_ATTEMPTED = False

INTENTS = {
    "greeting": (0, 1, 0, 0.55, 0.65),
    "request": (1, 1, 1, 0.85, 0.65),
    "disturbance": (2, 0, 0, 0.95, 0.0),
    "rest": (2, 2, 0, 0.20, 0.25),
    "negative_feedback": (0, 1, 2, 0.45, -1.0),
    "praise": (0, 0, 1, 0.75, 1.0),
    "preference": (0, 0, 0, 0.50, 0.60),
    "unknown": (0, 1, 0, 0.45, 0.25),
}

NEGATIVE_WORDS = {"stop", "annoy", "bad", "wrong", "hate", "dislike"}
PREFERENCE_WORDS = {"favorite", "favourite", "prefer"}
AFFINITY_LEMMAS = {"like", "love"}
REQUEST_WORDS = {"fetch", "bring", "get", "retrieve", "ball", "toy", "find"}
DISTURBANCE_WORDS = {"noise", "loud", "scare", "scared", "frighten", "bang", "thunder"}
REST_WORDS = {"rest", "sleep", "tired", "quiet", "lie"}
GREETING_WORDS = {"hello", "hi", "hey", "greet"}
PRAISE_WORDS = {"praise", "clever", "excellent"}


def _has_negative_language(raw, lemmas):
    lowered = raw.lower()
    return bool(lemmas & NEGATIVE_WORDS) or any(
        phrase in lowered for phrase in ("don't", "do not", "doesn't", "does not",
                                         "didn't", "did not", "not like", "not love")
    )


def _is_imperative(doc):
    """Avoid treating first-person statements such as 'I like...' as commands."""
    has_subject = any(token.dep_ in ("nsubj", "nsubjpass", "csubj") for token in doc)
    root = next((token for token in doc if token.dep_ == "ROOT"), None)
    if root is None or has_subject:
        return False
    # spaCy's small English model does not expose a reliable imperative mood
    # feature, so a subject-less verbal root is used as a conservative cue.
    return (root.pos_ == "VERB" and root.tag_ in ("VB", "VBP")
            and root.lemma_.lower() in {"fetch", "bring", "get", "retrieve", "find"})


def classify(doc, raw):
    lemmas = {token.lemma_.lower() for token in doc if not token.is_punct}
    words = {token.text.lower() for token in doc if not token.is_punct}
    lowered = raw.lower()

    # 1. Negative feedback takes precedence over praise, preference and commands.
    if _has_negative_language(lowered, lemmas):
        return "negative_feedback"

    # 2. Praise / affirmation.
    praise_phrases = (
        "good boy", "good girl", "good dog", "good job",
        "well done", "good pet",
    )
    if (any(phrase in lowered for phrase in praise_phrases)
            or lemmas & PRAISE_WORDS
            or ("good" in words and any(
                token.dep_ in ("amod", "attr", "ROOT")
                for token in doc if token.text.lower() == "good"
            ))):
        return "praise"

    # 3. Statements of preference. Require a personal subject for like/love,
    # while "prefer" and "favorite" are sufficiently explicit on their own.
    has_affinity_verb = bool(lemmas & AFFINITY_LEMMAS)
    has_subject = any(token.dep_ in ("nsubj", "nsubjpass", "csubj") for token in doc)
    if lemmas & PREFERENCE_WORDS or (has_affinity_verb and has_subject):
        if not _is_imperative(doc):
            return "preference"

    # 4. Commands / requests.
    if lemmas & REQUEST_WORDS or _is_imperative(doc):
        return "request"

    # 5. Other contextual categories.
    if lemmas & DISTURBANCE_WORDS:
        return "disturbance"
    if lemmas & REST_WORDS:
        return "rest"
    if lemmas & GREETING_WORDS or "how are you" in lowered:
        return "greeting"

    return "unknown"


def classify_keyword(raw):
    """Fallback classifier with the same priority order as the spaCy path."""
    lowered = raw.lower()
    words = set(re.findall(r"[a-z']+", lowered))

    if (words & {"stop", "annoying", "annoy", "bad", "wrong", "hate", "dislike"}
            or any(phrase in lowered for phrase in (
                "don't", "do not", "doesn't", "does not",
                "didn't", "did not", "not like", "not love"
            ))):
        return "negative_feedback"

    praise_phrases = ("good boy", "good girl", "good dog", "good job",
                      "well done", "good pet")
    if any(phrase in lowered for phrase in praise_phrases) or words & {
        "great", "praise", "excellent", "clever"
    }:
        return "praise"

    if words & {"favorite", "favourite", "prefer"}:
        return "preference"
    # A first-person subject is required for like/love to avoid classifying
    # imperative "like this" as a preference.
    if words & {"like", "love"} and words & {"i", "we", "my"}:
        return "preference"

    if words & REQUEST_WORDS:
        return "request"
    if words & DISTURBANCE_WORDS:
        return "disturbance"
    if words & REST_WORDS:
        return "rest"
    if words & GREETING_WORDS or "how are you" in lowered:
        return "greeting"
    return "unknown"


def _response(intent, method, confidence):
    goal, proximity, task, signal, reward = INTENTS.get(intent, INTENTS["unknown"])
    return {
        "intent": intent,
        "goal_state": goal,
        "target_proximity": proximity,
        "task_status": task,
        "signal_strength": signal,
        "reward": reward,
        "confidence": confidence,
        "method": method,
    }


def handle_line(line):
    request = json.loads(line or "{}")
    raw = str(request.get("text", ""))[:500]
    if not raw.strip():
        print(json.dumps(_response("unknown", "empty", 0.0)), flush=True)
        return

    global _NLP, _SPACY_LOAD_ATTEMPTED
    try:
        if not _SPACY_LOAD_ATTEMPTED:
            _SPACY_LOAD_ATTEMPTED = True
            import spacy
            _NLP = spacy.load("en_core_web_sm")
        if _NLP is None:
            raise RuntimeError("spaCy model unavailable")
        intent = classify(_NLP(raw), raw)
        method = "spacy"
        confidence = 0.80
    except Exception as exc:
        print("spaCy unavailable; using keyword fallback: " + str(exc), file=sys.stderr)
        intent = classify_keyword(raw)
        method = "keyword-fallback"
        confidence = 0.45

    print(json.dumps(_response(intent, method, confidence)), flush=True)


def main():
    # Persistent line-delimited protocol: one JSON request in, one response out.
    for line in sys.stdin:
        try:
            handle_line(line)
        except Exception as exc:
            response = _response("unknown", "service-error", 0.0)
            response["error"] = str(exc)
            print(json.dumps(response), flush=True)


if __name__ == "__main__":
    main()
