#include "companion-ui.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <memory>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
using nlohmann::json;

std::string quoteShell(const std::string& value) {
    std::string quoted(1, static_cast<char>(39));
    for (char ch : value) {
        if (static_cast<unsigned char>(ch) == 39) quoted += "'\\''";
        else quoted += ch;
    }
    quoted += static_cast<char>(39);
    return quoted;
}

std::string callNlp(const std::string& script, const std::string& input) {
    const std::string payload = json{{"text", input}}.dump();
    const std::string command = "printf %s " + quoteShell(payload) + " | python3 " +
                                quoteShell(script) + " 2>/dev/null";
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) return {};
    std::string output;
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe.get())) output += buffer;
    return output;
}
const sf::Color kBackground(239, 243, 247);
const sf::Color kPanel(255, 255, 255);
const sf::Color kInk(35, 42, 52);
const sf::Color kMuted(105, 115, 128);
const sf::Color kBlue(58, 112, 170);
const sf::Color kGreen(71, 145, 106);

std::string describeAction(const std::string& action) {
    if (action == "FETCH") return "Fetches the nearby object";
    if (action == "GREETING") return "Greets you warmly";
    if (action == "NUZZLE") return "Moves closer affectionately";
    if (action == "BARK") return "Barks at the disturbance";
    if (action == "ACT_CONFUSED") return "Pauses, appearing uncertain";
    if (action == "SLEEP_LAY") return "Lies down to rest";
    if (action == "WANDER") return "Explores the yard";
    if (action == "INSIST") return "Repeats its previous behaviour";
    return "Waits for an interaction";
}
std::string levelText(float value) {
    if (value < 0.34f) return "Low";
    if (value < 0.67f) return "Medium";
    return "High";
}
float clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }
}

CompanionUI::CompanionUI(const std::string& policyDirectoryValue,
                         const std::string& outputDirectoryValue)
    : window(sf::VideoMode(sf::Vector2u(1280u, 820u)), "Animal Agent | Companion Simulation"),
      policyDirectory(policyDirectoryValue),
      outputDirectory(outputDirectoryValue),
      baseline(EnvironmentConfig{}),
      proposed(EnvironmentConfig{}) {
    if (!font.openFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")) {
        throw std::runtime_error("Unable to load DejaVuSans.ttf. Install DejaVu fonts or update companion-ui.cpp.");
    }
    window.setFramerateLimit(60);
    std::filesystem::create_directories(outputDirectory);
    const auto basePath = std::filesystem::path(policyDirectory) / "baseline.qtable";
    const auto proposedPath = std::filesystem::path(policyDirectory) / "proposed.qtable";
    if (std::filesystem::exists(basePath)) baseline.loadPersistentState(basePath.string());
    if (std::filesystem::exists(proposedPath)) proposed.loadPersistentState(proposedPath.string());
    latestObservation = EnvironmentObservation{0, 1, 0, 0, 0.35f};
    appendLog("Simulation ready. Select an interaction or enter a short message.");
}

void CompanionUI::run() {
    while (window.isOpen()) {
        while (const auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                saveSession();
                window.close();
            } else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Escape) {
                    saveSession();
                    window.close();
                } else if (screen == Screen::Simulation && textInputFocused) {
                    if (key->code == sf::Keyboard::Key::Enter && !textInput.empty()) {
                        processTextInteraction(textInput);
                        textInput.clear();
                    } else if (key->code == sf::Keyboard::Key::Backspace && !textInput.empty()) {
                        textInput.pop_back();
                    }
                }
            } else if (const auto* text = event->getIf<sf::Event::TextEntered>()) {
                if (screen == Screen::Simulation && textInputFocused &&
                    text->unicode >= 32 && text->unicode < 127 && textInput.size() < 100) {
                    textInput.push_back(static_cast<char>(text->unicode));
                }
            } else if (const auto* click = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (click->button == sf::Mouse::Button::Left) {
                    handleClick(window.mapPixelToCoords(click->position));
                }
            }
        }
        draw();
    }
}

void CompanionUI::processInteraction(const std::string& label,
                                     const EnvironmentObservation& observation,
                                     float feedbackRewardValue) {
    latestObservation = observation;
    latestStimulus = label;
    latestBaselineAction = baseline.selectAction(observation);
    latestProposedAction = proposed.selectAction(observation);

    ActionOutcome baselineOutcome;
    baselineOutcome.reward = feedbackRewardValue;
    baselineOutcome.terminal = true;
    baselineOutcome.outcome_tag = "simulation_" + (feedbackRewardValue > 0 ? std::string("positive") :
                                                    feedbackRewardValue < 0 ? std::string("negative") : std::string("neutral"));
    ActionOutcome proposedOutcome = baselineOutcome;
    baseline.applyOutcome(observation, latestBaselineAction, baselineOutcome);
    proposed.applyOutcome(observation, latestProposedAction, proposedOutcome);

    baselineRewards.push_back(feedbackRewardValue);
    proposedRewards.push_back(feedbackRewardValue);
    ++interactionCount;
    agentX = 430.0f + static_cast<float>((interactionCount * 47) % 250);
    appendLog("You: " + label);
    appendLog("Baseline: " + describeAction(latestBaselineAction));
    appendLog("Proposed: " + describeAction(latestProposedAction));
    saveSession();
}

std::string CompanionUI::interpretIntent(const std::string& text, EnvironmentObservation& observation, float& reward) {
    observation = EnvironmentObservation{0, 1, 0, 0, 0.45f};
    reward = 0.25f;
    const std::string output = callNlp(nlpScript, text);
    try {
        if (!output.empty()) {
            const auto parsed = json::parse(output);
            const std::string intent = parsed.value("intent", std::string("unknown"));
            observation = EnvironmentObservation{
                parsed.value("goal_state", 0), parsed.value("target_proximity", 1),
                parsed.value("task_status", 0), 0, parsed.value("signal_strength", 0.45f)};
            reward = parsed.value("reward", 0.25f);
            nlpMethod = parsed.value("method", std::string("unknown"));
            return intent;
        }
    } catch (const std::exception&) {
        // Continue with a local fallback if Python is missing or returns invalid JSON.
    }
    std::string lowered = text;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    nlpMethod = "C++ keyword fallback";
    if (lowered.find("stop") != std::string::npos || lowered.find("annoy") != std::string::npos ||
        lowered.find("don't") != std::string::npos || lowered.find("do not") != std::string::npos) {
        observation = EnvironmentObservation{0, 1, 2, 0, 0.45f}; reward = -1.0f; return "negative_feedback";
    }
    if (lowered.find("fetch") != std::string::npos || lowered.find("ball") != std::string::npos ||
        lowered.find("bring") != std::string::npos || lowered.find("retrieve") != std::string::npos) {
        observation = EnvironmentObservation{1, 1, 1, 0, 0.85f}; reward = 0.65f; return "request";
    }
    if (lowered.find("noise") != std::string::npos || lowered.find("scared") != std::string::npos ||
        lowered.find("loud") != std::string::npos) {
        observation = EnvironmentObservation{2, 0, 0, 0, 0.95f}; reward = 0.0f; return "disturbance";
    }
    if (lowered.find("rest") != std::string::npos || lowered.find("tired") != std::string::npos ||
        lowered.find("quiet") != std::string::npos) {
        observation = EnvironmentObservation{2, 2, 0, 0, 0.20f}; reward = 0.25f; return "rest";
    }
    if (lowered.find("good") != std::string::npos || lowered.find("praise") != std::string::npos ||
        lowered.find("well done") != std::string::npos) {
        observation = EnvironmentObservation{0, 0, 1, 0, 0.75f}; reward = 1.0f; return "praise";
    }
    if (lowered.find("hello") != std::string::npos || lowered.find("hi") != std::string::npos ||
        lowered.find("hey") != std::string::npos) return "greeting";
    return "unknown";
}

void CompanionUI::processTextInteraction(const std::string& text) {
    EnvironmentObservation observation;
    float reward = 0.25f;
    const std::string intent = interpretIntent(text, observation, reward);
    processInteraction(text + " [" + intent + " / " + nlpMethod + "]", observation, reward);
}

void CompanionUI::handleClick(sf::Vector2f point) {
    const float x = point.x, y = point.y;
    if (y >= 14 && y <= 58) {
        if (x < 180) screen = Screen::Simulation;
        else if (x < 360) screen = Screen::AgentState;
        else if (x < 540) screen = Screen::Comparison;
        return;
    }
    if (screen != Screen::Simulation) return;
    textInputFocused = y >= 700 && y <= 750;
    if (y >= 445 && y <= 495) {
        if (x >= 40 && x < 255) processInteraction("Greet the companion", {0, 1, 0, 0, 0.55f}, 0.65f);
        else if (x >= 275 && x < 490) processInteraction("Ask the companion to fetch the ball", {1, 1, 1, 1, 0.85f}, 0.85f);
        else if (x >= 510 && x < 725) processInteraction("Praise the companion", {0, 0, 1, 0, 0.75f}, 1.0f);
        else if (x >= 745 && x < 960) processInteraction("A sudden loud noise occurs", {2, 0, 0, 0, 0.95f}, -0.15f);
        else if (x >= 980 && x < 1200) processInteraction("The companion should rest", {2, 2, 0, 0, 0.20f}, 0.25f);
        return;
    }
    if (y >= 700 && y <= 750 && x >= 1040 && !textInput.empty()) {
        processTextInteraction(textInput);
        textInput.clear();
    }
}

void CompanionUI::drawText(const std::string& value, float x, float y, unsigned int size, sf::Color color) {
    sf::Text text(font, value);
    text.setCharacterSize(size);
    text.setFillColor(color);
    text.setPosition({x, y});
    window.draw(text);
}

void CompanionUI::drawButton(const std::string& label, float x, float y, float width, float height, bool selected) {
    sf::RectangleShape rect({width, height});
    rect.setPosition({x, y});
    rect.setFillColor(selected ? sf::Color(218, 233, 247) : kPanel);
    rect.setOutlineColor(selected ? kBlue : sf::Color(215, 222, 230));
    rect.setOutlineThickness(1.0f);
    window.draw(rect);
    drawText(label, x + 12, y + 12, 16, selected ? kBlue : kInk);
}

void CompanionUI::drawBar(const std::string& label, float value, float x, float y, float width) {
    drawText(label + "  " + levelText(value), x, y, 15, kInk);
    sf::RectangleShape track({width, 10});
    track.setPosition({x, y + 25});
    track.setFillColor(sf::Color(225, 230, 235));
    window.draw(track);
    sf::RectangleShape fill({width * clamp01(value), 10});
    fill.setPosition({x, y + 25});
    fill.setFillColor(kBlue);
    window.draw(fill);
}

void CompanionUI::appendLog(const std::string& value) {
    eventLog.push_back(value);
    if (eventLog.size() > 12) eventLog.erase(eventLog.begin());
}

void CompanionUI::drawNavigation() {
    drawButton("Simulation", 28, 12, 145, 42, screen == Screen::Simulation);
    drawButton("Agent State", 185, 12, 145, 42, screen == Screen::AgentState);
    drawButton("Comparison", 342, 12, 145, 42, screen == Screen::Comparison);
    drawText("ANIMAL AGENT  /  SFML PROTOTYPE", 760, 22, 15, kMuted);
}

void CompanionUI::drawSimulationScreen() {
    drawText("MAIN SIMULATION", 30, 75, 26, kInk);
    drawText("Interact with the companion; both policies receive the same event.", 30, 106, 15, kMuted);
    sf::RectangleShape scene({1218, 260});
    scene.setPosition({30, 135});
    scene.setFillColor(sf::Color(217, 235, 216));
    scene.setOutlineColor(sf::Color(196, 214, 195));
    scene.setOutlineThickness(1);
    window.draw(scene);
    sf::RectangleShape path({470, 85});
    path.setPosition({360, 310});
    path.setFillColor(sf::Color(225, 210, 177));
    window.draw(path);
    sf::RectangleShape house({160, 100});
    house.setPosition({1040, 155});
    house.setFillColor(sf::Color(231, 221, 209));
    window.draw(house);
    sf::CircleShape tree(30);
    tree.setPosition({90, 160});
    tree.setFillColor(sf::Color(73, 143, 88));
    window.draw(tree);
    sf::CircleShape person(18);
    person.setPosition({userX, userY});
    person.setFillColor(sf::Color(216, 122, 110));
    window.draw(person);
    sf::CircleShape companion(22);
    companion.setPosition({agentX, agentY - 20});
    companion.setFillColor(kBlue);
    window.draw(companion);
    drawText("YOU", userX - 5, userY + 30, 12, kInk);
    drawText("COMPANION", agentX - 28, agentY + 8, 12, kInk);
    drawText("Current interaction: " + latestStimulus, 50, 360, 16, kInk);
    drawText("Baseline: " + describeAction(latestBaselineAction), 50, 386, 16, kBlue);
    drawText("Proposed: " + describeAction(latestProposedAction), 50, 412, 16, kGreen);

    const std::array<std::string, 5> labels{{"Greet", "Fetch ball", "Praise", "Loud noise", "Rest"}};
    const float bx = 40.0f;
    for (std::size_t i = 0; i < labels.size(); ++i) {
        drawButton(labels[i], bx + static_cast<float>(i) * 235.0f, 445, 215, 48);
    }
    drawText("Recent activity", 35, 510, 18, kInk);
    float logY = 540;
    for (auto it = eventLog.rbegin(); it != eventLog.rend() && logY < 675; ++it, logY += 23) {
        drawText(*it, 45, logY, 14, kMuted);
    }
    drawButton(textInput.empty() ? "Type a message here, then press Enter" : textInput, 35, 700, 980, 48, textInputFocused);
    drawButton("Send", 1030, 700, 120, 48);
    drawText("Enter to send  |  Esc to quit", 850, 765, 13, kMuted);
}

void CompanionUI::drawAgentStateScreen() {
    drawText("AGENT STATE", 30, 75, 26, kInk);
    drawText("Current context, affect and retrieved memory", 30, 106, 15, kMuted);
    auto panel = [&](float x, float y, float w, float h) {
        sf::RectangleShape r({w, h}); r.setPosition({x, y}); r.setFillColor(kPanel);
        r.setOutlineColor(sf::Color(220, 225, 232)); r.setOutlineThickness(1); window.draw(r);
    };
    panel(30, 140, 590, 270); panel(640, 140, 608, 270); panel(30, 425, 1218, 300);
    const auto& affect = proposed.getAffectiveState();
    drawText("Affective state (proposed agent)", 50, 158, 19, kGreen);
    drawBar("Attachment / affinity", affect.affinity, 50, 200, 235);
    drawBar("Stress", affect.stress, 340, 200, 235);
    drawBar("Arousal", affect.arousal, 50, 270, 235);
    drawBar("Engagement", affect.engagement, 340, 270, 235);
    drawText("Valence", 50, 350, 15, kInk);
    drawText(affect.valence == AffectValence::POSITIVE ? "Positive" :
             affect.valence == AffectValence::NEGATIVE ? "Negative" : "Neutral", 150, 350, 15, kBlue);
    drawText("Current action: " + latestProposedAction, 50, 380, 15, kInk);

    drawText("Current interaction context", 660, 158, 19, kBlue);
    drawText("Stimulus: " + latestStimulus, 660, 200, 15);
    drawText("Intent code: " + std::to_string(latestObservation.goal_state), 660, 230, 15);
    drawText("Target proximity: " + std::to_string(latestObservation.target_proximity), 660, 260, 15);
    drawText("Task active: " + std::string(latestObservation.task_status ? "Yes" : "No"), 660, 290, 15);
    drawText("Signal strength: " + std::to_string(latestObservation.signal_strength), 660, 320, 15);
    drawText("Interactions: " + std::to_string(interactionCount), 660, 350, 15);
    drawText("Current user context: local simulation session", 660, 380, 15);

    drawText("Relevant stored memories (proposed agent)", 50, 445, 19, kGreen);
    const auto& memories = proposed.getMemoryRecords();
    if (memories.empty()) drawText("No interaction memories have been recorded yet.", 50, 490, 16, kMuted);
    float y = 490;
    for (auto it = memories.rbegin(); it != memories.rend() && y < 690; ++it, y += 38) {
        std::ostringstream row;
        row << it->raw_fact.subject << "  --" << it->raw_fact.relation << "-->  "
            << it->raw_fact.object << "   | confidence " << std::fixed << std::setprecision(2) << it->confidence
            << " | context " << (it->current_context == record::USER_SPECIFIC ? "user-specific" : "general");
        drawText(row.str(), 50, y, 14, kInk);
    }
}

void CompanionUI::drawComparisonScreen() {
    drawText("AGENT COMPARISON", 30, 75, 26, kInk);
    drawText("Both agents receive identical interaction scenarios.", 30, 106, 15, kMuted);
    sf::RectangleShape left({590, 260}); left.setPosition({30, 145}); left.setFillColor(kPanel);
    left.setOutlineColor(sf::Color(215, 222, 230)); left.setOutlineThickness(1); window.draw(left);
    sf::RectangleShape right({608, 260}); right.setPosition({640, 145}); right.setFillColor(kPanel);
    right.setOutlineColor(sf::Color(215, 222, 230)); right.setOutlineThickness(1); window.draw(right);
    drawText("BASELINE AGENT", 55, 165, 20, kBlue);
    drawText(describeAction(latestBaselineAction), 55, 220, 18, kInk);
    drawText("No persistent semantic memory / affect augmentation", 55, 265, 14, kMuted);
    drawText("Interactions observed: " + std::to_string(baseline.getInteractionHistory().size()), 55, 310, 15);
    drawText("Last reward signal: " + std::to_string(baseline.getSessionContext().last_reward), 55, 345, 15);
    drawText("PROPOSED AGENT", 665, 165, 20, kGreen);
    drawText(describeAction(latestProposedAction), 665, 220, 18, kInk);
    drawText("Memory + affective-state augmentation", 665, 265, 14, kMuted);
    drawText("Memories stored: " + std::to_string(proposed.getMemoryRecords().size()), 665, 310, 15);
    drawText("Interactions observed: " + std::to_string(proposed.getInteractionHistory().size()), 665, 345, 15);
    drawText("Last reward signal: " + std::to_string(proposed.getSessionContext().last_reward), 665, 380, 15);

    sf::RectangleShape logPanel({1218, 265}); logPanel.setPosition({30, 425}); logPanel.setFillColor(kPanel);
    logPanel.setOutlineColor(sf::Color(215, 222, 230)); logPanel.setOutlineThickness(1); window.draw(logPanel);
    drawText("Response history", 50, 445, 19, kInk);
    float y = 485;
    for (auto it = eventLog.rbegin(); it != eventLog.rend() && y < 660; ++it, y += 27) {
        drawText(*it, 55, y, 15, kMuted);
    }
    drawText("Use the Simulation tab to run another shared interaction.", 50, 690, 14, kMuted);
}

void CompanionUI::draw() {
    window.clear(kBackground);
    drawNavigation();
    if (screen == Screen::Simulation) drawSimulationScreen();
    else if (screen == Screen::AgentState) drawAgentStateScreen();
    else drawComparisonScreen();
    window.display();
}

void CompanionUI::saveSession() {
    try {
        const auto directory = std::filesystem::path(outputDirectory);
        std::filesystem::create_directories(directory);
        nlohmann::json session;
        session["interaction_count"] = interactionCount;
        session["latest_stimulus"] = latestStimulus;
        session["baseline_action"] = latestBaselineAction;
        session["proposed_action"] = latestProposedAction;
        session["interactions"] = nlohmann::json::array();
        for (std::size_t i = 0; i < eventLog.size(); ++i) {
            session["interactions"].push_back(eventLog[i]);
        }
        std::ofstream out(directory / "simulation-session.json");
        if (!out) {
            std::cerr << "Unable to write simulation session summary.\n";
            return;
        }
        out << session.dump(2) << '\n';
        baseline.savePersistentState((directory / "baseline-session.qtable").string());
        proposed.savePersistentState((directory / "proposed-session.qtable").string());
    } catch (const std::exception& e) {
        std::cerr << "Unable to save simulation session: " << e.what() << '\n';
    }
}
