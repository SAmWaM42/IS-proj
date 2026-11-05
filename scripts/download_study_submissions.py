#!/usr/bin/env python3
"""Download questionnaire submissions from the Animal Agent Google Drive folder.

Uses the researcher's existing Google OAuth refresh-token credentials. No
participant token or Vercel endpoint is involved.
"""
from __future__ import annotations

import argparse
import json
import math
import os
import re
import sys
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

DRIVE_API = "https://www.googleapis.com/drive/v3"
TOKEN_URL = "https://oauth2.googleapis.com/token"
DEFAULT_OUTPUT = Path.home() / "AnimalAgentStudyData" / "submissions"
FILE_RE = re.compile(r"^(P(?P<participant>[0-9]{2,4})-S(?P<session>[12]))\.json$")
QUESTION_ID_RE = re.compile(r"^[a-z][a-z0-9_]{0,63}$")
MAX_FILE_BYTES = 5_000_000
TIMEOUT_SECONDS = 30


class RetrievalError(RuntimeError):
    """A safe-to-display retrieval or configuration error."""


def require_environment() -> dict[str, str]:
    names = (
        "GOOGLE_CLIENT_ID",
        "GOOGLE_CLIENT_SECRET",
        "GOOGLE_REFRESH_TOKEN",
        "GOOGLE_DRIVE_FOLDER_ID",
    )
    missing = [name for name in names if not os.environ.get(name)]
    if missing:
        raise RetrievalError(
            "Missing environment variables: " + ", ".join(missing)
        )
    return {name: os.environ[name] for name in names}


def http_request(
    url: str,
    *,
    method: str = "GET",
    headers: dict[str, str] | None = None,
    data: bytes | None = None,
) -> bytes:
    request = urllib.request.Request(
        url, data=data, headers=headers or {}, method=method
    )
    try:
        with urllib.request.urlopen(request, timeout=TIMEOUT_SECONDS) as response:
            return response.read(MAX_FILE_BYTES + 1)
    except urllib.error.HTTPError as exc:
        # Do not echo request URLs or headers: they can contain sensitive data.
        try:
            payload = json.loads(exc.read().decode("utf-8"))
            message = payload.get("error", {}).get("message", "request rejected")
        except (UnicodeDecodeError, json.JSONDecodeError, AttributeError):
            message = "request rejected"
        raise RetrievalError(f"Google API returned HTTP {exc.code}: {message}") from None
    except urllib.error.URLError as exc:
        raise RetrievalError(f"Could not reach Google API: {exc.reason}") from None


def refresh_access_token(config: dict[str, str]) -> str:
    body = urllib.parse.urlencode({
        "client_id": config["GOOGLE_CLIENT_ID"],
        "client_secret": config["GOOGLE_CLIENT_SECRET"],
        "refresh_token": config["GOOGLE_REFRESH_TOKEN"],
        "grant_type": "refresh_token",
    }).encode("utf-8")
    raw = http_request(
        TOKEN_URL,
        method="POST",
        headers={"Content-Type": "application/x-www-form-urlencoded"},
        data=body,
    )
    try:
        payload = json.loads(raw.decode("utf-8"))
        token = payload["access_token"]
    except (UnicodeDecodeError, json.JSONDecodeError, KeyError, TypeError):
        raise RetrievalError("Google did not return a valid access token.") from None
    if not isinstance(token, str) or not token:
        raise RetrievalError("Google returned an empty access token.")
    return token


def list_submission_files(access_token: str, folder_id: str) -> list[dict[str, Any]]:
    query = f"'{folder_id}' in parents and trashed = false and mimeType = 'application/json'"
    params = {
        "q": query,
        "spaces": "drive",
        "pageSize": "1000",
        "orderBy": "name",
        "fields": "nextPageToken,files(id,name,mimeType,size,modifiedTime,appProperties)",
    }
    files: list[dict[str, Any]] = []
    while True:
        url = f"{DRIVE_API}/files?{urllib.parse.urlencode(params)}"
        raw = http_request(url, headers={"Authorization": f"Bearer {access_token}"})
        try:
            payload = json.loads(raw.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            raise RetrievalError("Google Drive returned an invalid file listing.") from None
        batch = payload.get("files", [])
        if not isinstance(batch, list):
            raise RetrievalError("Google Drive returned an unexpected file listing.")
        files.extend(item for item in batch if isinstance(item, dict))
        next_page = payload.get("nextPageToken")
        if not next_page:
            break
        params["pageToken"] = next_page
    return files


def matching_submission(file: dict[str, Any], participant: int | None,
                        session: int | None) -> bool:
    name = file.get("name")
    if not isinstance(name, str):
        return False
    match = FILE_RE.fullmatch(name)
    if not match:
        return False
    return (
        (participant is None or int(match.group("participant")) == participant)
        and (session is None or int(match.group("session")) == session)
    )


def questionnaire_errors(value: Any, label: str) -> list[str]:
    errors: list[str] = []
    if not isinstance(value, dict) or set(value) != {"answers"}:
        return [f"{label} must contain only an answers array"]
    answers = value["answers"]
    if not isinstance(answers, list):
        return [f"{label}.answers must be an array"]
    allowed_types = {
        "likert", "integer", "number", "single_choice",
        "multiple_choice", "text", "boolean",
    }
    for index, answer in enumerate(answers):
        prefix = f"{label}.answers[{index}]"
        if not isinstance(answer, dict) or set(answer) != {"question_id", "answer_type", "value"}:
            errors.append(f"{prefix} must contain question_id, answer_type and value only")
            continue
        question_id = answer["question_id"]
        answer_type = answer["answer_type"]
        answer_value = answer["value"]
        if not isinstance(question_id, str) or not QUESTION_ID_RE.fullmatch(question_id):
            errors.append(f"{prefix}.question_id is invalid")
        if not isinstance(answer_type, str) or answer_type not in allowed_types:
            errors.append(f"{prefix}.answer_type is invalid")
            continue
        valid = True
        if answer_type in {"likert", "integer"}:
            valid = isinstance(answer_value, int) and not isinstance(answer_value, bool)
        elif answer_type == "number":
            valid = (isinstance(answer_value, (int, float))
                     and not isinstance(answer_value, bool)
                     and math.isfinite(answer_value))
        elif answer_type == "single_choice":
            valid = isinstance(answer_value, str)
        elif answer_type == "multiple_choice":
            valid = (isinstance(answer_value, list)
                     and all(isinstance(item, str) for item in answer_value)
                     and len(answer_value) == len(set(answer_value)))
        elif answer_type == "text":
            valid = isinstance(answer_value, str) and len(answer_value) <= 2000
        elif answer_type == "boolean":
            valid = isinstance(answer_value, bool)
        if not valid:
            errors.append(f"{prefix}.value does not match {answer_type}")
    return errors


def validate_submission(data: Any, expected_name: str) -> list[str]:
    errors: list[str] = []
    if not isinstance(data, dict):
        return ["submission root must be a JSON object"]
    required = {
        "schema_version", "submission_id", "participant_number",
        "session_index", "condition", "questionnaire_version", "responses",
    }
    if set(data) != required:
        errors.append("top-level fields do not match questionnaire schema v1")
    if data.get("schema_version") != 1 or isinstance(data.get("schema_version"), bool):
        errors.append("schema_version must be 1")
    match = FILE_RE.fullmatch(expected_name)
    if not match:
        return errors + ["Drive filename does not match Pxx-Sx.json"]
    participant = int(match.group("participant"))
    session = int(match.group("session"))
    if data.get("submission_id") != expected_name[:-5]:
        errors.append("submission_id does not match filename")
    number = data.get("participant_number")
    if not isinstance(number, int) or isinstance(number, bool) or number != participant:
        errors.append("participant_number does not match filename")
    if data.get("session_index") != session or isinstance(data.get("session_index"), bool):
        errors.append("session_index does not match filename")
    expected_condition = (
        ("A" if session == 1 else "B") if participant % 2
        else ("B" if session == 1 else "A")
    )
    if data.get("condition") != expected_condition:
        errors.append("condition does not match the participant counterbalancing assignment")
    version = data.get("questionnaire_version")
    if not isinstance(version, str) or not 1 <= len(version) <= 64:
        errors.append("questionnaire_version must be a string of 1–64 characters")
    responses = data.get("responses")
    if not isinstance(responses, dict) or set(responses) != {
        "condition_questionnaire", "comparison_questionnaire"
    }:
        errors.append("responses must contain condition_questionnaire and comparison_questionnaire only")
    else:
        errors.extend(questionnaire_errors(
            responses["condition_questionnaire"], "responses.condition_questionnaire"
        ))
        comparison = responses["comparison_questionnaire"]
        if session == 1 and comparison is not None:
            errors.append("session 1 comparison_questionnaire must be null")
        elif session == 2:
            if comparison is None:
                errors.append("session 2 comparison_questionnaire is required")
            else:
                errors.extend(questionnaire_errors(
                    comparison, "responses.comparison_questionnaire"
                ))
    return errors


def download_file(access_token: str, file_id: str) -> bytes:
    url = f"{DRIVE_API}/files/{urllib.parse.quote(file_id, safe='')}?alt=media"
    raw = http_request(url, headers={"Authorization": f"Bearer {access_token}"})
    if len(raw) > MAX_FILE_BYTES:
        raise RetrievalError(f"Drive file {file_id} exceeds the {MAX_FILE_BYTES}-byte safety limit.")
    return raw


def output_path_for(directory: Path, name: str) -> Path:
    # Submission IDs are deterministic, so repeated retrieval maps to the
    # same local path and is skipped unless the researcher opts into overwrite.
    return directory / name


def ensure_output_is_external(directory: Path) -> None:
    repo_root = Path(__file__).resolve().parents[1]
    resolved = directory.expanduser().resolve()
    if resolved == repo_root or repo_root in resolved.parents:
        raise RetrievalError(
            "Choose an output directory outside the repository; real participant data must not enter Git."
        )


def run(args: argparse.Namespace) -> int:
    config = require_environment()
    output_dir = args.out_dir.expanduser().resolve()
    ensure_output_is_external(output_dir)

    access_token = refresh_access_token(config)
    files = list_submission_files(access_token, config["GOOGLE_DRIVE_FOLDER_ID"])
    selected = [
        file for file in files
        if matching_submission(file, args.participant, args.session)
    ]
    selected.sort(key=lambda item: str(item.get("name", "")))

    if args.dry_run:
        print(f"Found {len(selected)} matching submission file(s).")
        for file in selected:
            print(f"  {file.get('name')}  {file.get('size', 'unknown size')} bytes")
        return 0

    output_dir.mkdir(parents=True, exist_ok=True)
    report: dict[str, Any] = {
        "retrieved_at": datetime.now(timezone.utc).isoformat(),
        "source": "Google Drive study submissions folder",
        "filters": {"participant": args.participant, "session": args.session},
        "files": [],
    }
    downloaded = skipped = invalid = 0

    for file in selected:
        name = str(file["name"])
        file_id = str(file.get("id", ""))
        if not file_id:
            report["files"].append({"name": name, "status": "skipped", "reason": "missing Drive file ID"})
            skipped += 1
            continue
        try:
            content = download_file(access_token, file_id)
        except RetrievalError as exc:
            report["files"].append({"name": name, "status": "failed", "reason": str(exc)})
            skipped += 1
            continue

        validation_errors: list[str]
        try:
            parsed = json.loads(content.decode("utf-8"))
            validation_errors = validate_submission(parsed, name)
        except (UnicodeDecodeError, json.JSONDecodeError):
            validation_errors = ["file is not valid UTF-8 JSON"]

        destination = output_path_for(output_dir, name)
        if destination.exists() and not args.overwrite:
            report["files"].append({
                "name": name, "local_name": destination.name,
                "status": "skipped", "reason": "local file already exists",
                "validation_errors": validation_errors,
            })
            skipped += 1
            continue

        destination.write_bytes(content)
        status = "downloaded" if not validation_errors else "downloaded_invalid"
        downloaded += 1
        if validation_errors:
            invalid += 1
        report["files"].append({
            "name": name,
            "local_name": destination.name,
            "drive_file_id": file_id,
            "size_bytes": len(content),
            "status": status,
            "validation_errors": validation_errors,
        })

    report_path = output_dir / "download_report.json"
    report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps({
        "matched": len(selected),
        "downloaded": downloaded,
        "invalid": invalid,
        "skipped_or_failed": skipped,
        "output_directory": str(output_dir),
        "report": str(report_path),
    }, indent=2))
    return 0 if skipped == 0 and invalid == 0 else 2


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUTPUT,
                        help=f"local output directory (default: {DEFAULT_OUTPUT})")
    parser.add_argument("--participant", type=int, help="download one participant number")
    parser.add_argument("--session", type=int, choices=(1, 2), help="download one session")
    parser.add_argument("--dry-run", action="store_true", help="list matching files without downloading")
    parser.add_argument("--overwrite", action="store_true",
                        help="overwrite existing matching local files")
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    if args.participant is not None and not 1 <= args.participant <= 9999:
        parser.error("--participant must be between 1 and 9999")
    try:
        return run(args)
    except RetrievalError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
