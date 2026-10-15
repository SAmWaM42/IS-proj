#pragma once

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <json.hpp>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>
#include <SFML/Window.hpp>

#include "Components/State-Action/state-action.h"

#define defaultWidth 1920
#define defaultHeight 1080

class object;

class tab {
public:
    std::string title;
    std::map<std::string, std::string> parameters_to_display;
    bool visible = false;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    void updateParameters(std::map<std::string, std::string> parameters_to_update);
};

class window {
public:
    std::vector<tab> window_tabs;
    void updateWindow();
};

class simulation_environment {
public:
    float clockT = 0.0f;
    float dt = 0.0f;
    std::string active_window;
    std::map<std::string, window> windows;
    std::vector<std::unique_ptr<object>> objectStore;
    std::string current_user;

    void initiate();
    void update();
    void fetchRepresentation();
    void addEntity(std::string entityType);
    void renderObjects();
};

class BackyardSimulation {
public:
    BackyardSimulation();
    void run();
    void runScenarioHistory();
    std::string getCurrentBehavior() const;
    std::string getCurrentObservation() const;
    std::vector<InteractionEvent> getInteractionHistory() const;
    SessionContext getSessionContext() const;
    RuntimeContext getRuntimeContext() const;

private:
    struct ScriptedScenarioEvent {
        std::string timestamp;
        std::string trigger;
        std::string user_signal;
        std::string proximity_label;
        float signal_strength = 0.0f;
        float reward = 0.0f;
        std::string selected_behavior;
    };

    sf::RenderWindow window;
    sf::RectangleShape ground;
    sf::RectangleShape path;
    sf::RectangleShape house;
    sf::CircleShape tree;
    sf::CircleShape agent;
    sf::CircleShape user;
    sf::Font hudFont;
    std::unique_ptr<sf::Text> hudText;
    sf::Clock clock;
    float agentX = 120.0f;
    float agentY = 300.0f;
    float userX = 560.0f;
    float userY = 260.0f;
    int stepCount = 0;

    std::vector<ScriptedScenarioEvent> scriptedEvents;
    std::vector<InteractionEvent> interactionHistory;
    std::vector<record> memoryRecords;
    SessionContext sessionContext;
    AffectiveState affectiveState;
    RuntimeContext runtimeContext;
    QLearning qPolicy;
    state currentModelState;
    std::string currentBehaviorText = "IDLE";
    std::string currentObservationText = "No active stimulus";
    float currentSignalStrength = 0.0f;
    float elapsedSimulationTime = 0.0f;
    float timeUntilNextEvent = 4.0f;
    std::size_t currentEventIndex = 0;
    bool sessionSaved = false;

    void initialiseScriptedEvents();
    void processScenarioEvent(const ScriptedScenarioEvent& event);
    void triggerLearningDrivenStimulus();
    void updateStimulusTimeline(float dt);
    std::string chooseBehaviorForStimulus(const std::string& trigger) const;
    std::string inferResultSummary(float reward, const std::string& behavior) const;
    std::string inferUserResponse(const std::string& action, float reward, const std::string& stimulus) const;
    float inferRewardForAction(const std::string& action, const std::string& stimulus) const;
    void persistSessionState();
    void updateHudText();

    void handleInput(float dt);
    void updateSimulation(float dt);
    void drawScene();
};
