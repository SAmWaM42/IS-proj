# Believability Testing Pipeline: Specification

*Version 0.1, draft for supervisor review. Study of a baseline companion agent versus a proposed agent with memory and affect.*

## 1. Purpose and scope

The pipeline lets participants interact with each of the two companions through the same guided script, optionally keep playing freely, and then rate that companion on a Google Form. It produces two linked data sets: questionnaire responses and interaction logs.

**In scope:** participant flow, the guided interaction script, UI changes needed to run it, the form link and data linking, measures, analysis plan, pilot, risks and readiness criteria.

**Out of scope:** retraining the policies, new art assets, and the wording of the form itself (see the separate Google Form template).

**What the study can and cannot claim.** The synthetic training reward is not a measure of believability. Only the participant ratings address the research question. Behavioural log metrics are supporting evidence.

## 2. Study design

| Item | Decision |
| --- | --- |
| Design | Within-subjects. Every participant meets both companions, labelled Companion A and Companion B. |
| Independent variable | Agent: baseline (observation only) or proposed (memory + affect). |
| Counterbalancing | The A/B mapping and the order alternate strictly between participants (participant 1: A is baseline, participant 2: A is proposed, and so on). |
| Blinding | Participants are not told what differs. The UI hides action labels, memory, affect and statistics. The researcher stays out of the way during sessions. |
| Held constant | Scene, assets, animation mapping, guided script, step order, ball-throw distances, session length, policies (learning off, exploration off). |
| Covariates | Prior pet ownership, familiarity with virtual pets or games. |

**Hypotheses (to confirm with the supervisor before data collection):**

- **H1:** the proposed companion receives higher Adaptability ratings than the baseline.
- **H2:** the proposed companion receives Overall believability ratings at least as high as the baseline (a higher rating is possible but not assumed).
- **H3 (exploratory):** after a stated preference, the proposed companion's choices change in the later probe, while the baseline's do not.

A null result is a valid finding and will be reported as inconclusive rather than as no effect, given the sample size.

## 3. Participants and ethics

- **Sample:** pilot of 3 to 5 people, then about 24 participants for the main study. About 24 gives roughly 80% power for a medium-to-large paired effect (dz about 0.6); verify with a power calculation before recruiting. Smaller samples limit conclusions to large effects.
- **Inclusion:** adults (18 or over), comfortable using a mouse and keyboard. No prior pet experience is required.
- **Consent and approval:** informed consent on the form. {{Ethics or departmental approval reference and supervisor sign-off}}.
- **Anonymity:** no names or email addresses are collected. Each participant gets a researcher-assigned code (P01, P02, ...). The code is written only on the researcher's log sheet.
- **Data handling:** interaction logs and form exports are stored outside the Git repository, in a restricted folder. The Google Form must not collect email addresses. Withdrawal: a participant may stop at any time and their data is removed on request.
- **Debrief:** after the final form, the researcher explains that one companion had memory and affect and the other did not.

## 4. Session protocol

**Researcher setup (before each participant):** launch the application with the new-participant option (section 6, R7), which clears the proposed agent's memory and applies the next counterbalanced mapping; confirm the form link is prefilled; start screen capture if permitted.

| Phase | Minutes | What happens |
| --- | --- | --- |
| 1. Briefing and consent | 5 | Information sheet, consent (first form session only). |
| 2. Warm-up | 1 | Participant moves with WASD and reads the control hints. Not logged as a rated step. |
| 3. Companion 1: guided script | 6 to 8 | 12 prompted steps (table below). |
| 4. Companion 1: free interaction | 0 to 3 | Optional. A Finish button ends it. |
| 5. Form, part 1 | 4 | Rating of Companion 1 and background questions. |
| 6. Companion 2: guided script, free interaction | 6 to 11 | Same script, same order. |
| 7. Form, part 2 | 6 | Rating of Companion 2 and the direct comparison. |
| 8. Debrief | 2 | Researcher explains the design and answers questions. |

Total: about 35 to 40 minutes.

### 4.1 Guided script

The same 12 steps, in the same order, are shown for both companions. Prompts appear in an on-screen task panel and a step completes when the matching interaction has been issued. The script is designed to elicit comparable situations, and two memory probes are built in.

| Step | Prompt shown to the participant | Interaction | What it probes |
| --- | --- | --- | --- |
| 1 | Walk close to the companion and press Greet. | Greet | Proximity-sensitive greeting |
| 2 | Press Praise. | Praise | Response to positive feedback |
| 3 | Press Throw ball and watch. | Throw ball (fixed landing 1) | Baseline fetch behaviour (probe A, before any stated preference) |
| 4 | Press Feed. | Feed | Need-driven behaviour (eating) |
| 5 | Press Loud noise. | Loud noise | Stress and protective response |
| 6 | Type: I like the ball. Press Enter. | Typed statement | Memory write (preference) |
| 7 | Press Throw ball again. | Throw ball (same fixed landing) | Memory probe A: compare with step 3 |
| 8 | Press Scold. | Scold | Response to negative feedback |
| 9 | Stay still for 30 seconds and watch. | None | Autonomous behaviour |
| 10 | Type: I hate loud noises. Press Enter. | Typed statement | Memory write (dislike) |
| 11 | Press Loud noise again. | Loud noise | Memory probe B: compare with step 5 |
| 12 | Press Rest. | Rest | Rest behaviour |

**Rules:** the participant may not skip a step except through the researcher. The script text never mentions what the companion should do. The free interaction phase begins after step 12 and is flagged separately in the logs.

## 5. Measures

**Questionnaire (Google Form):** five subscales, each with three items, one reverse-scored: Behavioural consistency, Adaptability, Autonomy and variation, Emotional expression, Engagement, plus Overall believability, a summary item, contextual appropriateness, a statement-response item, an attention check, a direct comparison (D1 to D5) and a blinding check (D6). Items are in the form template.

**Behavioural metrics (from logs, computed per participant and companion):**

| Metric | Definition |
| --- | --- |
| Action variety | Number of distinct actions chosen and their entropy across the guided steps |
| Compliance rate | Share of steps in which the action matches the requested one |
| Justified deviation | Share of non-compliant actions that a plausibility rule marks as contextually sensible |
| Memory probe shift | Change in the chosen action between step 3 and step 7, and between step 5 and step 11 |
| Response latency | Time from interaction to the start of the visible action |
| Idle behaviour | Actions and counts during step 9 and the free phase |
| Free-interaction use | Whether the participant used it and for how long |

These metrics are descriptive. They are not used to claim believability.

## 6. System requirements

Priority: P0 is needed to run the study, P1 is strongly recommended, P2 is optional.

| ID | Requirement | Priority | Status |
| --- | --- | --- | --- |
| R1 | Guided-task panel: step text, progress (Step n of 12) and automatic advance when the matching interaction is issued; researcher override key | P0 | Implemented in source; local verification pending |
| R2 | Session phases (warm-up, guided, free, finished) stored in the state and written to every log row | P0 | Implemented in source; local verification pending |
| R3 | Log schema additions: participant\_code, session\_index, condition\_code, phase, step\_id, build version (Git commit), policy checksums | P0 | Implemented across manifests and JSONL logs; local verification pending |
| R4 | End of guided script: free-interaction screen with a Finish button and an optional 3-minute timer; then a clear Answer Form prompt | P0 | Implemented in source; local verification pending |
| R5 | Prefilled form link carrying condition code, run id and session number as Google Forms entry parameters (the form ignores plain names such as condition=A) | P0 | Implemented; requires configured form URL and entry IDs |
| R6 | Change Model is available only after the Answer Form button has been used for the current companion | P1 | Implemented in source; local verification pending |
| R7 | New-participant launch option: clears proposed memory, increments a participant counter stored in a small state file and sets the A/B mapping from its parity | P0 | Partially implemented: each guided-study launch increments the persistent counter and clears in-memory memory/affect; no separate reset-counter UI |
| R8 | Fixed ball-landing positions per scripted step so both companions see identical throws | P0 | Implemented for the two guided throw probes; local verification pending |
| R9 | Autonomous idle decisions suppressed during steps 1 to 8 and 10 to 12, active during step 9 and the free phase | P1 | Implemented in source; local verification pending |
| R10 | Scripted replay check: feed the 12 step observations to both agents many times and report the action distribution and any unfamiliar-state warnings before participants | P1 | Partial: action distributions and unfamiliar-state counts are logged/analyzed during actual runs; pre-participant repeated replay remains unimplemented |
| R11 | Policies loaded read-only, learning and exploration off, checksums logged at start | P0 | Implemented in source; local verification pending |
| R12 | Study mode hides action labels, state, statistics, interpretation method and the event log; no mapping is visible | P0 | Retained in guided-study mode |
| R13 | Export a participant data bundle (manifest, interaction log, step log) to a restricted folder outside the repository | P1 | Partial: manifest, interaction, step and session files are written outside the repo by default; no archive bundle generated |

**Form link construction (R5).** Configure three environment variables: the base form link and the entry ids of the three prefilled questions. The application appends the entry parameters to the base link. Entry ids are obtained from the form's Get pre-filled link option.

**Minimum viable pipeline** if time is short: R5, R7, R8, R11 and R12, with the guided script on a printed card and the researcher noting the step order on a log sheet. The two memory probes then rely on the researcher following the card exactly.

## 7. Data and linking

| Data | Source | Link key |
| --- | --- | --- |
| Questionnaire responses | Google Form export (CSV) | run id, condition code, session number (prefilled) |
| Run manifest | Application | run id |
| Interaction and step log (JSON Lines) | Application | run id |
| Researcher log sheet | Paper or spreadsheet | participant code, run id |

The run id and condition code are prefilled into the form so that responses join to logs without the participant typing them. The mapping from code to baseline or proposed lives only in the manifest and is hidden from the participant.

## 8. Analysis plan

1. **Screening:** exclude a companion rating if the attention check fails, the form is incomplete, or the application crashed during the guided script. Record every exclusion.
2. **Scoring:** reverse-score the marked items, average each subscale, and check internal consistency (Cronbach's alpha, about 0.70 or higher).
3. **Primary comparison:** paired Wilcoxon signed-rank test for Adaptability (H1) and Overall believability (H2). Report the effect size and a confidence interval.
4. **Secondary:** the other subscales with a Holm correction across subscales.
5. **Order and covariates:** compare first and second companion ratings, and check prior pet experience and game familiarity.
6. **Direct comparison:** report D1 to D5 as counts; analyse D6 to describe how well the blinding held.
7. **Behavioural metrics and probes:** descriptive tables and per-participant plots. For H3, compare the step 3 versus 7 and step 5 versus 11 actions within each agent.
8. **Qualitative:** code the open comments into themes (what felt lifelike, what broke the illusion).

## 9. Pilot and readiness

**Pilot (3 to 5 people):** timing per phase, whether prompts are understood, form length and wording, crashes, and whether the memory probes show any difference. Make changes, then freeze the build with a Git tag and record policy checksums.

**Go criteria for the main study:**

- 10 consecutive full dry runs complete for both companions with no crash.
- Log rows contain every field in R3 for every step.
- The prefilled link opens the right form fields and the responses join to logs.
- The scripted replay check shows no obviously implausible action in the 12 steps for either agent. {{Define the plausibility rule used}}.
- Supervisor approval of the script, the form and the consent text.

## 10. Risks and mitigations

| Risk | Mitigation |
| --- | --- |
| Memory has no visible effect in one short session | The scripted preference statements and repeated probes (steps 6 and 7, 10 and 11); report as exploratory if no shift appears |
| Untrained states produce odd actions | Scripted replay check (R10); keep proximity in the trained range; fix or document before the study |
| Order effects | Strict counterbalancing and an order analysis |
| Demand characteristics | No mention of memory or mood in the briefing; neutral task wording |
| Visual limits of the 2D animation | Both agents share the renderer; report as a limitation |
| Small sample | Report effect sizes with intervals; frame findings as preliminary |
| Uncontrolled movement | Guided script keeps participants near the companion; fixed ball positions |
| Researcher influence | Researcher reads a fixed script and does not comment on the companion's behaviour |
| Data protection | No email collection, pseudonymous codes, logs kept outside the repository |

## 11. Researcher checklist

**Before:** build tag matches the frozen version, policies trained with the current state encoding, form link tested, consent sheets ready, participant code assigned.

**During:** read the script verbatim, do not explain what the companion is doing, note crashes and interruptions with the step number.

**After:** export the data bundle, check that the form response and log share a run id, record the participant code and any deviations.

## 12. Open decisions

- Ethics approval route and reference. {{to fill}}
- Final sample size and recruitment method. {{to fill}}
- Whether the free interaction is timed or open-ended.
- Plausibility rule used for justified deviation.
- Whether to add a memory-only or affect-only condition (needs more participants).
