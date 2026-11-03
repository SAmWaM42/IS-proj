import type { VercelRequest, VercelResponse } from "@vercel/node";
import { google } from "googleapis";
import { timingSafeEqual } from "node:crypto";

type Submission = {
  schema_version: number;
  submission_id: string;
  participant_number: number;
  session_index: number;
  condition: "A" | "B";
  [key: string]: unknown;
};

function errorResponse(res: VercelResponse, status: number, message: string) {
  return res.status(status).json({ stored: false, error: message });
}

function isSubmission(value: unknown): value is Submission {
  if (!value || typeof value !== "object" || Array.isArray(value)) return false;
  const item = value as Record<string, unknown>;
  if (item.schema_version !== 1 ||
      !Number.isInteger(item.participant_number) ||
      Number(item.participant_number) < 1 || Number(item.participant_number) > 9999 ||
      ![1, 2].includes(Number(item.session_index)) ||
      !["A", "B"].includes(String(item.condition)) ||
      typeof item.submission_id !== "string") return false;
  const expected = `P${String(item.participant_number).padStart(2, "0")}-S${item.session_index}`;
  return item.submission_id === expected;
}

function authenticate(participantNumber: number, supplied: string): boolean {
  const configured = process.env.PARTICIPANT_TOKENS;
  if (!configured) return false;
  let tokens: Record<string, string>;
  try {
    tokens = JSON.parse(configured) as Record<string, string>;
  } catch {
    return false;
  }
  const expected = tokens[String(participantNumber)];
  if (typeof expected !== "string" || !expected || !supplied) return false;
  const expectedBytes = Buffer.from(expected);
  const suppliedBytes = Buffer.from(supplied);
  return expectedBytes.length === suppliedBytes.length &&
    timingSafeEqual(expectedBytes, suppliedBytes);
}

export default async function handler(req: VercelRequest, res: VercelResponse) {
  res.setHeader("Cache-Control", "no-store");
  if (req.method !== "POST") {
    res.setHeader("Allow", "POST");
    return errorResponse(res, 405, "Method not allowed.");
  }

  if (!isSubmission(req.body)) {
    return errorResponse(res, 400, "Invalid submission schema.");
  }

  const bearer = /^Bearer (.+)$/i.exec(req.headers.authorization ?? "");
  if (!bearer || !authenticate(req.body.participant_number, bearer[1])) {
    return errorResponse(res, 401, "Participant authentication failed.");
  }

  const { GOOGLE_CLIENT_ID, GOOGLE_CLIENT_SECRET, GOOGLE_REFRESH_TOKEN, GOOGLE_DRIVE_FOLDER_ID } = process.env;
  if (!GOOGLE_CLIENT_ID || !GOOGLE_CLIENT_SECRET || !GOOGLE_REFRESH_TOKEN || !GOOGLE_DRIVE_FOLDER_ID) {
    return errorResponse(res, 503, "Collection storage is not configured.");
  }

  try {
    const oauth = new google.auth.OAuth2(GOOGLE_CLIENT_ID, GOOGLE_CLIENT_SECRET);
    oauth.setCredentials({ refresh_token: GOOGLE_REFRESH_TOKEN });
    const drive = google.drive({ version: "v3", auth: oauth });
    const id = req.body.submission_id;

    // A retry after a lost acknowledgement finds the already-stored Drive file.
    const existing = await drive.files.list({
      q: `'${GOOGLE_DRIVE_FOLDER_ID}' in parents and trashed = false and appProperties has { key='submission_id' and value='${id}' }`,
      spaces: "drive",
      fields: "files(id)",
      pageSize: 1
    });
    if ((existing.data.files ?? []).length > 0) {
      return res.status(200).json({ stored: true, submission_id: id });
    }

    await drive.files.create({
      requestBody: {
        name: `${id}.json`,
        mimeType: "application/json",
        parents: [GOOGLE_DRIVE_FOLDER_ID],
        appProperties: { submission_id: id }
      },
      media: {
        mimeType: "application/json",
        body: JSON.stringify(req.body, null, 2)
      },
      fields: "id"
    });

    return res.status(200).json({ stored: true, submission_id: id });
  } catch (error) {
    console.error("Drive submission storage failed", error);
    return errorResponse(res, 502, "Submission could not be stored.");
  }
}
