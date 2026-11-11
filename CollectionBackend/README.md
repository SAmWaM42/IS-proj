# Collection Backend Source Snapshot

This directory contains a Vercel/TypeScript collection-backend source snapshot within the `IS-proj` development repository. It is retained alongside researcher-side work for reference and development history.

**Canonical web service repository:** [`SAmWaM42/IS-proj-web`](https://github.com/SAmWaM42/IS-proj-web). The separately maintained web repository owns the current API and release-download redirect README, deployment configuration and service integration. Use that repository as the operational source of truth for current web deployment. Do not assume this directory and the web repository are automatically synchronized.

The C++ participant client is maintained on [`deployment/participant-client`](https://github.com/SAmWaM42/IS-proj/tree/deployment/participant-client). Researcher-side agent development, training, evaluation and analysis remain on [`Dev`](https://github.com/SAmWaM42/IS-proj/tree/Dev). The root `main` README is the project-wide system map.

## Backend responsibility

The service receives questionnaire-response JSON from the participant client and stores submissions in a designated Google Drive folder using Google OAuth. It does not host the simulation, train the agent, run NLP, or perform questionnaire analysis.

### Submission contract

`POST /api/submissions`

Headers:
- `Authorization: Bearer <participant-token>`
- `Content-Type: application/json`
- `Idempotency-Key: P03-S1`

The version-1 envelope includes deterministic `submission_id` (`P{participant}-S{session}`), `participant_number`, `session_index` (1 or 2), `condition` (A or B), and questionnaire response fields.

The current canonical web API validates the envelope metadata, deterministic ID, odd/even A/B condition mapping, idempotency header and HMAC-backed participant token before writing questionnaire JSON to Drive. It does not yet perform complete validation of each active question ID, answer type, required field, allowed option, scale bound or questionnaire version. Drive lookup-before-create handles ordinary retries but is not atomic under concurrent duplicates.

## Security and data boundary

- Only questionnaire response JSON should be submitted.
- Do not send raw interaction text, memory records, affect traces, Q-table data or researcher diagnostics.
- Google OAuth credentials, refresh tokens, Drive folder IDs, HMAC seed and participant-token digest map are server-only configuration.
- Never commit secrets or real participant tokens.
- Keep the Drive folder restricted to the research team and establish retention/withdrawal procedures before real data collection.

## Deployment guidance

For current deployment instructions, required Vercel environment variables, token generation, test procedures and download redirect behaviour, use [the canonical web repository README](https://github.com/SAmWaM42/IS-proj-web/blob/main/README.md). Do not deploy this source snapshot or change production settings without explicit approval.
