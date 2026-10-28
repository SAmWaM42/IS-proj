#pragma once

#include <memory>
#include <deque>
#include <random>
#include <string>
#include <vector>

#include <SFML/Graphics.hpp>

#include "Components/State-Action/state-action.h"
#include "Components/Simulation/backyard-environment.h"

class CompanionUI {
public:
    explicit CompanionUI(const std::string& policyDirectory = "artifacts/training",
                         const std::string& outputDirectory = "artifacts/simulation",
                         bool researcherMode = false);
    void run();

private:
    enum class Screen { AgentSelection, Simulation, AgentState, Comparison };
    bool researcherMode = false;
    bool agentAIsProposed = false;
    char selectedAgentCode = 'A';
    bool selectedAgentIsProposed = false;

    sf::RenderWindow window;
    sf::Font font;
    sf::Texture environmentTexture;
    std::vector<std::unique_ptr<sf::Texture>> idleFrames;
    std::vector<std::unique_ptr<sf::Texture>> walkFrames;
    std::vector<std::unique_ptr<sf::Texture>> sitFrames;
    std::vector<std::unique_ptr<sf::Texture>> layFrames;
    std::vector<std::unique_ptr<sf::Texture>> sleepFrames;
    std::vector<std::unique_ptr<sf::Texture>> barkFrames;
    bool environmentTextureLoaded = false;
    sf::Clock frameClock;
    std::string policyDirectory;
    std::string outputDirectory;
    std::string runId;
    Screen screen = Screen::AgentSelection;
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
    int lastActionInteractionId = 0;
    bool controlsVisible = true;
    struct BehaviourStep {
        std::string pose = "idle";
        float targetX = -1.0f, speed = 0.0f, duration = 0.0f;
        std::string cue;
        float hop = 0.0f, hopRate = 0.0f, tiltBias = 0.0f, tilt = 0.0f;
        float tiltRate = 1.2f, shiver = 0.0f, crouch = 0.0f, frameInterval = 0.16f;
        int faceDir = 0;
        bool pickUpBall = false, dropBall = false, consumeFood = false;
        sf::Color tint = sf::Color::White;
    };
    struct SceneLayout {
        float x = 0, y = 0, w = 0, h = 0;
        float groundY = 0, userX = 0, bedX = 0, bowlX = 0, ballHomeX = 0;
        float treeX = 0, houseX = 0, minX = 0, maxX = 0;
    };
    struct SceneObject { std::string label; sf::FloatRect bounds; };
    std::mt19937 sceneRng{std::random_device{}()};
    float agentX = 520.0f;
    bool facingRight = false;
    std::vector<BehaviourStep> behaviourPlan;
    std::size_t stepIndex = 0;
    float stepTime = 0.0f;
    bool behaviourActive = false;
    std::string restingPose = "idle";
    int animationFrame = 0;
    float frameTimer = 0.0f;
    float sceneTime = 0.0f;
    float totalSimulationElapsedSeconds = 0.0f;
    // Shared physical energy, observed by both baseline and proposed agents.
    float environmentEnergy = 0.8f;
    float autonomousDecisionTimer = 0.0f;
    float autonomousDecisionInterval = 5.0f;
    float ballX = 600.0f, ballLandX = 600.0f, ballStartX = 0.0f, ballHeight = 0.0f, ballFlightTime = 0.0f;
    bool ballCarried = false, ballFlying = false;
    float throwTimer = 0.0f;
    bool foodPresent = false;
    struct QueuedInteraction {
        std::string label;
        EnvironmentObservation observation;
        float reward;
        std::string preference;
        bool preferencePositive = true;
        std::string sceneEvent;
    };
    std::deque<QueuedInteraction> interactionBuffer;
    void enqueueInteraction(const std::string& label, const EnvironmentObservation& observation, float reward);

    std::string nlpMethod = "not used";
    std::string assetsDirectory = "assets";
    std::string interpretIntent(const std::string& text, EnvironmentObservation& observation, float& reward);
    SceneLayout sceneLayout() const;
    std::vector<SceneObject> sceneObjects() const;
    const std::vector<std::unique_ptr<sf::Texture>>& framesForPose(const std::string& pose) const;
    void resetScene();
    void startNewParticipant();
    void openStudyForm();
    void applySceneEvent(const std::string& sceneEvent);
    void startBehaviour(const std::string& action, const std::string& sceneEvent);
    void beginStep();
    void finishStep();
    void updateAnimation(float elapsedSeconds);
    void drawCompanion(float x, float groundY);
    void drawPlayer(float x, float groundY);
    void drawBubble(const std::string& value, float x, float bottomY);
    void drawBackdrop(float x, float y, float width, float height);
    void drawSceneObjects();
    void drawHover();

    void processInteraction(const std::string& label, const EnvironmentObservation& observation,
                            float feedbackReward, const std::string& preference = "",
                            bool preferencePositive = true, const std::string& sceneEvent = "");
    void executeInteraction(const std::string& label, const EnvironmentObservation& observation,
                            float feedbackReward, const std::string& preference = "",
                            bool preferencePositive = true, const std::string& sceneEvent = "");
    void processTextInteraction(const std::string& text);
    void handleClick(sf::Vector2f point);
    void draw();
    void drawNavigation();
    void drawAgentSelectionScreen();
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
