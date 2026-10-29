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
#include <stdexcept>
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
    std::string id;
    std::string prompt;
    std::vector<std::string> eventTags;
    EnvironmentObservation observation;
};

struct SurveyItem {
    std::string key;
    std::string question;
    int rating = 0;
};

std::vector<Scenario> loadScenarios(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Cannot open scenario fixture: " + path.string());
    const nlohmann::json root = nlohmann::json::parse(input);
    if (root.value("schema_version", 0) != 1 || !root.contains("scenarios") ||
        !root["scenarios"].is_array() || root["scenarios"].empty())
        throw std::runtime_error("Invalid or empty scenario fixture");
    std::vector<Scenario> result;
    for (const auto& item : root["scenarios"]) {
        const auto& o = item.at("observation");
        Scenario s;
        s.id = item.at("id").get<std::string>();
        s.prompt = item.at("prompt").get<std::string>();
        s.eventTags = item.value("event_tags", std::vector<std::string>{});
        s.observation.goal_state = o.at("goal_state").get<int>();
        s.observation.target_proximity = o.at("target_proximity").get<int>();
        s.observation.user_status = o.at("user_status").get<int>();
        s.observation.task_status = o.at("task_status").get<int>();
        s.observation.signal_strength = o.at("signal_strength").get<float>();
        s.observation.food_available = o.value("food_available", false);
        s.observation.play_available = o.value("play_available", false);
        s.observation.energy = o.value("energy", 0.8f);
        result.push_back(std::move(s));
    }
    return result;
}

std::vector<SurveyItem> loadSurvey(const std::filesystem::path& path, std::string& version) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Cannot open questionnaire fixture: " + path.string());
    const nlohmann::json root = nlohmann::json::parse(input);
    if (root.value("schema_version", 0) != 1 || !root.contains("items") ||
        !root["items"].is_array() || root["items"].size() != 5)
        throw std::runtime_error("Questionnaire fixture must contain exactly five items");
    version = root.value("questionnaire_version", std::string("unspecified"));
    std::vector<SurveyItem> result;
    for (const auto& item : root["items"])
        result.push_back({item.at("key").get<std::string>(), item.at("question").get<std::string>(), 0});
    return result;
}

nlohmann::json affectToJson(const AffectiveState& a) {
    const char* v = a.valence == AffectValence::POSITIVE ? "positive" :
                    a.valence == AffectValence::NEGATIVE ? "negative" : "neutral";
    const char* e = a.energy == AffectEnergy::LOW ? "low" :
                    a.energy == AffectEnergy::HIGH ? "high" : "moderate";
    return {{"valence",v},{"energy",e},{"arousal",a.arousal},{"engagement",a.engagement},
            {"stress",a.stress},{"affinity",a.affinity}};
}


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
                           const std::vector<SurveyItem>& survey) {
    nlohmann::json condition;
    condition["condition_code"] = conditionCode;
    condition["trials"] = trials;
    condition["questionnaire"] = nlohmann::json::object();
    int total = 0;
    bool complete = survey.size() == 5;
    for (const auto& item : survey) {
        condition["questionnaire"][item.key] = item.rating;
        if (item.rating < 1 || item.rating > 5) complete = false;
        total += item.rating;
    }
    condition["questionnaire_version"] = root.value("questionnaire_version", std::string("unspecified"));
    condition["believability_score"] = complete
        ? nlohmann::json(static_cast<double>(total) / static_cast<double>(survey.size()))
        : nlohmann::json(nullptr);
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

    std::vector<Scenario> scenarios;
    std::vector<SurveyItem> surveyTemplate;
    std::string questionnaireVersion;
    try {
        const fs::path dataDirectory = fs::path(IS2_SOURCE_DIR) / "Data" / "Study";
        scenarios = loadScenarios(dataDirectory / "scenarios.json");
        surveyTemplate = loadSurvey(dataDirectory / "questionnaire.json", questionnaireVersion);
    } catch (const std::exception& error) {
        std::cerr << "Unable to load believability study fixtures: " << error.what() << '\n';
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

    const fs::path sessionPolicyDirectory = fs::path(outputDirectory) / sessionId / "policies";
    fs::create_directories(sessionPolicyDirectory, directoryError);
    if (directoryError) {
        std::cerr << "Unable to create isolated study policy directory: " << directoryError.message() << '\n';
        return false;
    }
    const fs::path sessionBaselinePolicy = sessionPolicyDirectory / "baseline.qtable";
    const fs::path sessionProposedPolicy = sessionPolicyDirectory / "proposed.qtable";
    try {
        fs::copy_file(baselinePolicy, sessionBaselinePolicy, fs::copy_options::overwrite_existing);
        fs::copy_file(proposedPolicy, sessionProposedPolicy, fs::copy_options::overwrite_existing);
    } catch (const fs::filesystem_error& error) {
        std::cerr << "Unable to initialise isolated study Q-tables: " << error.what() << '\n';
        return false;
    }
    EnvironmentConfig config;
    config.seed = static_cast<int>(std::chrono::system_clock::now().time_since_epoch().count() & 0x7fffffff);
    BaselineAgent baseline(config);
    ProposedAgent proposed(config);
    baseline.loadPersistentState(sessionBaselinePolicy.string());
    proposed.loadPersistentState(sessionProposedPolicy.string());
    baseline.setExplorationRate(0.0f);
    proposed.setExplorationRate(0.0f);
    baseline.setLearningEnabled(true);
    proposed.setLearningEnabled(true);

    Screen screen = Screen::Consent;
    std::string currentCondition;
    std::string currentAgentKind;
    int conditionIndex = 0;
    std::size_t scenarioIndex = 0;
    std::vector<SurveyItem> survey = surveyTemplate;
    std::size_t surveyIndex = 0;
    nlohmann::json trialRows = nlohmann::json::array();
    bool studyDataSaved = false;
    nlohmann::json studyData = nlohmann::json::object();
    studyData["schema_version"] = 1;
    studyData["session_id"] = sessionId;
    studyData["consent"] = true;
    studyData["policy_updates_enabled"] = true;
    studyData["protocol_id"] = "believability-v1";
    studyData["scenario_version"] = 1;
    studyData["questionnaire_version"] = questionnaireVersion;
    studyData["software_revision"] = IS2_GIT_REVISION;
    studyData["policy_initialisation"] = {
        {"baseline_source", baselinePolicy.string()},
        {"baseline_session_copy", sessionBaselinePolicy.string()},
        {"proposed_source", proposedPolicy.string()},
        {"proposed_session_copy", sessionProposedPolicy.string()},
        {"rule", "copy Q-table files only; do not import training-time memory or affect sidecars"}
    };
    studyData["started_at_epoch_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    studyData["exploration_rate"] = 0.0;
    studyData["learning_rate_updates"] = "enabled on observed participant feedback";
    studyData["conditions"] = nlohmann::json::array();
    studyData["condition_key"] = nlohmann::json::object();
    studyData["scenario_order"] = nlohmann::json::array();
    for (const auto& scenario : scenarios) studyData["scenario_order"].push_back(scenario.id);
    studyData["rating_scale"] = "1=strongly disagree, 5=strongly agree";
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
        std::ofstream events(fs::path(outputDirectory) / (sessionId + ".jsonl"));
        if (!events) {
            std::cerr << "Unable to write study event log for session " << sessionId << '\n';
            return false;
        }
        for (const auto& condition : studyData["conditions"]) {
            for (const auto& trial : condition["trials"]) {
                nlohmann::json event = trial;
                event["schema_version"] = 1;
                event["run_id"] = sessionId;
                event["condition_code"] = condition["condition_code"];
                event["condition"] = studyData["condition_key"][condition["condition_code"].get<std::string>()];
                event["questionnaire_version"] = studyData["questionnaire_version"];
                events << event.dump() << '\n';
            }
        }
        if (!events) {
            std::cerr << "Failed while writing study event log for session " << sessionId << '\n';
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
                    survey = surveyTemplate;
                    if (currentAgentKind == "baseline") baseline.reset();
                    else proposed.reset();
                    screen = Screen::Scenario;
                }
            } else if (screen == Screen::Scenario) {
                if (key == sf::Keyboard::Key::Escape) {
                    window.close();
                } else if (key == sf::Keyboard::Key::Enter || key == sf::Keyboard::Key::Space) {
                    const Scenario& scenario = scenarios[scenarioIndex];
                    const std::string action = currentAgentKind == "baseline"
                        ? baseline.selectAction(scenario.observation)
                        : proposed.selectAction(scenario.observation);
                    nlohmann::json row;
                    row["trial"] = scenarioIndex;
                    row["scenario_id"] = scenario.id;
                    row["scenario_version"] = studyData["scenario_version"];
                    row["scenario"] = scenario.prompt;
                    row["event_tags"] = scenario.eventTags;
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
                    if (currentAgentKind == "proposed") {
                        const auto& trace = proposed.getLastDecisionTrace();
                        row["state_key_before"] = trace.encoded_state;
                        row["memory_count_before"] = trace.memory_record_count;
                        row["affect_before"] = affectToJson(trace.affect);
                    } else {
                        row["state_key_before"] = nullptr;
                        row["memory_count_before"] = 0;
                        row["affect_before"] = nullptr;
                    }
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
                    const Scenario& scenario = scenarios[scenarioIndex];
                    const float reward = feedbackReward(feedbackKey);
                    ActionOutcome outcome;
                    outcome.action_id = static_cast<int>(scenarioIndex);
                    outcome.reward = reward;
                    outcome.terminal = scenarioIndex + 1 == scenarios.size();
                    outcome.outcome_tag = "participant_" + feedbackLabel(feedbackKey);
                    const std::string action = studyData["pending_trial"]["action_internal"].get<std::string>();
                    // "No response" is missing feedback, not a neutral judgement.
                    // Preserve it in the study record but do not update either agent.
                    const bool responseObserved = feedbackKey != 4;
                    if (responseObserved) {
                        if (currentAgentKind == "baseline") {
                            baseline.applyOutcome(scenario.observation, action, outcome);
                        } else {
                            proposed.applyOutcome(scenario.observation, action, outcome);
                        }
                    }
                    nlohmann::json row = studyData["pending_trial"];
                    row["participant_response"] = feedbackLabel(feedbackKey);
                    row["participant_response_code"] = feedbackKey;
                    row["response_observed"] = responseObserved;
                    row["feedback_reward_used_for_state_update"] =
                        responseObserved ? nlohmann::json(reward) : nlohmann::json(nullptr);
                    row["reward"] = responseObserved ? nlohmann::json(reward) : nlohmann::json(nullptr);
                    row["state_update_applied"] = responseObserved;
                    row["affect_memory_update_applied"] =
                        responseObserved && currentAgentKind == "proposed";
                    row["policy_update_enabled"] = true;
                    row["policy_update_applied"] = responseObserved;
                    if (currentAgentKind == "proposed") {
                        row["memory_count_after"] = proposed.getMemoryRecords().size();
                        row["affect_after"] = affectToJson(proposed.getAffectiveState());
                    } else {
                        row["memory_count_after"] = 0;
                        row["affect_after"] = nullptr;
                    }
                    trialRows.push_back(row);
                    studyData.erase("pending_trial");

                    if (scenarioIndex + 1 < scenarios.size()) {
                        ++scenarioIndex;
                        screen = Screen::Scenario;
                    } else {
                        survey = surveyTemplate;
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
                            survey = surveyTemplate;
                            if (currentAgentKind == "baseline") baseline.reset();
                            else proposed.reset();
                            screen = Screen::BetweenConditions;
                        } else {
                            studyData["completed"] = true;
                            studyData["condition_order"] = {conditionCodes[0], conditionCodes[1]};
                            studyData["policy_snapshots"] = {
                                {"baseline", sessionBaselinePolicy.string()},
                                {"proposed", sessionProposedPolicy.string()}
                            };
                            try {
                                baseline.savePersistentState(sessionBaselinePolicy.string());
                                proposed.savePersistentState(sessionProposedPolicy.string());
                            } catch (const std::exception& error) {
                                std::cerr << "Unable to save isolated policy snapshots: " << error.what() << '\n';
                                studyData["policy_snapshot_save_error"] = error.what();
                            }
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
                    "Each version responds to the same nine scripted scenarios.",
                    "After each behaviour, rate your response: 1 positive, 2 neutral,",
                    "3 negative, or 4 no response. Per-action feedback updates learning;",
                    "but the end-of-condition questionnaire is evaluation data only.",
                    "At the end of each version, rate five statements from 1 to 5.",
                    "Press Enter to begin."
                });
                break;
            case Screen::Scenario: {
                const Scenario& scenario = scenarios[scenarioIndex];
                drawFrame(window, font, "Condition " + currentCondition,
                    {"Scenario " + std::to_string(scenarioIndex + 1) + " of " + std::to_string(scenarios.size()),
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
                     "Rate from 1 (strongly disagree) to 5 (strongly agree).",
                     "1 - Strongly disagree",
                     "2 - Disagree",
                     "3 - Neither agree nor disagree",
                     "4 - Agree",
                     "5 - Strongly agree"});
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
