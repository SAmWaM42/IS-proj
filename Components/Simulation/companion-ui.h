#pragma once

#include <memory>
#include <deque>
#include <random>
#include <string>
#include <vector>

#include <SFML/Graphics.hpp>

#include "Components/State-Action/state-action.h"

class CompanionUI {
public:
    // researcherMode = false (default): participant-facing study mode. Statistics,
    // agent state, comparison, action labels and the event log are hidden.
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

    // Autonomous (idle) decisions. Keep the values from your own header if they differ.
    float autonomousDecisionTimer = 0.0f;
    float autonomousDecisionInterval = 6.0f;

    // ---- Top-down scene and dog behaviour state ------------------------------
    // One BehaviourStep is a pose plus an optional walk target or a timed hold.
    struct BehaviourStep {
        std::string pose = "idle";   // idle, walk, sit, lay, sleep, bark
        bool hasTarget = false;      // true: walk to (targetX, targetY)
        float targetX = 0.0f;
        float targetY = 0.0f;
        bool trackUser = false;      // true: the target follows the (moving) player
        float userOffsetX = 0.0f;    // distance to keep from the player along x
        int trackSide = 1;           // which side of the player the dog stops on
        bool faceUser = false;       // keep facing the player while this step runs
        float speed = 0.0f;          // px/s while walking
        float duration = 0.0f;       // seconds, for stationary steps
        std::string cue;             // speech-bubble text ("" = none)
        float hop = 0.0f;
        float hopRate = 0.0f;
        float tiltBias = 0.0f;       // constant forward tilt (degrees, relative to facing)
        float tilt = 0.0f;           // oscillating tilt amplitude (degrees)
        float tiltRate = 1.2f;
        float shiver = 0.0f;
        float crouch = 0.0f;         // 0..0.2 vertical squash
        float frameInterval = 0.16f;
        int faceDir = 0;             // -1 left, +1 right, 0 = automatic
        bool pickUpBall = false;
        bool dropBall = false;
        bool consumeFood = false;
        sf::Color tint = sf::Color::White;
    };
    struct SceneLayout {
        float x = 0, y = 0, w = 0, h = 0;                 // scene rectangle
        float minX = 0, maxX = 0, minY = 0, maxY = 0;     // walkable area for feet
        float bedX = 0, bedY = 0, bowlX = 0, bowlY = 0;
        float ballHomeX = 0, ballHomeY = 0;
        float treeX = 0, houseX = 0;                      // left edges of backdrop sprites
    };
    struct SceneObject {
        std::string label;
        sf::FloatRect bounds;
    };

    std::mt19937 sceneRng{std::random_device{}()};
    // Player (feet position) and companion (feet position)
    float userX = 400.0f, userY = 480.0f;
    bool playerFacingRight = true;
    bool playerMoving = false;
    float agentX = 700.0f, agentY = 440.0f;
    bool facingRight = false;        // the supplied dog frames face LEFT
    std::vector<BehaviourStep> behaviourPlan;
    std::size_t stepIndex = 0;
    float stepTime = 0.0f;
    bool behaviourActive = false;
    std::string restingPose = "idle";
    int animationFrame = 0;
    float frameTimer = 0.0f;
    float sceneTime = 0.0f;
    // Ball: (ballX, ballY) is where it touches the ground, ballHeight its height above that.
    float ballX = 600.0f, ballY = 480.0f;
    float ballLandX = 600.0f, ballLandY = 480.0f;
    float ballStartX = 0.0f, ballStartY = 0.0f;
    float ballHeight = 0.0f;
    float ballFlightTime = 0.0f;
    bool ballCarried = false;
    bool ballFlying = false;
    float throwTimer = 0.0f;
    bool foodPresent = false;
    std::string pendingCue;          // bubble drawn after both characters
    float pendingCueX = 0.0f, pendingCueY = 0.0f;

    struct QueuedInteraction {
        std::string label;
        EnvironmentObservation observation;
        float reward;
        std::string preference;
        bool preferencePositive = true;
        std::string sceneEvent;
        std::string proximityBasis;  // "user", "ball", "bowl" or "" (keep preset proximity)
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
    void applyGeometryProximity(EnvironmentObservation& observation, const std::string& basis) const;
    void startBehaviour(const std::string& action, const std::string& sceneEvent);
    void beginStep();
    void finishStep();
    void updatePlayer(float elapsedSeconds);
    void updateAnimation(float elapsedSeconds);
    void drawCompanion(float x, float y);
    void drawPlayer(float x, float y);
    void drawBubble(const std::string& value, float x, float bottomY);
    void drawBackdrop(float x, float y, float width, float height);
    void drawSceneObjects();
    void drawControlHelper();
    void drawHover();

    void processInteraction(const std::string& label, const EnvironmentObservation& observation,
                            float feedbackReward, const std::string& preference = "",
                            bool preferencePositive = true, const std::string& sceneEvent = "",
                            const std::string& proximityBasis = "");
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
