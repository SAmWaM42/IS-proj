# Windows Participant Build — Manual Validation Record

**Validation date:** 12 November 2026  
**Branch:** `deployment/participant-client` (Windows build changes delivered through PR #29)  
**Platform:** Windows desktop  
**Validation type:** Manual end-to-end test of the packaged participant application

## Test environment

The Windows package was downloaded from the GitHub Actions build artifact and extracted on a Windows computer that did not have the project's development toolchain or application-specific build dependencies installed. The application was launched from the packaged runtime rather than from a development build.

## Validation performed

The following participant workflow checks were completed:

| Area | Result | Evidence recorded |
|---|---|---|
| Application launch and participant login | Passed | Packaged application launched and accepted the assigned participant number |
| Condition assignment | Passed | Both questionnaire conditions (A and B) were tested and the corresponding questionnaire was displayed |
| Session handling | Passed | Session 1 and session 2 were tested and generated distinct submission IDs |
| Submission persistence | Passed | Submissions were accepted and the corresponding JSON data was present in the configured Google Drive destination |
| Answer integrity | Passed | The persisted JSON contained the latest answers entered during the test |
| Standalone Windows runtime | Passed | The packaged application completed the workflow without requiring the project's development environment or separately installed project dependencies |

## Result

The packaged Windows participant application completed the tested participant workflow successfully on the Windows test computer. The result provides manual evidence beyond the automated Windows CI build: it verifies application launch, participant entry, both questionnaire conditions, distinct session submissions, and server-side persistence of the latest submitted answers.

The automated workflow separately validates compilation, C++/NLP tests, standalone Python and spaCy provisioning, and package creation.

## Scope and limitations

- This record documents the manual test described above; it is not a claim of compatibility with every Windows version or hardware configuration.
- The successful submission confirms the tested records were persisted. It does not replace ongoing monitoring of the collection service during the participant study.
- Participant credentials are supplied separately and are not included in the common Windows package.
- The test was performed using the downloaded package, not by compiling the application on the test computer.
