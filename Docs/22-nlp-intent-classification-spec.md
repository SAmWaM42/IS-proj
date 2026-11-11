# NLP Intent Classification Integration

**Status:** Implemented on \`Dev\`; local build and runtime verification pending  
**Scope:** Python spaCy intent adapter and the C++ interaction-to-memory handoff  
**Purpose:** Improve classification of user statements, especially preferences, without changing the Q-learning policy or inventing actions from ambiguous text.

## 1. Existing integration

The SFML companion UI communicates with a persistent Python process through newline-delimited JSON. C++ sends one object containing \`text\`; the Python process returns one JSON object. The service writes diagnostics to stderr and reserves stdout for protocol responses.

The NLP process is an interpretation adapter only. It does not directly modify memory, affect, the environment, or the Q-table. C++ remains responsible for deciding how a classified utterance affects the simulation.

## 2. Supported intent classes

| Intent | Goal | Proximity | Task status | Signal | Reward |
|---|---:|---:|---:|---:|---:|
| greeting | 0 | 1 | 0 | 0.55 | 0.65 |
| request | 1 | 1 | 1 | 0.85 | 0.65 |
| disturbance | 2 | 0 | 0 | 0.95 | 0.00 |
| rest | 2 | 2 | 0 | 0.20 | 0.25 |
| negative_feedback | 0 | 1 | 2 | 0.45 | -1.00 |
| praise | 0 | 0 | 1 | 0.75 | 1.00 |
| preference | 0 | 0 | 0 | 0.50 | 0.60 |
| unknown | 0 | 1 | 0 | 0.45 | 0.25 |

The tuple maps to goal state, target proximity, task status, signal strength, and reward. User response status remains derived by the C++ interaction layer from the classified intent; it is not a separate NLP tuple field.

## 3. Classification precedence

The service uses the following precedence to prevent a positive keyword from masking a negative statement:

1. **Negative feedback:** stop, annoyance, bad/wrong, hate/dislike, and explicit negation such as “don't”, “do not”, “doesn't”, “did not”, “not like”, and “not love”.
2. **Praise:** common praise phrases (good boy/girl/dog/job, well done) and praise terms.
3. **Preference:** explicit preference/favourite terms, or like/love verbs with a grammatical subject. Imperative phrases such as “like this” must not be treated as a first-person preference.
4. **Request:** fetch/bring/get/retrieve/ball/toy/find vocabulary and subject-less imperative verb constructions.
5. **Context:** disturbance, rest, and greeting vocabulary.
6. **Unknown:** no supported intent confidently matches.

The spaCy path uses token lemmas, dependency subjects, part-of-speech tags and phrase checks. If spaCy or \`en_core_web_sm\` cannot be loaded, the service uses a deterministic keyword fallback with the same precedence.

The service remains deterministic for the same input and installed model version. Confidence values are source indicators (0.80 for spaCy, 0.45 for fallback), not empirically calibrated probabilities.

## 4. Preference-to-memory integration

Preference statements are not action commands. The C++ interaction layer records supported statements as semantic memory relations:

- Positive statement: \`likes\`
- Negative statement: \`dislikes\`

Recognised object categories include ball, stick, rope, sound/noise, and rest/sleep. Existing object extraction and memory ownership rules remain in place:
- Proposed agent may update its semantic memory.
- Baseline must not gain persistent semantic memory.
- A preference statement does not itself force an action or update the Q-table.
- Unresolved preference objects are logged as unresolved rather than being turned into a fabricated action.

The C++ preference detector now also recognises American \`favorite\` and British \`favourite\` wording. The NLP service classifies both as \`preference\`.

Examples:
- “I prefer the rope toy” → \`preference\`, object \`rope\`, relation \`likes\`.
- “My favourite toy is the stick” → \`preference\`, object \`stick\`, relation \`likes\`.
- “I don't like the ball” → \`negative_feedback\` at the NLP boundary; the C++ preference memory handler records \`dislikes ball\`.
- “Please fetch the ball” → \`request\`, not a preference.
- “Good dog, well done” → \`praise\`.

## 5. JSON-lines protocol

Input:
\`\`\`json
{"text":"I prefer the rope toy"}
\`\`\`

Output shape:
\`\`\`json
{
  "intent": "preference",
  "goal_state": 0,
  "target_proximity": 0,
  "task_status": 0,
  "signal_strength": 0.5,
  "reward": 0.6,
  "confidence": 0.8,
  "method": "spacy"
}
\`\`\`

When the model is unavailable, \`method\` is \`keyword-fallback\`. Empty input returns \`method: empty\`; malformed requests return one \`service-error\` response without terminating the persistent process.

## 6. Tests and acceptance

- Existing JSON-lines smoke test retains request, negative-feedback and greeting cases.
- Expanded smoke test covers preference, favourite/favorite, negative preference, and praise through the persistent process.
- Deterministic unit tests exercise fallback classification and intent-to-observation mappings.
- CTest registers the unit test as \`nlp_intent_classification_test\`.
- Existing \`nlp_service_smoke_test\` remains registered.

## 7. Boundaries and non-goals

This change does not add a trained intent classifier, neural model, cloud API, or external dependency. It does not alter reward shaping, epsilon scheduling, synthetic-user parameters, Q-learning, simulation transitions, participant deployment, the deployment branch, the web repository, or Vercel configuration.

The supplied intent logic is integrated with a correction to imperative detection: a first-person subject prevents a root verb such as “like” from being mistaken for an imperative. The C++ preference memory handoff is also extended to recognise both spellings of “favorite”.

## 8. Verification record

Repository changes are committed on \`Dev\`. Remote editing did not provide a local C++ build or execute the Python tests against the project's installed spaCy model. Run the commands below in the local checkout before treating the integration as verified:

\`\`\`bash
python3 tests/test_nlp_intent_classification.py
python3 tests/nlp_service_smoke_test.py
cmake -S . -B build -DANIMAL_AGENT_BUILD_RESEARCH_TESTS=ON
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
\`\`\`

The test suite permits the keyword fallback when spaCy/model assets are unavailable. For a separate spaCy-path check, ensure \`en_core_web_sm\` is installed in the same Python environment used by the application.
