#pragma once

#include <memory>
#include <string>
#include <vector>

#include <SFML/Graphics.hpp>

#include "Components/State-Action/state-action.h"

class CompanionUI {
public:
    explicit CompanionUI(const std::string& policyDirectory = "artifacts/training",
                         const std::string& outputDirectory = "artifacts/simulation");
    void run();

private:
    enum class Screen { Simulation, AgentState, Comparison };

    sf::RenderWindow window;
    sf::Font font;
    std::string policyDirectory;
    std::string outputDirectory;
    Screen screen = Screen::Simulation;
    BaselineAgent baseline;
    ProposedAgent proposed;
    EnvironmentObservation latestObservation;
    std::string latestBaselineAction = "IDLE";
    std::string latestProposedAction = "IDLE";
    std::string latestStimulus = "Welcome. Choose an interaction to begin.";
    std::string textInput;
    bool textInputFocused = false;
    std::vector<std::string> eventLog;
    std::vector<float> baselineRewards;
    std::vector<float> proposedRewards;
    int interactionCount = 0;
    float agentX = 500.0f;
    float agentY = 320.0f;
    float userX = 220.0f;
    float userY = 350.0f;
    bool sceneVisible = true;
    bool animationActive = false;
    float animationClock = 0.0f;
    std::string nlpMethod = "not used";
    std::string assetsDirectory = "assets";
    std::string nlpScript = "Components/NLP/nlp_intent_service.py";
    std::string interpretIntent(const std::string& text, EnvironmentObservation& observation, float& reward);
    void updateAnimation(float elapsedSeconds);
    void drawCompanion(float x, float y);

    void processInteraction(const std::string& label, const EnvironmentObservation& observation,
                            float feedbackReward);
    void processTextInteraction(const std::string& text);
    void handleClick(sf::Vector2f point);
    void draw();
    void drawNavigation();
    void drawSimulationScreen();
    void drawAgentStateScreen();
    void drawComparisonScreen();
    void drawText(const std::string& value, float x, float y, unsigned int size = 18,
                  sf::Color color = sf::Color(35, 42, 52));
    void drawButton(const std::string& label, float x, float y, float width, float height,
                    bool selected = false);
    void drawBar(const std::string& label, float value, float x, float y, float width);
    void appendLog(const std::string& value);
    void saveSession();
};
