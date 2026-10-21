#include "study.h"

#include "Components/State-Action/state-action.h"

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {
enum class Screen {
    Consent,
    Introduction,
    Scenario,
    Feedback,
    Survey,
    BetweenConditions,
    Complete,
    Declined
};

struct Scenario {
    std::string prompt;
    EnvironmentObservation observation;
};

struct SurveyItem {
    std::string key;
    std::string question;
    int rating = 0;
};

const std::array<Scenario, 9> kScenarios{{
    {"You greet the companion after entering the yard.",
     EnvironmentObservation{0, 1, 0, 0, 0.55f}},
    {"You ask the companion to fetch a ball nearby.",
     EnvironmentObservation{1, 1, 1, 1, 0.85f}},
    {"A sudden loud noise occurs nearby.",
     EnvironmentObservation{2, 0, 0, 0, 0.90f}},
    {"You sit quietly to rest; no task is pending.",
     EnvironmentObservation{2, 2, 0, 0, 0.25f}},
    {"You praise the companion after an interaction.",
     EnvironmentObservation{0, 0, 1, 0, 0.75f}},
    {"You offer the companion a bowl of food.",
     EnvironmentObservation{2, 0, 1, 0, 0.30f, true, false}},
    {"You invite the companion to play.",
     EnvironmentObservation{0, 1, 1, 0, 0.70f, false, true}},
    {"You tell the companion to stop after an unwanted behaviour.",
     EnvironmentObservation{0, 1, 2, 0, 0.45f}},
    {"A task remains pending, but you do not respond to the companion's first attempt.",
     EnvironmentObservation{1, 1, 0, 1, 0.82f}}
}};

const std::array<SurveyItem, 4> kSurveyTemplate{{
    {"believability", "How believable did the companion's behaviour feel?", 0},
    {"consistency", "How consistent did its behaviour feel across interactions?", 0},
    {"adaptability", "How well did its behaviour seem to respond to your feedback?", 0},
    {"continuity", "How much did it seem to retain useful context from earlier interactions?", 0}
}};

std::string makeSessionId() {
    const auto ticks = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::random_device device;
    std::mt19937 rng(device());
    std::uniform_int_distribution<unsigned int> dist(100000u, 999999u);
    return "S" + std::to_string(ticks) + "-" + std::to_string(dist(rng));
}


float feedbackReward(int key) {
    if (key == 1) return 1.0f;
    if (key == 3) return -1.0f;
    return 0.0f;
}

std::string feedbackLabel(int key) {
    switch (key) {
        case 1: return "positive";
        case 2: return "neutral";
        case 3: return "negative";
        case 4: return "no response";
        default: return "unrecorded";
    }
}

void drawLines(sf::RenderWindow& window, const sf::Font& font,
               const std::vector<std::string>& lines, unsigned int size = 25,
               float startY = 70.0f, float lineGap = 44.0f) {
    float y = startY;
    for (const auto& line : lines) {
        sf::Text text(font, line);
        text.setCharacterSize(size);
        text.setFillColor(sf::Color(30, 35, 42));
        text.setPosition({48.0f, y});
        window.draw(text);
        y += lineGap;
    }
}

void drawFrame(sf::RenderWindow& window, const sf::Font& font,
               const std::string& title, const std::vector<std::string>& lines) {
    window.clear(sf::Color(244, 246, 248));
    sf::Text heading(font, title);
    heading.setCharacterSize(34);
    heading.setFillColor(sf::Color(32, 57, 78));
    heading.setPosition({48.0f, 22.0f});
    window.draw(heading);
    drawLines(window, font, lines);
    window.display();
}

void appendConditionResult(nlohmann::json& root,
                           const std::string& conditionCode,
                           const std::string& agentKind,
                           const nlohmann::json& trials,
                           const std::array<SurveyItem, 4>& survey) {
    nlohmann::json condition;
    condition["condition_code"] = conditionCode;
    condition["trials"] = trials;
    condition["questionnaire"] = nlohmann::json::object();
    for (const auto& item : survey) {
        condition["questionnaire"][item.key] = item.rating;
    }
    root["conditions"].push_back(condition);
    root["condition_key"][conditionCode] = agentKind;
}
} // namespace

bool runParticipantStudy(const std::string& policyDirectory,
                         const std::string& outputDirectory) {
    namespace fs = std::filesystem;
    const fs::path policyPath(policyDirectory);
    const fs::path baselinePolicy = policyPath / "baseline.qtable";
    const fs::path proposedPolicy = policyPath / "proposed.qtable";
    if (!fs::exists(baselinePolicy) || !fs::exists(proposedPolicy)) {
        std::cerr << "Study mode requires trained policies. Expected "
                  << baselinePolicy << " and " << proposedPolicy << ".\n"
                  << "Run --train first, then launch --study.\n";
        return false;
    }

    std::error_code directoryError;
    fs::create_directories(outputDirectory, directoryError);
    if (directoryError) {
        std::cerr << "Unable to create study output directory: " << directoryError.message() << '\n';
        return false;
    }

    sf::Font font;
    if (!font.openFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")) {
        std::cerr << "Study UI could not load DejaVuSans.ttf. Install the DejaVu fonts or update study.cpp with a valid font path.\n";
        return false;
    }

    sf::RenderWindow window(sf::VideoMode(sf::Vector2u(1120u, 760u)), "Companion Interaction Study");
    window.setFramerateLimit(60);

    const std::string sessionId = makeSessionId();
    std::random_device device;
    std::mt19937 orderRng(device());
    std::bernoulli_distribution swapOrder(0.5);
    const bool proposedFirst = swapOrder(orderRng);
    const std::array<std::string, 2> conditionCodes = {"A", "B"};
    const std::array<std::string, 2> agentKinds = proposedFirst
        ? std::array<std::string, 2>{"proposed", "baseline"}
        : std::array<std::string, 2>{"baseline", "proposed"};

    EnvironmentConfig config;
    config.seed = static_cast<int>(std::chrono::system_clock::now().time_since_epoch().count() & 0x7fffffff);
    BaselineAgent baseline(config);
    ProposedAgent proposed(config);
    baseline.loadPersistentState(baselinePolicy.string());
    proposed.loadPersistentState(proposedPolicy.string());
    baseline.setExplorationRate(0.0f);
    proposed.setExplorationRate(0.0f);
    baseline.setLearningEnabled(false);
    proposed.setLearningEnabled(false);

    Screen screen = Screen::Consent;
    std::string currentCondition;
    std::string currentAgentKind;
    int conditionIndex = 0;
    std::size_t scenarioIndex = 0;
    std::array<SurveyItem, 4> survey = kSurveyTemplate;
    std::size_t surveyIndex = 0;
    nlohmann::json trialRows = nlohmann::json::array();
    bool studyDataSaved = false;
    nlohmann::json studyData = nlohmann::json::object();
    studyData["schema_version"] = 1;
    studyData["session_id"] = sessionId;
    studyData["consent"] = true;
    studyData["policy_updates_enabled"] = false;
    studyData["conditions"] = nlohmann::json::array();
    studyData["condition_key"] = nlohmann::json::object();
    studyData["scenario_order"] = nlohmann::json::array();
    for (std::size_t i = 0; i < kScenarios.size(); ++i) {
        studyData["scenario_order"].push_back(i);
    }
    studyData["rating_scale"] = "1=not at all, 5=very much";
    studyData["limitations"] = "Synthetic task and participant ratings are separate measures; ratings are not training rewards.";

    const auto saveStudyData = [&]() {
        const fs::path path = fs::path(outputDirectory) / (sessionId + ".json");
        std::ofstream out(path);
        if (!out) {
            std::cerr << "Unable to write study data to " << path << '\n';
            return false;
        }
        out << studyData.dump(2) << '\n';
        if (!out) {
            std::cerr << "Failed while writing study data to " << path << '\n';
            return false;
        }
        return true;
    };

    while (window.isOpen()) {
        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
                continue;
            }
            const auto* keyEvent = event->getIf<sf::Event::KeyPressed>();
            if (keyEvent == nullptr) continue;
            const auto key = keyEvent->code;

            if (screen == Screen::Consent) {
                if (key == sf::Keyboard::Key::Y) {
                    screen = Screen::Introduction;
                } else if (key == sf::Keyboard::Key::N || key == sf::Keyboard::Key::Escape) {
                    screen = Screen::Declined;
                }
            } else if (screen == Screen::Introduction) {
                if (key == sf::Keyboard::Key::Enter || key == sf::Keyboard::Key::Space) {
                    conditionIndex = 0;
                    currentCondition = conditionCodes[0];
                    currentAgentKind = agentKinds[0];
                    scenarioIndex = 0;
                    trialRows = nlohmann::json::array();
                    survey = kSurveyTemplate;
                    if (currentAgentKind == "baseline") baseline.reset();
                    else proposed.reset();
                    screen = Screen::Scenario;
                }
            } else if (screen == Screen::Scenario) {
                if (key == sf::Keyboard::Key::Escape) {
                    window.close();
                } else if (key == sf::Keyboard::Key::Enter || key == sf::Keyboard::Key::Space) {
                    const Scenario& scenario = kScenarios[scenarioIndex];
                    const std::string action = currentAgentKind == "baseline"
                        ? baseline.selectAction(scenario.observation)
                        : proposed.selectAction(scenario.observation);
                    nlohmann::json row;
                    row["trial"] = scenarioIndex;
                    row["scenario"] = scenario.prompt;
                    row["condition_code"] = currentCondition;
                    row["action_label"] = actionLabel(action);
                    row["action_internal"] = action;
                    row["observation"] = {
                        {"goal_state", scenario.observation.goal_state},
                        {"target_proximity", scenario.observation.target_proximity},
                        {"user_status", scenario.observation.user_status},
                        {"task_status", scenario.observation.task_status},
                        {"signal_strength", scenario.observation.signal_strength},
                        {"food_available", scenario.observation.food_available},
                        {"play_available", scenario.observation.play_available}
                    };
                    studyData["pending_trial"] = row;
                    screen = Screen::Feedback;
                }
            } else if (screen == Screen::Feedback) {
                int feedbackKey = 0;
                if (key == sf::Keyboard::Key::Num1) feedbackKey = 1;
                else if (key == sf::Keyboard::Key::Num2) feedbackKey = 2;
                else if (key == sf::Keyboard::Key::Num3) feedbackKey = 3;
                else if (key == sf::Keyboard::Key::Num4) feedbackKey = 4;
                if (feedbackKey != 0) {
                    const Scenario& scenario = kScenarios[scenarioIndex];
                    const float reward = feedbackReward(feedbackKey);
                    ActionOutcome outcome;
                    outcome.action_id = static_cast<int>(scenarioIndex);
                    outcome.reward = reward;
                    outcome.terminal = scenarioIndex + 1 == kScenarios.size();
                    outcome.outcome_tag = "participant_" + feedbackLabel(feedbackKey);
                    const std::string action = studyData["pending_trial"]["action_internal"].get<std::string>();
                    if (currentAgentKind == "baseline") {
                        baseline.applyOutcome(scenario.observation, action, outcome);
                    } else {
                        proposed.applyOutcome(scenario.observation, action, outcome);
                    }
                    nlohmann::json row = studyData["pending_trial"];
                    row["participant_response"] = feedbackLabel(feedbackKey);
                    row["participant_response_code"] = feedbackKey;
                    row["feedback_reward_used_for_state_update"] = reward;
                    trialRows.push_back(row);
                    studyData.erase("pending_trial");

                    if (scenarioIndex + 1 < kScenarios.size()) {
                        ++scenarioIndex;
                        screen = Screen::Scenario;
                    } else {
                        survey = kSurveyTemplate;
                        surveyIndex = 0;
                        screen = Screen::Survey;
                    }
                }
            } else if (screen == Screen::Survey) {
                int rating = 0;
                if (key == sf::Keyboard::Key::Num1) rating = 1;
                else if (key == sf::Keyboard::Key::Num2) rating = 2;
                else if (key == sf::Keyboard::Key::Num3) rating = 3;
                else if (key == sf::Keyboard::Key::Num4) rating = 4;
                else if (key == sf::Keyboard::Key::Num5) rating = 5;
                if (rating != 0) {
                    survey[surveyIndex].rating = rating;
                    if (surveyIndex + 1 < survey.size()) {
                        ++surveyIndex;
                    } else {
                        appendConditionResult(studyData, currentCondition, currentAgentKind, trialRows, survey);
                        if (conditionIndex == 0) {
                            ++conditionIndex;
                            currentCondition = conditionCodes[1];
                            currentAgentKind = agentKinds[1];
                            scenarioIndex = 0;
                            trialRows = nlohmann::json::array();
                            survey = kSurveyTemplate;
                            if (currentAgentKind == "baseline") baseline.reset();
                            else proposed.reset();
                            screen = Screen::BetweenConditions;
                        } else {
                            studyData["completed"] = true;
                            studyData["condition_order"] = {conditionCodes[0], conditionCodes[1]};
                            studyDataSaved = saveStudyData();
                            screen = Screen::Complete;
                        }
                    }
                }
            } else if (screen == Screen::BetweenConditions) {
                if (key == sf::Keyboard::Key::Enter || key == sf::Keyboard::Key::Space) {
                    screen = Screen::Scenario;
                }
            } else if (screen == Screen::Complete || screen == Screen::Declined) {
                if (key == sf::Keyboard::Key::Enter || key == sf::Keyboard::Key::Escape) {
                    window.close();
                }
            }
        }

        if (!window.isOpen()) break;
        switch (screen) {
            case Screen::Consent:
                drawFrame(window, font, "Consent to participate", {
                    "This short study compares two simulated companion behaviours.",
                    "Participation is voluntary. You may close the window at any time.",
                    "We save an anonymous session code, scenario responses, and ratings.",
                    "Do not enter your name or other identifying information.",
                    "The study uses synthetic scenarios; it is not a real animal.",
                    "Only proceed if you have the required study approval and consent process.",
                    "Press Y to consent and continue, or N to decline."
                });
                break;
            case Screen::Introduction:
                drawFrame(window, font, "Instructions", {
                    "You will experience two versions labelled A and B.",
                    "Their order is randomised; the interface does not identify which is which.",
                    "Each version responds to the same five short scenarios.",
                    "After each behaviour, rate your response: 1 positive, 2 neutral,",
                    "3 negative, or 4 no response. Your ratings are not policy-training data.",
                    "At the end of each version, rate four statements from 1 to 5.",
                    "Press Enter to begin."
                });
                break;
            case Screen::Scenario: {
                const Scenario& scenario = kScenarios[scenarioIndex];
                drawFrame(window, font, "Condition " + currentCondition,
                    {"Scenario " + std::to_string(scenarioIndex + 1) + " of " + std::to_string(kScenarios.size()),
                     scenario.prompt,
                     "Press Enter to let the companion respond.",
                     "The same scenario sequence is used for both versions.",
                     "Close the window to stop; incomplete sessions are not saved."});
                break;
            }
            case Screen::Feedback: {
                const auto pending = studyData["pending_trial"];
                drawFrame(window, font, "How did that response feel?",
                    {"Condition " + currentCondition,
                     pending["action_label"].get<std::string>(),
                     "Choose the response that best matches your reaction:",
                     "1 - Positive / appropriate",
                     "2 - Neutral / unsure",
                     "3 - Negative / inappropriate",
                     "4 - No response / prefer not to rate"});
                break;
            }
            case Screen::Survey:
                drawFrame(window, font, "Short questionnaire",
                    {"Condition " + currentCondition + " — item " + std::to_string(surveyIndex + 1) + " of " + std::to_string(survey.size()),
                     survey[surveyIndex].question,
                     "Rate from 1 (not at all) to 5 (very much).",
                     "1 - Not at all",
                     "2 - A little",
                     "3 - Moderately",
                     "4 - Quite a lot",
                     "5 - Very much"});
                break;
            case Screen::BetweenConditions:
                drawFrame(window, font, "Condition A complete",
                    {"Take a short pause if you need one.",
                     "The next version will use the same scenario sequence.",
                     "Please continue to respond based on your own impression.",
                     "Press Enter to start the next version."});
                break;
            case Screen::Complete:
                drawFrame(window, font, "Thank you",
                    {"The session is complete.",
                     studyDataSaved ? "Your anonymous responses have been saved." : "Warning: responses could not be saved; tell the researcher.",
                     "Session code: " + sessionId,
                     "The condition key is retained for analysis.",
                     "Press Enter to close this window."});
                break;
            case Screen::Declined:
                drawFrame(window, font, "Participation declined",
                    {"No study response data was saved.",
                     "Press Enter to close this window."});
                break;
        }
    }
    return true;
}
