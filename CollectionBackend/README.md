# Animal Agent collection backend

This directory is a separate Vercel Node.js function project. It receives questionnaire-response JSON from the participant client and stores each submission as a JSON file in a designated Google Drive folder.

## Deployment

1. Import this repository into Vercel and set the project's **Root Directory** to `CollectionBackend`.
2. Configure a Google Cloud project, enable the Google Drive API, create an OAuth client, and authorise the Google account that owns the destination Drive folder.
3. Create a refresh token for that OAuth client with Drive file access, and create a Drive folder for study responses.
4. Add the environment variables below in Vercel Project Settings, then deploy/redeploy.

Required environment variables:

- `GOOGLE_CLIENT_ID`
- `GOOGLE_CLIENT_SECRET`
- `GOOGLE_REFRESH_TOKEN`
- `GOOGLE_DRIVE_FOLDER_ID`
- `PARTICIPANT_TOKENS`: JSON object mapping participant numbers to unique random bearer tokens, for example `{"3":"<random-token>","4":"<random-token>"}`.

Generate a different high-entropy token for each participant and distribute it privately with the participant assignment. Never commit tokens, OAuth credentials, refresh tokens, or Drive identifiers to Git. The desktop app must not contain Google OAuth credentials.

## Request contract

`POST /api/submissions`

Headers:

- `Authorization: Bearer <participant-token>`
- `Content-Type: application/json`
- `Idempotency-Key: P03-S1`

Body: schema version 1, deterministic `submission_id` (`P{participant}-S{session}`), `participant_number`, `session_index` (1 or 2), `condition` (A or B), and questionnaire response fields.

Successful response:

```json
{"stored":true,"submission_id":"P03-S1"}
```

The function validates the identifier and assigned participant token, then checks Drive for an existing file with that submission ID before creating a file. This handles normal client retries after an acknowledgement is lost. The Drive lookup/create sequence is not a transactional uniqueness guarantee under simultaneous duplicate requests; a database with a unique constraint would be needed for that stronger guarantee.

## Data handling

Only questionnaire response JSON should be sent. Do not add raw interaction text, memory records, affect traces, Q-table data, or unrelated diagnostic logs. Restrict the Drive folder to the research team and establish a retention/withdrawal procedure before collecting data. Avoid logging request bodies or bearer tokens in Vercel.
