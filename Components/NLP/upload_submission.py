#!/usr/bin/env python3
"""Upload one queued questionnaire response over HTTPS.

The participant token is retained only in the local queue record and removed
from the payload before transmission.
"""
import json
import pathlib
import sys
import urllib.error
import urllib.request


def main():
    if len(sys.argv) != 3:
        return 2
    try:
        config = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
        record = json.loads(pathlib.Path(sys.argv[2]).read_text(encoding="utf-8"))
        endpoint = str(config.get("endpoint", "")).strip()
        token = str(record.pop("_upload_token", "")).strip()
        submission_id = str(record["submission_id"])
        if not endpoint.startswith("https://") or not token:
            print("HTTPS endpoint or participant upload token is missing.", file=sys.stderr)
            return 3

        request = urllib.request.Request(
            endpoint,
            data=json.dumps(record, ensure_ascii=False).encode("utf-8"),
            headers={
                "Content-Type": "application/json; charset=utf-8",
                "Authorization": "Bearer " + token,
                "Idempotency-Key": submission_id,
            },
            method="POST",
        )
        with urllib.request.urlopen(request, timeout=15) as response:
            acknowledgement = json.loads(response.read().decode("utf-8"))
        if acknowledgement.get("stored") is True and acknowledgement.get("submission_id") == submission_id:
            return 0
        print("Server did not confirm durable storage.", file=sys.stderr)
        return 5
    except (OSError, ValueError, KeyError, urllib.error.URLError) as error:
        print("Submission upload failed: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
