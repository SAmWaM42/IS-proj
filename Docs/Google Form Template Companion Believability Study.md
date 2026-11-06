# Google Form Template: Companion Believability Study

*Draft v0.1. Copy each block into Google Forms in the order shown. Text in brackets needs your details.*

## 1. How the form is used

The same form is opened twice per participant: once after each companion. The application opens it with three answers already filled in (session number, companion code, run id). The session number decides which sections the participant sees.

| Session | Path through the form |
| --- | --- |
| 1 (first companion) | Section 0 → Section 1 (consent) → Section 2 (about you) → Section 3a (rating) → Submit |
| 2 (second companion) | Section 0 → Section 3b (rating) → Section 4 (comparison) → Submit |

Build order in Google Forms: create Sections 0, 1, 2, 3a, 3b and 4 in that order. Section 3b is a duplicate of Section 3a (use the section's Duplicate option).

## 2. Form settings checklist

- **Collect email addresses:** Off (keeps responses anonymous).
- **Limit to 1 response:** Off (each participant submits twice).
- **Show progress bar:** On.
- **Shuffle row order** on both rating grids: On (hides the position of the attention check and reduces order effects).
- **Require a response in each row** on both rating grids: On.
- **Limit to one response per column:** Off (several rows may share an answer).
- **Confirmation message** (Settings, Presentation): see section 9.
- **Responses:** link to a Google Sheet that only you can open, and export a CSV after each session.

## 3. Section 0: Session details (all participants)

*Description shown to the participant:* Your researcher will normally have these filled in for you. Please do not change the first three answers. If any are empty, ask your researcher.

| # | Question | Type | Options or format | Required |
| --- | --- | --- | --- | --- |
| 0.1 | Session (filled in for you) | Multiple choice | 1; 2 | Yes |
| 0.2 | Companion you just used (filled in for you) | Multiple choice | A; B | Yes |
| 0.3 | Run ID (filled in for you) | Short answer | Free text | Yes |
| 0.4 | Participant code (given by your researcher, for example P07) | Short answer | Response validation: text, contains P | Yes |

**Branching on 0.1:** option 1 → Go to Section 1; option 2 → Go to Section 3b.

## 4. Section 1: Information and consent (session 1 only)

*Title:* About this study

*Description text (edit the bracketed parts):*

This study looks at how lifelike two virtual animal companions seem. You will interact with each companion for about ten minutes, answer questions about it after each one, and compare them at the end. Taking part is voluntary and you can stop at any time without giving a reason. We do not collect your name or email address. Your answers are stored under a participant code and used only for this research project at \[institution\]. Interaction logs from the application are stored separately and are linked to your answers only through the participant code. Results will be reported as group summaries. Contact: \[researcher name and email\]. Ethics reference: \[reference\].

| # | Question | Type | Options | Required |
| --- | --- | --- | --- | --- |
| 1.1 | I am 18 or older, I have read the information above, and I agree to take part. | Multiple choice | Yes, I agree; No, I do not agree | Yes |

**Branching on 1.1:** Yes, I agree → Continue to Section 2; No, I do not agree → Submit form.

## 5. Section 2: About you (session 1 only)

*Section 2 ends with: After this section, go to Section 3a.*

| # | Question | Type | Options or scale | Required |
| --- | --- | --- | --- | --- |
| 2.1 | What is your age range? | Multiple choice | 18 to 24; 25 to 34; 35 to 44; 45 or older; Prefer not to say | Yes |
| 2.2 | Have you owned or regularly cared for a pet? | Multiple choice | Currently; In the past; Never | Yes |
| 2.3 | How familiar are you with virtual pets or pet-style games? | Linear scale 1 to 5 | 1 = Not at all familiar; 5 = Very familiar | Yes |
| 2.4 | How comfortable are you using a computer with a mouse and keyboard? | Linear scale 1 to 5 | 1 = Not at all comfortable; 5 = Very comfortable | Yes |

## 6. Section 3a and 3b: Rating the companion

*Section 3a ends with: After this section, Submit form. Section 3b ends with: After this section, go to Section 4.*

*Title:* Rate the companion you just used

*Description:* Think about the companion you interacted with just now (the one shown in question 0.2). For each statement, choose how much you agree. There are no right or wrong answers. Some statements are the opposite of others on purpose.

### Grid 1 (multiple-choice grid, 10 rows)

**Columns (same for both grids):** Strongly disagree; Disagree; Neutral; Agree; Strongly agree.

| Row | Statement |
| --- | --- |
| 1 | The companion's personality felt stable throughout the session. |
| 2 | The companion reacted in similar ways to similar things I did. |
| 3 | The companion's behaviour seemed random or contradictory. |
| 4 | The companion's behaviour changed in sensible ways depending on how I treated it. |
| 5 | The companion seemed to remember how I had treated it earlier. |
| 6 | The companion responded the same way no matter what had happened before. |
| 7 | When I told the companion something about my preferences, its later behaviour seemed to reflect it. |
| 8 | The companion did things on its own, not only in response to my buttons. |
| 9 | The companion's behaviour was varied and did not repeat the same responses. |
| 10 | The companion felt scripted, like a machine running through commands. |

### Grid 2 (multiple-choice grid, 11 rows)

| Row | Statement |
| --- | --- |
| 11 | I could tell what kind of mood the companion was in from how it behaved. |
| 12 | The companion's reactions suited the situation. |
| 13 | The companion's reactions seemed flat or emotionless. |
| 14 | When the companion did not do what I asked, its behaviour still made sense. |
| 15 | I wanted to keep interacting with the companion. |
| 16 | I kept paying attention to what the companion was doing. |
| 17 | I became bored during the session. |
| 18 | The companion seemed like a believable animal. |
| 19 | I could imagine this companion having its own personality. |
| 20 | Overall, the companion behaved in a lifelike way. |
| 21 | To show you are reading carefully, please select Agree for this row. |

### Other questions in the section

| # | Question | Type | Format | Required |
| --- | --- | --- | --- | --- |
| 3.1 | Overall, how believable was this companion as an animal? | Linear scale 1 to 5 | 1 = Not at all believable; 5 = Extremely believable | Yes |
| 3.2 | What made this companion feel most like a real animal? | Paragraph | Free text | No |
| 3.3 | Was there a moment that made it feel less like a real animal? What happened? | Paragraph | Free text | No |
| 3.4 | Did anything not work properly during this session (for example a freeze or a delay)? | Multiple choice | No; Yes | Yes |
| 3.5 | If yes, what happened? | Short answer | Free text | No |

## 7. Section 4: Comparing the two companions (session 2 only)

*Title:* Comparing Companion A and Companion B

*Description:* You have now used both companions. Answer these questions about the two of them together.

| # | Question | Type | Options or scale | Required |
| --- | --- | --- | --- | --- |
| 4.1 | Which companion behaved more consistently? | Multiple choice | Companion A; Companion B; No noticeable difference | Yes |
| 4.2 | Which companion adapted better to how you treated it? | Multiple choice | Companion A; Companion B; No noticeable difference | Yes |
| 4.3 | Which companion was more engaging to interact with? | Multiple choice | Companion A; Companion B; No noticeable difference | Yes |
| 4.4 | Which companion was more believable as an animal overall? | Multiple choice | Companion A; Companion B; No noticeable difference | Yes |
| 4.5 | How confident are you in your answer to 4.4? | Linear scale 1 to 5 | 1 = I was guessing; 5 = Very sure | Yes |
| 4.6 | Did you think the two companions were built differently? | Multiple choice | Yes; No; Not sure | Yes |
| 4.7 | If yes or not sure, what did you think was different? | Paragraph | Free text | No |
| 4.8 | Anything else you would like to tell us? | Paragraph | Free text | No |

Question 4.6 is a blinding check. Do not rename it or add hints about memory or mood.

## 8. Scoring key (for your analysis sheet, not shown to participants)

Score 1 to 5 from Strongly disagree to Strongly agree. Items marked Reverse are scored as 6 minus the response. Row numbers are the row numbers above.

| Subscale | Rows | Reverse |
| --- | --- | --- |
| Behavioural consistency | 1, 2, 3 | 3 |
| Adaptability | 4, 5, 6 | 6 |
| Statement response (exploratory) | 7 | none |
| Autonomy and variation | 8, 9, 10 | 10 |
| Emotional expression | 11, 12, 13 | 13 |
| Contextual appropriateness (exploratory) | 14 | none |
| Engagement | 15, 16, 17 | 17 |
| Overall believability | 18, 19, 20 | none |
| Attention check | 21 | Correct answer is Agree (4); exclude the companion rating otherwise |
| Summary item | 3.1 | none |

Headline measures from the project documentation: Behavioural consistency, Adaptability, Engagement, Overall believability. Compute each subscale as the mean of its rows after reverse scoring. Rows 7 and 14 are single items and are reported separately.

## 9. Text to paste

**Confirmation message:** Thank you. If this was your first companion, please return to the application and choose the other companion. If it was your second, you are finished. Your researcher will explain more about the study.

**Debrief script (spoken by the researcher after the final form):** One of the two companions learned from your earlier actions and had a simple mood that changed over time; the other responded only to what was happening at that moment. We did not tell you this beforehand so that your ratings would reflect what you saw. Thank you for taking part. You can ask for your data to be removed at any time using the participant code on your card.

## 10. Linking the form to the application

1. In Google Forms, open the form preview and choose the three-dot menu, then Get pre-filled link.
2. Fill 0.1 with 1, 0.2 with A and 0.3 with test, then choose Get link and copy it.
3. The link contains parameters like entry.123456789=1. Note the three entry numbers (session, companion, run id). Multiple-choice answers must match the option text exactly.
4. Give the application four settings: the base form link (everything before the first entry parameter, ending in ?usp=pp\_url) and the three entry numbers. The application then appends the participant's session, companion and run id.
5. Test with a dry run: open the link from the application and confirm the three fields arrive filled in and that the response lands in the sheet.

## 11. Before you publish

- Supervisor and ethics approval of consent text and questions. {{to fill}}
- Run through both paths (session 1 and session 2) in preview.
- Confirm the attention-check row and both reverse-worded rows are present in each grid copy.
- Check that the responses sheet is private and that email collection is off.
