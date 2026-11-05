# In-app questionnaire: integration notes

Files go in `Components/Questionnaire/`: questionnaire.h, questionnaire.cpp, questionnaire_content.cpp,
questionnaire_demo.cpp (stand-alone preview).

## 1. CMakeLists.txt
    target_sources(IS-2 PRIVATE
       Components/Questionnaire/questionnaire.cpp
       Components/Questionnaire/questionnaire_content.cpp)
    target_include_directories(IS-2 PRIVATE ${CMAKE_SOURCE_DIR}/Components/Questionnaire)

    # optional preview tool: ./build/questionnaire_demo   (F1 consent, F2 rating A, F3 rating B, F4 comparison)
    add_executable(questionnaire_demo
       Components/Questionnaire/questionnaire_demo.cpp
       Components/Questionnaire/questionnaire.cpp
       Components/Questionnaire/questionnaire_content.cpp)
    target_include_directories(questionnaire_demo PRIVATE ${CMAKE_SOURCE_DIR}/Inc ${CMAKE_SOURCE_DIR}/Components/Questionnaire)
    target_link_libraries(questionnaire_demo PRIVATE sfml-graphics sfml-window sfml-system)

Run the demo first: it lets you tune wording and layout without playing the simulation.

## 2. Using it from CompanionUI (sketch)
Header:
    #include "Components/Questionnaire/questionnaire.h"
    enum class Screen { ModeSelect, AgentSelection, Simulation, AgentState, Comparison, Questionnaire };
    questionnaire::View questionnaireView;          // declare right after 'sf::Font font;'
    std::string questionnaireTag;                   // "consent", "rating-A", "rating-B", "comparison"
    void startQuestionnaire(const questionnaire::Definition& definition, const std::string& tag);
    void finishQuestionnaire();
Constructor init list (after window): questionnaireView(window, font)

    void CompanionUI::startQuestionnaire(const questionnaire::Definition& definition, const std::string& tag) {
        const unsigned int seed = static_cast<unsigned int>(participantNumber * 100 + (selectedAgentCode == 'A' ? 1 : 2));
        nlohmann::json context = {{"run_id", runId}, {"participant_code", participantCode},
                                  {"session_index", sessionIndex}, {"condition_code", std::string(1, selectedAgentCode)},
                                  {"build_id", APP_BUILD_ID}, {"tag", tag}};
        questionnaireTag = tag;
        questionnaireView.begin(definition, context, seed);
        screen = Screen::Questionnaire;
    }

    void CompanionUI::finishQuestionnaire() {
        const nlohmann::json result = questionnaireView.result();
        const auto path = std::filesystem::path(outputDirectory) / ("questionnaire-" + runId + "-" + questionnaireTag + ".json");
        std::ofstream out(path);
        if (out) out << result.dump(2) << '\n';              // ALWAYS save locally first, then upload
        logStudyEvent("questionnaire_saved", nlohmann::json{{"tag", questionnaireTag}});
        // TODO: queue the HTTPS upload of 'result' (retry in a worker thread; keep the file if it fails)
        // Then continue the study flow, for example:
        //   rating-A / rating-B  -> formOpenedThisSession = true; screen = AgentSelection (or comparison once both are done)
        //   comparison           -> thank-you screen
        //   consent              -> if the 'consent' answer is not "Yes, I agree", return to the menu; else AgentSelection
    }

Per-frame and events:
    updateAnimation():  questionnaireView.update(dt);  if (screen == Screen::Questionnaire && questionnaireView.finished()) finishQuestionnaire();
    draw():             if (screen == Screen::Questionnaire) questionnaireView.draw(); (no top bar)
    handleClick():      if (screen == Screen::Questionnaire) { questionnaireView.handleClick(point); return; }
    run() events:       TextEntered    -> questionnaireView.handleText(text->unicode)
                        KeyPressed     -> questionnaireView.handleKey(key->code)   (Backspace, Enter)
                        MouseWheelScrolled -> questionnaireView.handleScroll(wheel->delta)
In run(), skip the simulation's own text-box handling while screen == Screen::Questionnaire.

Where it fits the study loop: Research Session card -> consent + background -> companion selection -> session A
(warm-up, 12 steps, free time) -> rating A -> session B -> rating B -> comparison -> thank you.
Replace the Answer Form button's action (openStudyForm) with startQuestionnaire(makeCompanionRating(...)).

## 3. Writing questions (questionnaire_content.cpp)
Helpers: likert(id, text, construct, reverse), choice(id, text, options), scale(id, text, low, high), freeText(id, text, paragraph), info(text).
Rows are shuffled across pages with the seed passed to makeCompanionRating (stable per participant and companion).
Reverse-scored flags, constructs and the attention check are stored in the JSON, never shown.

## 4. Result JSON (shape)
    { "schema_version": 1, "questionnaire_id": "rating_A", "title": "...",
      "context": { run_id, participant_code, session_index, condition_code, build_id, tag },
      "started_at": "...Z", "completed_at": "...Z", "duration_seconds": 214.8, "shuffle_seed": 101,
      "responses": [ { "id": "C12", "page": 2, "display_position": 3, "type": "likert", "value": 4,
                       "construct": "Emotional expression", "answered_at_ms": 15230.0, "changes": 0 }, ...,
                     { "id": "C21", "type": "likert", "attention_check": true, "expected": 4, "passed": true, ... },
                     { "id": "comment_most_lifelike", "type": "text", "value": "..." } ],
      "attention_checks_passed": true }

## 5. Controls
Click answers; Back / Next / Submit (Enter also goes Next when no text box is focused); mouse wheel scrolls a long page;
required questions are highlighted if missing; text boxes take keyboard input (Backspace, Enter for new line in paragraphs).

## 6. Tested / not tested
- Syntax-checked and flow-tested with stub rendering: pagination, required-answer blocking, completion of all four
  definitions (consent, rating A, rating B, comparison).
- NOT run against real SFML 3 or the real nlohmann json: check clipping (sf::View viewport), text widths and the colours.
- The consent answer is only recorded; the app must act on a "No" answer.
