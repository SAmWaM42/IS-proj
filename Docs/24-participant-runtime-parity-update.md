# Participant Runtime Parity Update

**Target branch:** \`deployment/participant-client\`  
**Source of selected runtime behavior:** \`Dev\`  
**Status:** Ported; local participant build and study walkthrough pending  
**Scope:** Runtime logic only. Researcher training orchestration, experiment configuration, and analysis utilities are intentionally excluded.

## 1. Why this is a selective port

The development and participant branches have diverged. The participant branch contains deployment-specific questionnaire, study, and packaging work. This update therefore ports selected runtime behavior without merging \`Dev\` wholesale or replacing participant-specific files.

## 2. Changes ported

### 2.1 Memory-biased fetch-object selection

The participant scene now supports three fetch objects: ball, stick, and rope toy.

- The scene initializes and renders all three objects.
- Fetch animation approaches, carries, and returns the selected object.
- The proposed agent chooses from its stored user statements through \`getPreferredFetchObject()\`.
- The baseline continues to fetch the ball, preserving the baseline's lack of persistent semantic preference memory.
- The participant interaction log records \`fetch_object_selected\`.
- The guided script now asks the participant to state a rope-toy preference, then asks the companion to fetch a toy.
- The guided protocol is versioned as \`companion-believability-v0.2\`, script version 2.

Supported preference records include \`likes(ball|stick|rope)\` and \`dislikes(ball|stick|rope)\`. The default remains the ball unless the memory contains a supported preference or a dislike of the default ball.

### 2.2 Shared observation and state representation

The participant runtime now carries the bounded \`no_response_count\` field in \`EnvironmentObservation\` and \`state\`. Both baseline and proposed state construction preserve it, and the state encoder uses a separate radix digit for values 0–2.

The action mask makes \`INSIST\` eligible only when a task remains pending, the user status is non-responsive, and at least two no-response observations have accumulated. Critical-energy recovery restrictions remain in place.

The guided participant UI does not currently model repeated ignored requests, so its observations leave this counter at the default value of zero. Consequently, `INSIST` is not eligible in the current guided interaction protocol. This matches the guided runtime on `Dev`; the staged no-response progression is part of the randomized training scenario generator, not the participant's user-driven interaction sequence.

### 2.3 NLP classification and preference wording

The participant NLP service now supports the intent categories:
- greeting
- request
- disturbance
- rest
- negative feedback
- praise
- preference
- unknown

Negative feedback takes precedence over positive/request terms. Preference classification recognises \`prefer\`, American \`favorite\`, British \`favourite\`, and subject-bearing \`like/love\` statements. Imperative request detection is restricted to supported action verbs. The deterministic keyword fallback remains available when spaCy or \`en_core_web_sm\` is unavailable.

The C++ interaction handler recognises both spellings of favorite/favourite and routes supported preference statements into the proposed agent's semantic memory.

### 2.4 Tests

- Expanded persistent JSON-lines NLP smoke test.
- Added deterministic NLP intent classification tests.
- Registered \`nlp_intent_classification_test\` in the participant CTest configuration.

## 3. Deliberately excluded

The following remain on \`Dev\` only:
- epsilon schedules and training-mode exploration changes;
- reward shaping and intrinsic exploration bonuses;
- synthetic-user training model and profile parameters;
- training diagnostics, dashboards, and artifact generation;
- researcher CLI and training orchestration;
- training-only scenario generation and evaluation scripts.

No Vercel project, participant web repository, production deployment, release asset, or packaging workflow was changed by this port.

## 4. Validation required

The port has been committed to the participant branch, but the participant build has not been executed in this remote editing session. In the local checkout, run:

\`\`\`bash
git checkout deployment/participant-client
git pull origin deployment/participant-client
cmake -S . -B build -DANIMAL_AGENT_BUILD_TESTS=ON
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
\`\`\`

Then complete the guided study walkthrough and confirm:
1. The preference statement is accepted and stored only by the proposed condition.
2. The baseline fetches the ball.
3. The proposed condition fetches the rope toy after the guided rope preference.
4. The carried object is visibly correct during pickup and return.
5. The participant manifest reports protocol \`companion-believability-v0.2\` and guided script version 2.
6. NLP smoke and classification tests pass with the local Python environment, including the spaCy path when \`en_core_web_sm\` is installed.

Do not treat the participant branch as release-ready until the local build, CTest suite, and guided interaction walkthrough pass.
