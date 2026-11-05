# Local Google Drive Study Data Retrieval

**Status:** implementation in progress on `Dev`  
**Scope:** researcher-side retrieval of questionnaire submissions only.

## Purpose

Download the JSON questionnaire submissions stored by the Animal Agent collection backend into a private local directory so the existing Python analysis scripts can consume them. This is a separate researcher utility; it is not part of the participant executable and does not call the participant submission endpoint.

## Architecture

- The researcher runs `scripts/download_study_submissions.py` locally.
- The script authenticates directly to Google OAuth using a researcher-controlled refresh token.
- It lists JSON files in the configured Google Drive submissions folder and downloads matching files.
- Each file is preserved as JSON. The downloader does not transform questionnaire answers or upload anything.
- Downloaded files and the retrieval report are stored outside the Git repository by default.

The downloader does not use participant bearer tokens, the HMAC seed, or the public Vercel submission endpoint.

## Configuration

Set these environment variables in the local shell:

- `GOOGLE_CLIENT_ID`
- `GOOGLE_CLIENT_SECRET`
- `GOOGLE_REFRESH_TOKEN`
- `GOOGLE_DRIVE_FOLDER_ID`

Use the OAuth client and Drive folder associated with the collection backend. The refresh token must have read access to the destination folder and its submission files. If the existing token was issued with a scope that cannot list/read those files, create a researcher refresh token with the least-privilege suitable read scope (normally `https://www.googleapis.com/auth/drive.readonly`). Do not put credentials in source control, command-line arguments, or the common participant package.

## Usage

Install no additional Python package; the downloader uses the Python standard library.

List matching files without downloading:

```bash
python3 scripts/download_study_submissions.py --dry-run
```

Download all submissions:

```bash
python3 scripts/download_study_submissions.py
```

Download one participant or one session:

```bash
python3 scripts/download_study_submissions.py --participant 20
python3 scripts/download_study_submissions.py --session 2
python3 scripts/download_study_submissions.py --participant 20 --session 1
```

Choose a private output directory:

```bash
python3 scripts/download_study_submissions.py --out-dir "$HOME/AnimalAgentStudyData/submissions"
```

Existing local files are not overwritten by default. Use `--overwrite` only when intentionally replacing local copies. Drive files are never modified or deleted.

## Output and validation

The default output directory is `~/AnimalAgentStudyData/submissions`. The script writes:

- One original `Pxx-Sx.json` file per downloaded Drive submission.
- `download_report.json`, containing retrieval time, filters, Drive file IDs, file sizes, statuses, and validation errors.

The downloader checks the v1 envelope, deterministic filename/submission ID, participant number, session, odd/even A/B assignment, questionnaire version, typed answer values, and the session-specific comparison-questionnaire rule. Invalid JSON is still preserved locally for inspection and clearly marked invalid in the report; the script returns a non-zero status if any file is invalid or any download is skipped/failed.

The downloader does not claim that a submission is a valid research response merely because it matches the structural schema. Research eligibility, consent, attention checks, completeness, duplicate participant sessions, and analysis exclusions remain the responsibility of the analysis pipeline and approved protocol.

## Data protection

- Keep the output directory private and outside the repository.
- Do not email or commit downloaded participant responses.
- Restrict access to the research team and follow the approved retention/withdrawal procedure.
- Do not add raw interaction text, memory contents, affect traces, Q-table data, or diagnostic logs to the download scope without a separately approved protocol change.
- The downloader is read-only with respect to Google Drive.

## Acceptance criteria

- [ ] Researcher OAuth can list the configured submissions folder.
- [ ] Dry-run lists the expected `Pxx-S1.json` and `Pxx-S2.json` files without downloading them.
- [ ] Full retrieval downloads files and preserves their JSON content.
- [ ] Participant/session filters select only matching files.
- [ ] Re-running without `--overwrite` preserves existing local copies.
- [ ] `--overwrite` explicitly replaces local copies.
- [ ] Malformed JSON and schema-invalid submissions are reported without silently being treated as valid.
- [ ] Output and credentials remain outside Git.
- [ ] The existing analysis scripts can be pointed at the downloaded dataset after their input adapter is defined.

## Known boundary

The current analysis script `scripts/analyze_companion_believability_study.py` expects a Google Forms CSV plus application JSONL logs. The new Drive downloader retrieves the questionnaire submission JSON envelope only. An adapter from this new envelope into the analysis script's expected input model is a separate task; the downloader must not fabricate missing manifests or interaction logs.
