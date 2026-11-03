#!/usr/bin/env python3
"""POST one queued Animal Agent questionnaire response to the configured HTTPS endpoint.

The endpoint must durably store the response and return JSON:
{"stored": true, "submission_id": "<same id>"}.
The Idempotency-Key header makes retries safe if the server stored a request but
the client did not receive its acknowledgement.
"""
import json
import pathlib
import sys
import urllib.error
import urllib.request


def main():
    if len(sys.argv) != 3:
        return 2

    config_path = pathlib.Path(sys.argv[1])
    response_path = pathlib.Path(sys.argv[2])
    try:
        config = json.loads(config_path.read_text(encoding="utf-8"))
        record = json.loads(response_path.read_text(encoding="utf-8"))
        endpoint = str(config.get("endpoint", "")).strip()
        if not endpoint.startswith("https://"):
            print("Upload endpoint is not configured as HTTPS.", file=sys.stderr)
            return 3
        submission_id = str(record["submission_id"])
        payload = json.dumps(record, ensure_ascii=False).encode("utf-8")
        request = urllib.request.Request(
            endpoint,
            data=payload,
            headers={
                "Content-Type": "application/json; charset=utf-8",
                "Idempotency-Key": submission_id,
            },
            method="POST",
        )
        with urllib.request.urlopen(request, timeout=15) as response:
            if not 200 <= response.status < 300:
                return 4
            acknowledgement = json.loads(response.read().decode("utf-8"))
        if acknowledgement.get("stored") is True and acknowledgement.get("submission_id") == submission_id:
            return 0
        print("Server did not confirm durable storage of this submission.", file=sys.stderr)
        return 5
    except (OSError, ValueError, KeyError, urllib.error.URLError) as error:
        print("Submission upload failed: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
