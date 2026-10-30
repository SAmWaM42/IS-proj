#include "companion-ui.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <ctime>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdlib>
#include <memory>
#include <limits>
#include <json.hpp>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <random>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <future>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

namespace {
using nlohmann::json;

class PersistentNlpClient {
public:
    explicit PersistentNlpClient(std::string scriptPath) : script(std::move(scriptPath)), worker(&PersistentNlpClient::run, this) {}
    ~PersistentNlpClient() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stopping = true;
        }
        condition.notify_all();
        if (worker.joinable()) worker.join();
    }
    std::string request(const std::string& text) {
        auto promise = std::make_shared<std::promise<std::string>>();
        auto result = promise->get_future();
        {
            std::lock_guard<std::mutex> lock(mutex);
            requests.push(Request{json{{"text", text}}.dump(), promise});
        }
        condition.notify_one();
        return result.get();
    }
private:
    struct Request { std::string jsonLine; std::shared_ptr<std::promise<std::string>> result; };
    std::string script;
    std::mutex mutex;
    std::condition_variable condition;
    std::queue<Request> requests;
    bool stopping = false;
    std::thread worker;
    pid_t child = -1;
    int childInput = -1;
    int childOutput = -1;

    bool startService() {
        int toChild[2], fromChild[2];
        if (pipe(toChild) != 0 || pipe(fromChild) != 0) return false;
        child = fork();
        if (child == 0) {
            dup2(toChild[0], STDIN_FILENO);
            dup2(fromChild[1], STDOUT_FILENO);
            close(toChild[0]); close(toChild[1]); close(fromChild[0]); close(fromChild[1]);
            execlp("python3", "python3", script.c_str(), static_cast<char*>(nullptr));
            _exit(127);
        }
        close(toChild[0]); close(fromChild[1]);
        if (child < 0) { close(toChild[1]); close(fromChild[0]); return false; }
        childInput = toChild[1];
        childOutput = fromChild[0];
        return true;
    }
    void stopService() {
        if (childInput >= 0) { close(childInput); childInput = -1; }
        if (childOutput >= 0) { close(childOutput); childOutput = -1; }
        if (child > 0) { int status = 0; waitpid(child, &status, 0); child = -1; }
    }
    std::string exchange(const std::string& line) {
        if (childInput < 0 || childOutput < 0) return {};
        const std::string payload = line + "\n";
        std::size_t written = 0;
        while (written < payload.size()) {
            const ssize_t n = write(childInput, payload.data() + written, payload.size() - written);
            if (n <= 0) return {};
            written += static_cast<std::size_t>(n);
        }
        std::string response;
        char ch = 0;
        while (read(childOutput, &ch, 1) == 1) {
            if (ch == '\n') break;
            response.push_back(ch);
            if (response.size() > 65536) return {};
        }
        return response;
    }
    void run() {
        bool running = startService();
        for (;;) {
            Request requestItem;
            {
                std::unique_lock<std::mutex> lock(mutex);
                condition.wait(lock, [&] { return stopping || !requests.empty(); });
                if (stopping && requests.empty()) break;
                requestItem = std::move(requests.front());
                requests.pop();
            }
            std::string response;
            if (running) response = exchange(requestItem.jsonLine);
            if (response.empty()) {
                if (running) { stopService(); running = false; }
                response = R"({"intent":"unknown","goal_state":0,"target_proximity":1,"task_status":0,"signal_strength":0.45,"reward":0.25,"method":"nlp-service-unavailable"})";
            }
            requestItem.result->set_value(std::move(response));
        }
        stopService();
    }
};

PersistentNlpClient& nlpClient() {
    static PersistentNlpClient client("Components/NLP/nlp_intent_service.py");
    return client;
}
std::string callNlp(const std::string& input) {
    return nlpClient().request(input);
}
// Cosy palette: cream paper, cocoa ink, terracotta accent, sage green.
const sf::Color kBackground(250, 241, 225);
const sf::Color kPanel(255, 250, 240);
const sf::Color kInk(84, 58, 44);
const sf::Color kMuted(150, 118, 96);
const sf::Color kAccent(196, 112, 84);
const sf::Color kGreen(118, 150, 92);

std::string describeAction(const std::string& action) {
    return actionLabel(action);
}

std::string levelText(float value) {
    if (value < 0.34f) return "Low";
    if (value < 0.67f) return "Medium";
    return "High";
}
float clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }
std::string utcTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t value = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
    gmtime_r(&value, &utc);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return buffer;
}

std::string fileChecksum(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return "unavailable";
    unsigned long long hash = 1469598103934665603ULL;
    char byte = 0;
    while (input.get(byte)) {
        hash ^= static_cast<unsigned char>(byte);
        hash *= 1099511628211ULL;
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

// ---- Layout and scene constants -------------------------------------------
constexpr float kSceneX = 30.0f;
constexpr float kSceneY = 135.0f;
constexpr float kSceneW = 1218.0f;
constexpr float kSceneHControls = 430.0f;
constexpr float kSceneHFull = 710.0f;
constexpr float kDogScale = 4.5f;  // the supplied dog frames are 16 px tall
constexpr float kPi = 3.14159265f;
constexpr float kButtonW = 215.0f;
constexpr float kButtonH = 44.0f;
constexpr float kButtonGap = 20.0f;
constexpr float kButtonX0 = 40.0f;
constexpr float kButtonRowStep = 52.0f;
constexpr float kInputY = 692.0f;
constexpr float kInputH = 48.0f;
const sf::Color kShadow(70, 45, 30, 70);
constexpr float kMouthOffset = 26.0f;    // distance from the dog's centre to its mouth
constexpr float kPlayerSpeed = 230.0f;   // player walking speed in px/s
// Map measured scene-space distance into the existing 0=close, 1=medium, 2=far state.
// Scale thresholds with the scene width so they remain meaningful if the UI is resized.
int proximityBucket(float distance, float sceneWidth) {
    return distance < sceneWidth * 0.10f ? 0 : distance < sceneWidth * 0.25f ? 1 : 2;
}

// ---- Cosy theme helpers --------------------------------------------------------
const sf::Color kShadowSoft(88, 56, 40, 46);        // warm drop shadow under panels
const sf::Color kAccentDark(150, 78, 54);           // text on selected accent surfaces
const sf::Color kAccentSoft(247, 218, 192);         // selected / highlighted surface
const sf::Color kBorder(228, 204, 174);             // soft panel border
const sf::Color kCream(255, 246, 228);              // bubbles and key caps

// Rounded rectangle built from a convex polygon (SFML has no native rounded shape).
void drawRoundedBox(sf::RenderTarget& target, float x, float y, float w, float h, float radius,
                    sf::Color fill, sf::Color outline = sf::Color::Transparent,
                    float outlineThickness = 0.0f) {
    radius = std::min(radius, std::min(w, h) * 0.5f);
    const int segments = 6;
    sf::ConvexShape shape;
    shape.setPointCount(static_cast<std::size_t>(4 * (segments + 1)));
    const float cx[4] = {x + w - radius, x + w - radius, x + radius, x + radius};
    const float cy[4] = {y + radius, y + h - radius, y + h - radius, y + radius};
    std::size_t index = 0;
    for (int corner = 0; corner < 4; ++corner) {
        for (int s = 0; s <= segments; ++s) {
            const float degrees = -90.0f + 90.0f * static_cast<float>(corner) +
                                  90.0f * static_cast<float>(s) / static_cast<float>(segments);
            const float angle = degrees * kPi / 180.0f;
            shape.setPoint(index++, {cx[corner] + radius * std::cos(angle), cy[corner] + radius * std::sin(angle)});
        }
    }
    shape.setFillColor(fill);
    shape.setOutlineColor(outline);
    shape.setOutlineThickness(outlineThickness);
    target.draw(shape);
}

// Two-colour gradient rectangle (vertical: c1 at the top, c2 at the bottom; otherwise c1 left, c2 right).
void drawGradient(sf::RenderTarget& target, float x, float y, float w, float h,
                  sf::Color c1, sf::Color c2, bool vertical) {
    sf::VertexArray strip(sf::PrimitiveType::TriangleStrip, 4);
    if (vertical) {
        strip[0].position = {x, y};         strip[0].color = c1;
        strip[1].position = {x + w, y};     strip[1].color = c1;
        strip[2].position = {x, y + h};     strip[2].color = c2;
        strip[3].position = {x + w, y + h}; strip[3].color = c2;
    } else {
        strip[0].position = {x, y};         strip[0].color = c1;
        strip[1].position = {x, y + h};     strip[1].color = c1;
        strip[2].position = {x + w, y};     strip[2].color = c2;
        strip[3].position = {x + w, y + h}; strip[3].color = c2;
    }
    target.draw(strip);
}

// Named arguments avoid the user_status / task_status mix-up that positional
// aggregate initialisation allows (member order is goal, proximity, USER, TASK).
EnvironmentObservation makeObservation(int goal, int proximity, int user, int task,
                                       float signal, bool food = false, bool play = false) {
    EnvironmentObservation o;
    o.goal_state = goal;
    o.target_proximity = proximity;
    o.user_status = user;
    o.task_status = task;
    o.signal_strength = signal;
    o.food_available = food;
    o.play_available = play;
    return o;
}

// Preset interaction buttons. Observations match the presets / text fallbacks
// that were already in the UI, so the policies see the same inputs as before.
struct ControlButton {
    const char* label;
    const char* stimulus;
    int goal, proximity, user, task;
    float signal;
    bool food, play;
    float reward;
    const char* sceneEvent;
    const char* basis;   // what proximity is measured to: user, ball, bowl, or "" for the preset value
};
const std::array<ControlButton, 10> kControls{{
    {"Greet",      "Greet the companion",                  0, 1, 0, 0, 0.55f, false, false,  0.65f, "",           "user"},
    {"Feed",       "Offer the companion food",             2, 0, 1, 0, 0.30f, true,  false,  0.50f, "feed",       "bowl"},
    {"Play",       "Invite the companion to play",         0, 1, 1, 0, 0.70f, false, true,   0.50f, "",           "user"},
    {"Praise",     "Praise the companion",                 0, 0, 1, 0, 0.75f, false, false,  1.00f, "",           "user"},
    {"Scold",      "Scold the companion",                  0, 1, 2, 0, 0.45f, false, false, -1.00f, "",           "user"},
    {"Explore",    "Let the companion explore",            2, 1, 0, 0, 0.30f, false, false,  0.25f, "",           "nearest"},
    {"Rest",       "The companion should rest",            2, 2, 0, 0, 0.20f, false, false,  0.25f, "",           "bed"},
    {"Loud noise", "A sudden loud noise occurs",           2, 0, 0, 0, 0.95f, false, false, -0.15f, "",           "nearest"},
    {"Fetch ball", "Ask the companion to fetch the ball",  1, 1, 1, 1, 0.85f, false, false,  0.85f, "fetch_ball", "ball"},
    {"Throw ball", "Throw the ball for the companion",     1, 1, 1, 1, 0.85f, false, false,  0.85f, "throw_ball", "ball"},
}};
}

CompanionUI::CompanionUI(const std::string& policyDirectoryValue,
                         const std::string& outputDirectoryValue,
                         bool researcherModeValue,
                         bool guidedStudyModeValue)
    : window(sf::VideoMode(sf::Vector2u(1280u, 900u)), "Animal Agent | Companion Simulation"),
      policyDirectory(policyDirectoryValue),
      outputDirectory(outputDirectoryValue),
      baseline(EnvironmentConfig{}),
      proposed(EnvironmentConfig{}) {
    researcherMode = researcherModeValue;
    guidedStudyMode = guidedStudyModeValue;
    baseline.setLearningEnabled(false);
    proposed.setLearningEnabled(false);
    baseline.setExplorationRate(0.0f);
    proposed.setExplorationRate(0.0f);
    const auto runTicks = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    runId = "run-" + std::to_string(runTicks);
    std::random_device conditionRandom;
    agentAIsProposed = (conditionRandom() % 2u) == 0u;
    if (!font.openFromFile("Data/Fonts/UIFont.ttf") &&
        !font.openFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")) {
        throw std::runtime_error("Unable to load DejaVuSans.ttf. Install DejaVu fonts or update companion-ui.cpp.");
    }
    window.setFramerateLimit(60);
    environmentTextureLoaded = environmentTexture.loadFromFile("Data/Assets/free version/free.png");
    if (environmentTextureLoaded) appendLog("Loaded backyard atlas (free.png)");
    auto loadFrames = [](std::vector<std::unique_ptr<sf::Texture>>& frames, const std::vector<std::string>& files) {
        for (const auto& file : files) {
            auto texture = std::make_unique<sf::Texture>();
            if (texture->loadFromFile("Data/Assets/white_dog_by_Brysia/" + file))
                frames.push_back(std::move(texture));
        }
    };
    loadFrames(idleFrames, {"idle1.png", "idle2.png", "idle3.png"});
    loadFrames(walkFrames, {"walk1.png", "walk2.png", "walk3.png", "walk11.png", "walk12.png", "walk13.png"});
    loadFrames(sitFrames, {"sit1.png", "sit2.png"});
    loadFrames(layFrames, {"lay1.png", "lay2.png"});
    loadFrames(sleepFrames, {"sleep1.png", "sleep2.png"});
    loadFrames(barkFrames, {"bark1.png", "bark2.png"});
    if (idleFrames.empty()) appendLog("Dog frames missing (Data/Assets/white_dog_by_Brysia); using a placeholder companion.");
    std::filesystem::create_directories(outputDirectory);
    std::filesystem::create_directories(policyDirectory);
    const auto basePath = std::filesystem::path(policyDirectory) / "baseline.qtable";
    const auto proposedPath = std::filesystem::path(policyDirectory) / "proposed.qtable";
    if (guidedStudyMode && (!std::filesystem::exists(basePath) || !std::filesystem::exists(proposedPath))) {
        throw std::runtime_error("Guided believability study requires trained baseline.qtable and proposed.qtable files in " + policyDirectory);
    }
    if (std::filesystem::exists(basePath)) baseline.loadPersistentState(basePath.string());
    if (std::filesystem::exists(proposedPath)) proposed.loadPersistentState(proposedPath.string());
    latestObservation = EnvironmentObservation{0, 1, 0, 0, 0.35f};
    resetScene();
    if (guidedStudyMode) {
        initialiseGuidedStudy();
    }
    appendLog("Simulation ready. Select an interaction or enter a short message.");
}

void CompanionUI::run() {
    while (window.isOpen()) {
        updateAnimation(frameClock.restart().asSeconds());
        while (const auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                saveSession();
                window.close();
            } else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Escape) {
                    if (!guidedStudyMode || studyPhase != "consent") saveSession();
                    window.close();
                } else if (guidedStudyMode && studyPhase == "consent" &&
                           (key->code == sf::Keyboard::Key::Y || key->code == sf::Keyboard::Key::Enter)) {
                    studyPhase = "warm_up";
                    studyStartedAt = utcTimestamp();
                    logStudyPhaseEvent("consent_accepted", "warm_up");
                    logStudyPhaseEvent("phase_started", "warm_up");
                    writeStudyManifest();
                } else if (guidedStudyMode && studyPhase == "consent" && key->code == sf::Keyboard::Key::N) {
                    studyPhase = "declined";
                    window.close();
                } else if (guidedStudyMode && studyPhase == "warm_up" &&
                           (key->code == sf::Keyboard::Key::Enter || key->code == sf::Keyboard::Key::Space)) {
                    studyPhase = "guided";
                    logStudyPhaseEvent("phase_ended", "warm_up");
                    logStudyPhaseEvent("phase_started", "guided");
                    writeStudyManifest();
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

CompanionUI::SceneLayout CompanionUI::sceneLayout() const {
    SceneLayout L;
    L.x = kSceneX;
    L.y = kSceneY;
    L.w = kSceneW;
    L.h = controlsVisible ? kSceneHControls : kSceneHFull;
    // Feet stay inside this rectangle; the band above it is the house, tree and hedge.
    L.minX = L.x + 70.0f;
    L.maxX = L.x + L.w - 70.0f;
    L.minY = L.y + L.h * 0.50f;
    L.maxY = L.y + L.h - 26.0f;
    L.bedX = L.x + L.w * 0.14f;
    L.bedY = L.y + L.h * 0.76f;
    L.bowlX = L.x + L.w * 0.74f;
    L.bowlY = L.y + L.h * 0.68f;
    L.ballHomeX = L.x + L.w * 0.50f;
    L.ballHomeY = L.y + L.h * 0.86f;
    L.treeX = L.x + 330.0f;
    L.houseX = L.x + L.w - 260.0f;
    return L;
}

std::vector<CompanionUI::SceneObject> CompanionUI::sceneObjects() const {
    const SceneLayout L = sceneLayout();
    std::vector<SceneObject> objects;
    objects.push_back(SceneObject{"House", sf::FloatRect({L.houseX, L.minY - 16.0f - 168.0f}, {147.0f, 168.0f})});
    objects.push_back(SceneObject{"Tree", sf::FloatRect({L.treeX, L.minY - 8.0f - 102.0f}, {102.0f, 102.0f})});
    objects.push_back(SceneObject{"Dog bed", sf::FloatRect({L.bedX - 62.0f, L.bedY - 26.0f}, {124.0f, 44.0f})});
    objects.push_back(SceneObject{foodPresent ? "Food bowl (full)" : "Food bowl (empty)",
                                  sf::FloatRect({L.bowlX - 30.0f, L.bowlY - 20.0f}, {60.0f, 34.0f})});
    objects.push_back(SceneObject{"You", sf::FloatRect({userX - 22.0f, userY - 100.0f}, {44.0f, 100.0f})});
    objects.push_back(SceneObject{"Companion", sf::FloatRect({agentX - 36.0f, agentY - 72.0f}, {72.0f, 72.0f})});
    if (!ballCarried)
        objects.push_back(SceneObject{"Ball", sf::FloatRect({ballX - 15.0f, ballY - ballHeight - 26.0f}, {30.0f, 30.0f})});
    return objects;
}

void CompanionUI::resetScene() {
    const SceneLayout L = sceneLayout();
    behaviourPlan.clear();
    stepIndex = 0;
    stepTime = 0.0f;
    behaviourActive = false;
    restingPose = "idle";
    animationFrame = 0;
    frameTimer = 0.0f;
    userX = L.x + L.w * 0.30f;
    userY = L.y + L.h * 0.82f;
    playerFacingRight = true;
    playerMoving = false;
    agentX = L.x + L.w * 0.58f;
    agentY = L.y + L.h * 0.70f;
    facingRight = false;
    ballX = L.ballHomeX;
    ballY = L.ballHomeY;
    ballLandX = ballX;
    ballLandY = ballY;
    ballHeight = 0.0f;
    ballCarried = false;
    ballFlying = false;
    throwTimer = 0.0f;
    foodPresent = false;
    pendingCue.clear();
    autonomousDecisionTimer = 0.0f;
    interactionBuffer.clear();
}

const std::vector<std::unique_ptr<sf::Texture>>& CompanionUI::framesForPose(const std::string& pose) const {
    if (pose == "walk") return walkFrames;
    if (pose == "sit") return sitFrames;
    if (pose == "lay") return layFrames;
    if (pose == "sleep") return sleepFrames;
    if (pose == "bark") return barkFrames;
    return idleFrames;
}

void CompanionUI::startNewParticipant() {
    // Strict alternation keeps the A/B <-> baseline/proposed mapping balanced across participants.
    agentAIsProposed = !agentAIsProposed;
    const auto runTicks = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    runId = "run-" + std::to_string(runTicks);
    proposed.clearMemory();
    std::error_code ec;
    std::filesystem::remove(std::filesystem::path(policyDirectory) / "proposed.qtable.memory.json", ec);
    eventLog.clear();
    baselineRewards.clear();
    proposedRewards.clear();
    interactionCount = 0;
    lastActionInteractionId = 0;
    latestStimulus = "Welcome. Choose an interaction to begin.";
    latestBaselineAction = "IDLE";
    latestProposedAction = "IDLE";
    textInput.clear();
    selectedAgentCode = 'A';
    resetScene();
    appendLog(std::string("New participant: memory cleared. Mapping A = ") +
              (agentAIsProposed ? "proposed" : "baseline") + ".");
}

void CompanionUI::initialiseGuidedStudy() {
    const auto counterPath = std::filesystem::path(outputDirectory) / "study-participant-counter.txt";
    std::filesystem::create_directories(outputDirectory);
    int counter = 0;
    {
        std::ifstream input(counterPath);
        if (input) input >> counter;
    }
    ++counter;
    {
        std::ofstream output(counterPath, std::ios::trunc);
        if (output) output << counter << '\n';
    }
    studyParticipantCounter = counter;
    studyParticipantCode = "P" + (counter < 10 ? std::string("0") : std::string()) + std::to_string(counter);
    // Strict alternation: odd participants see A=baseline, even participants A=proposed.
    agentAIsProposed = (counter % 2) == 0;
    studySessionIndex = 1;
    studyStepIndex = 0;
    studyPhase = "consent";
    studyStartedAt = utcTimestamp();
    studyIdleElapsed = 0.0f;
    studyFreeElapsed = 0.0f;
    studyGuidedComplete = false;
    studyFreeFinished = false;
    studyFormOpenedCurrentCondition = false;
    studyFinished = false;
    selectedAgentCode = 'A';
    selectedAgentIsProposed = agentAIsProposed;
    const auto ticks = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    runId = "study-" + studyParticipantCode + "-" + std::to_string(ticks);
    baseline.setLearningEnabled(false);
    proposed.setLearningEnabled(false);
    baseline.setExplorationRate(0.0f);
    proposed.setExplorationRate(0.0f);
    proposed.resetForStudy();
    resetScene();
    screen = Screen::Simulation;
}

std::string CompanionUI::currentStudyPrompt() const {
    static const std::array<std::string, 12> prompts = {
        "1/12: Walk close to the companion and press Greet.",
        "2/12: Press Praise.",
        "3/12: Press Throw ball and watch.",
        "4/12: Press Feed.",
        "5/12: Press Loud noise.",
        "6/12: Type: I like the ball. Then press Enter.",
        "7/12: Press Throw ball again.",
        "8/12: Press Scold.",
        "9/12: Stay still for 30 seconds and watch the companion.",
        "10/12: Type: I hate loud noises. Then press Enter.",
        "11/12: Press Loud noise again.",
        "12/12: Press Rest."
    };
    if (studyStepIndex < 0 || studyStepIndex >= static_cast<int>(prompts.size())) return {};
    return prompts[static_cast<std::size_t>(studyStepIndex)];
}

bool CompanionUI::studyInputMatches(const std::string& interaction, bool isText) const {
    if (!guidedStudyMode || studyPhase != "guided") return false;
    std::string lower = interaction;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    switch (studyStepIndex) {
        case 0: return !isText && lower.find("greet") != std::string::npos;
        case 1: return !isText && lower.find("praise") != std::string::npos;
        case 2: return !isText && lower.find("throw") != std::string::npos && lower.find("ball") != std::string::npos;
        case 3: return !isText && lower.find("feed") != std::string::npos;
        case 4: return !isText && lower.find("loud noise") != std::string::npos;
        case 5: return isText && lower.find("i like the ball") != std::string::npos;
        case 6: return !isText && lower.find("throw") != std::string::npos && lower.find("ball") != std::string::npos;
        case 7: return !isText && lower.find("scold") != std::string::npos;
        case 8: return false; // completed by the timed observation window
        case 9: return isText && lower.find("i hate loud noises") != std::string::npos;
        case 10: return !isText && lower.find("loud noise") != std::string::npos;
        case 11: return !isText && lower.find("rest") != std::string::npos;
        default: return false;
    }
}

void CompanionUI::writeStudyManifest() {
    if (!guidedStudyMode) return;
    nlohmann::json manifest = {
        {"schema_version", 1},
        {"protocol_id", "companion-believability-v0.1"},
        {"participant_code", studyParticipantCode},
        {"run_id", runId},
        {"session_index", studySessionIndex},
        {"condition_code", std::string(1, selectedAgentCode)},
        {"condition", selectedAgentIsProposed ? "proposed" : "baseline"},
        {"condition_key", {{"A", agentAIsProposed ? "proposed" : "baseline"},
                           {"B", agentAIsProposed ? "baseline" : "proposed"}}},
        {"phase", studyPhase},
        {"software_revision", IS2_GIT_REVISION},
        {"started_at", studyStartedAt},
        {"policy_checksums", {{"baseline", fileChecksum(std::filesystem::path(policyDirectory) / "baseline.qtable")},
                              {"proposed", fileChecksum(std::filesystem::path(policyDirectory) / "proposed.qtable")}}},
        {"policy_updates_enabled", false},
        {"exploration_rate", 0.0},
        {"memory_initialisation", "clean proposed memory per participant; no cross-participant memory"},
        {"guided_script_version", 1},
        {"guided_step_count", 12}
    };
    const auto manifestPath = std::filesystem::path(outputDirectory) /
        (runId + "-condition-" + std::string(1, selectedAgentCode) + "-manifest.json");
    std::ofstream out(manifestPath);
    if (out) out << manifest.dump(2) << '\n';
}

void CompanionUI::logStudyPhaseEvent(const std::string& eventType, const std::string& phase) {
    if (!guidedStudyMode) return;
    nlohmann::json row = {
        {"schema_version", 1}, {"protocol_id", "companion-believability-v0.1"},
        {"event_type", eventType}, {"run_id", runId},
        {"participant_code", studyParticipantCode}, {"session_index", studySessionIndex},
        {"condition_code", std::string(1, selectedAgentCode)},
        {"condition", selectedAgentIsProposed ? "proposed" : "baseline"},
        {"phase", phase}, {"timestamp", utcTimestamp()},
        {"software_revision", IS2_GIT_REVISION}
    };
    std::ofstream log(std::filesystem::path(outputDirectory) / (runId + "-steps.jsonl"), std::ios::app);
    if (log) log << row.dump() << '\n';
}

void CompanionUI::completeStudyStep(const std::string& interaction) {
    if (!guidedStudyMode || studyPhase != "guided" || studyStepIndex >= 12) return;
    nlohmann::json row = {
        {"schema_version", 1},
        {"protocol_id", "companion-believability-v0.1"},
        {"run_id", runId},
        {"participant_code", studyParticipantCode},
        {"session_index", studySessionIndex},
        {"condition_code", std::string(1, selectedAgentCode)},
        {"condition", selectedAgentIsProposed ? "proposed" : "baseline"},
        {"phase", studyPhase},
        {"step_id", studyStepIndex + 1},
        {"step_prompt", currentStudyPrompt()},
        {"interaction", interaction},
        {"selected_action", (studyStepIndex == 5 || studyStepIndex == 8 || studyStepIndex == 9)
                                ? "NO_DIRECT_ACTION"
                                : (selectedAgentIsProposed ? latestProposedAction : latestBaselineAction)},
        {"memory_count_after", selectedAgentIsProposed ? proposed.getMemoryRecords().size() : 0},
        {"affect_after", selectedAgentIsProposed ? nlohmann::json({
            {"valence", static_cast<int>(proposed.getAffectiveState().valence)},
            {"stress", proposed.getAffectiveState().stress},
            {"arousal", proposed.getAffectiveState().arousal},
            {"engagement", proposed.getAffectiveState().engagement},
            {"affinity", proposed.getAffectiveState().affinity}
        }) : nlohmann::json(nullptr)},
        {"timestamp", utcTimestamp()}
    };
    std::ofstream log(std::filesystem::path(outputDirectory) / (runId + "-steps.jsonl"), std::ios::app);
    if (log) log << row.dump() << '\n';
    if (studyStepIndex + 1 == 8) studyIdleElapsed = 0.0f;
    ++studyStepIndex;
    if (studyStepIndex >= 12) {
        studyGuidedComplete = true;
        studyPhase = "free";
        studyFreeElapsed = 0.0f;
        logStudyPhaseEvent("phase_started", "free");
    }
    writeStudyManifest();
}

void CompanionUI::openStudyForm() {
    if (guidedStudyMode && (studyPhase != "rating" || studyFormOpenedCurrentCondition)) {
        appendLog("The form opens only after the guided and free-interaction phases.");
        return;
    }
    const char* configuredUrl = std::getenv("ANIMAL_AGENT_STUDY_FORM_URL");
    const char* sessionEntry = std::getenv("ANIMAL_AGENT_FORM_ENTRY_SESSION");
    const char* conditionEntry = std::getenv("ANIMAL_AGENT_FORM_ENTRY_CONDITION");
    const char* runEntry = std::getenv("ANIMAL_AGENT_FORM_ENTRY_RUN");
    if (!configuredUrl || !*configuredUrl || !sessionEntry || !*sessionEntry ||
        !conditionEntry || !*conditionEntry || !runEntry || !*runEntry) {
        appendLog("Google Form configuration missing. Set ANIMAL_AGENT_STUDY_FORM_URL and the three ANIMAL_AGENT_FORM_ENTRY_* IDs.");
        return;
    }
    const std::string baseUrl(configuredUrl);
    const std::string separator = baseUrl.find('?') == std::string::npos ? "?" : "&";
    const std::string url = baseUrl + separator +
        "entry." + sessionEntry + "=" + std::to_string(studySessionIndex) +
        "&entry." + conditionEntry + "=" + std::string(1, selectedAgentCode) +
        "&entry." + runEntry + "=" + runId;
    {
        nlohmann::json row = {
            {"schema_version", 1}, {"event", "study_form_opened"},
            {"protocol_id", "companion-believability-v0.1"}, {"run_id", runId},
            {"participant_code", studyParticipantCode}, {"session_index", studySessionIndex},
            {"condition_code", std::string(1, selectedAgentCode)},
            {"condition", selectedAgentIsProposed ? "proposed" : "baseline"},
            {"timestamp", utcTimestamp()}, {"phase", studyPhase}
        };
        std::ofstream log(std::filesystem::path(outputDirectory) / (runId + "-steps.jsonl"), std::ios::app);
        if (log) log << row.dump() << '\n';
    }
    const pid_t browser = fork();
    if (browser == 0) {
        execlp("xdg-open", "xdg-open", url.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
    if (browser < 0) {
        appendLog("Could not start browser for the study form.");
        return;
    }
    if (guidedStudyMode) studyFormOpenedCurrentCondition = true;
    appendLog("Opened Google Form for Companion " + std::string(1, selectedAgentCode));
}

void CompanionUI::applySceneEvent(const std::string& sceneEvent) {
    const SceneLayout L = sceneLayout();
    if (sceneEvent == "feed") {
        foodPresent = true;
    } else if (sceneEvent == "fetch_ball") {
        // The user puts the ball down in the yard.
        ballCarried = false;
        ballFlying = false;
        ballHeight = 0.0f;
        ballX = L.ballHomeX;
        ballY = L.ballHomeY;
        ballLandX = ballX;
        ballLandY = ballY;
    } else if (sceneEvent == "throw_ball") {
        ballCarried = false;
        ballStartX = userX + (playerFacingRight ? 26.0f : -26.0f);
        ballStartY = userY;
        if (guidedStudyMode) {
            // The two fetch probes use the same absolute landing point in both conditions.
            ballLandX = std::clamp(L.ballHomeX + L.w * 0.18f, L.minX + 10.0f, L.maxX - 10.0f);
            ballLandY = L.minY + (L.maxY - L.minY) * 0.62f;
        } else {
            std::uniform_real_distribution<float> distance(240.0f, 520.0f);
            std::uniform_real_distribution<float> lane(L.minY + 20.0f, L.maxY - 10.0f);
            const float dir = playerFacingRight ? 1.0f : -1.0f;
            ballLandX = std::clamp(userX + dir * distance(sceneRng), L.minX + 10.0f, L.maxX - 10.0f);
            ballLandY = lane(sceneRng);
        }
        ballX = ballStartX;
        ballY = ballStartY;
        ballHeight = 85.0f;
        ballFlightTime = 0.0f;
        ballFlying = true;
        throwTimer = 0.5f;
    }
}

// Maps a real distance in the scene onto the existing three-level proximity
// classes (close / medium / far) used by the policies.
void CompanionUI::applyGeometryProximity(EnvironmentObservation& observation, const std::string& basis) const {
    const SceneLayout L = sceneLayout();

    struct Target {
        const char* name;
        float x;
        float y;
    };
    std::array<Target, 4> targets{{
        {"user", userX, userY},
        {"bowl", L.bowlX, L.bowlY},
        {"bed", L.bedX, L.bedY},
        {"ball", ballFlying ? ballLandX : ballX, ballFlying ? ballLandY : ballY}
    }};

    // A carried ball is not a separate reachable target.
    const bool ballIsAvailable = !ballCarried;
    const Target* selected = nullptr;
    if (!basis.empty() && basis != "nearest") {
        for (const auto& target : targets) {
            if (basis == target.name && (basis != "ball" || ballIsAvailable)) {
                selected = &target;
                break;
            }
        }
    }

    // Empty/unknown targets and carried-ball requests fall back to the nearest
    // relevant scene target instead of leaving a theoretical/stale value in place.
    if (!selected) {
        float nearestDistance = std::numeric_limits<float>::max();
        for (const auto& target : targets) {
            if (std::string(target.name) == "ball" && !ballIsAvailable) continue;
            const float distance = std::hypot(agentX - target.x, agentY - target.y);
            if (distance < nearestDistance) {
                nearestDistance = distance;
                selected = &target;
            }
        }
    }

    if (selected) {
        const float distance = std::hypot(agentX - selected->x, agentY - selected->y);
        observation.target_proximity = proximityBucket(distance, L.w);
    }
}

void CompanionUI::beginStep() {
    BehaviourStep& s = behaviourPlan[stepIndex];
    stepTime = 0.0f;
    animationFrame = 0;
    frameTimer = 0.0f;
    if (s.faceDir != 0) facingRight = s.faceDir > 0;
    if (s.trackUser) s.trackSide = (agentX >= userX) ? 1 : -1;   // choose the side the dog is on NOW
    if (s.pickUpBall) {
        ballCarried = true;
        ballFlying = false;
        ballHeight = 0.0f;
    }
    if (s.dropBall) {
        ballCarried = false;
        ballX = agentX + (facingRight ? kMouthOffset : -kMouthOffset);
        ballY = agentY;
        ballLandX = ballX;
        ballLandY = ballY;
        ballHeight = 0.0f;
    }
}

void CompanionUI::finishStep() {
    if (behaviourPlan[stepIndex].consumeFood) foodPresent = false;
    ++stepIndex;
    if (stepIndex >= behaviourPlan.size()) {
        behaviourActive = false;
        behaviourPlan.clear();
        stepIndex = 0;
    } else {
        beginStep();
    }
}

void CompanionUI::startBehaviour(const std::string& action, const std::string& sceneEvent) {
    const SceneLayout L = sceneLayout();
    behaviourPlan.clear();
    stepIndex = 0;
    restingPose = (action == "SLEEP_LAY") ? "sleep" : "idle";

    auto walkTo = [&](float x, float y, float speed, float frameInterval) {
        BehaviourStep s;
        s.pose = "walk";
        s.hasTarget = true;
        s.targetX = std::clamp(x, L.minX, L.maxX);
        s.targetY = std::clamp(y, L.minY, L.maxY);
        s.speed = speed;
        s.frameInterval = frameInterval;
        return s;
    };
    // Walk to the player and keep following while the player moves.
    auto toPlayer = [&](float offset, float speed, float frameInterval) {
        BehaviourStep s = walkTo(userX, userY, speed, frameInterval);
        s.trackUser = true;
        s.userOffsetX = offset;
        s.trackSide = (agentX >= userX) ? 1 : -1;
        return s;
    };
    auto stay = [&](const std::string& pose, float seconds, const std::string& cue) {
        BehaviourStep s;
        s.pose = pose;
        s.duration = seconds;
        s.cue = cue;
        return s;
    };

    if (action == "FETCH") {
        const float ballTx = ballFlying ? ballLandX : ballX;
        const float ballTy = ballFlying ? ballLandY : ballY;
        if (sceneEvent == "throw_ball") {
            BehaviourStep watch = stay("idle", 0.55f, "!");   // watch the ball leave the hand
            watch.faceDir = playerFacingRight ? 1 : -1;
            behaviourPlan.push_back(watch);
        }
        const float mouth = (ballTx >= agentX) ? -kMouthOffset : kMouthOffset;   // stop so the mouth reaches the ball
        behaviourPlan.push_back(walkTo(ballTx + mouth, ballTy, 300.0f, 0.08f));
        BehaviourStep pick = stay("idle", 0.35f, "");
        pick.pickUpBall = true;
        behaviourPlan.push_back(pick);
        behaviourPlan.push_back(toPlayer(70.0f, 240.0f, 0.09f));     // carry it back to wherever the player is now
        BehaviourStep drop = stay("sit", 1.0f, "Got it!");           // ball is placed at the player's feet
        drop.dropBall = true;
        drop.faceUser = true;
        behaviourPlan.push_back(drop);
    } else if (action == "SNIFF") {
        const float ballDistance = std::hypot(agentX - ballX, agentY - ballY);
        const float bowlDistance = std::hypot(agentX - L.bowlX, agentY - L.bowlY);
        const bool ballCloser = !ballCarried && ballDistance <= bowlDistance;
        const float tx = ballCloser ? ballX : L.bowlX;
        const float ty = ballCloser ? ballY : L.bowlY;
        const float standX = (tx >= agentX) ? tx - 50.0f : tx + 50.0f;
        BehaviourStep w = walkTo(standX, ty, 70.0f, 0.22f);
        w.tiltBias = 6.0f;
        behaviourPlan.push_back(w);
        BehaviourStep s = stay("idle", 1.2f, "sniff");
        s.tiltBias = 9.0f;
        s.tilt = 3.0f;
        s.tiltRate = 2.0f;
        s.faceDir = (tx >= standX) ? 1 : -1;
        behaviourPlan.push_back(s);
    } else if (action == "WANDER") {
        std::uniform_real_distribution<float> px(L.minX, L.maxX);
        std::uniform_real_distribution<float> py(L.minY, L.maxY);
        float tx = px(sceneRng), ty = py(sceneRng);
        for (int attempt = 0; attempt < 6 && std::hypot(tx - agentX, ty - agentY) < 140.0f; ++attempt) {
            tx = px(sceneRng);
            ty = py(sceneRng);
        }
        behaviourPlan.push_back(walkTo(tx, ty, 85.0f, 0.18f));
        BehaviourStep look = stay("idle", 0.7f, "");
        look.tilt = 5.0f;
        look.tiltRate = 1.0f;
        behaviourPlan.push_back(look);
    } else if (action == "APPROACH_FOLLOW") {
        behaviourPlan.push_back(toPlayer(110.0f, 150.0f, 0.12f));
        BehaviourStep s = stay("idle", 0.5f, "");
        s.faceUser = true;
        behaviourPlan.push_back(s);
    } else if (action == "HIDE_AVOID") {
        // Run to the corner of the yard that is farthest from the player.
        const float cornersX[4] = {L.minX + 20.0f, L.maxX - 20.0f, L.minX + 20.0f, L.maxX - 20.0f};
        const float cornersY[4] = {L.minY + 10.0f, L.minY + 10.0f, L.maxY - 10.0f, L.maxY - 10.0f};
        int best = 0;
        float bestDistance = -1.0f;
        for (int i = 0; i < 4; ++i) {
            const float d = std::hypot(cornersX[i] - userX, cornersY[i] - userY);
            if (d > bestDistance) { bestDistance = d; best = i; }
        }
        BehaviourStep run = walkTo(cornersX[best], cornersY[best], 210.0f, 0.09f);
        run.crouch = 0.10f;
        run.tint = sf::Color(205, 205, 225);
        behaviourPlan.push_back(run);
        BehaviourStep cower = stay("sit", 1.4f, "...");
        cower.shiver = 2.0f;
        cower.crouch = 0.12f;
        cower.tint = sf::Color(205, 205, 225);
        cower.faceUser = true;
        behaviourPlan.push_back(cower);
    } else if (action == "WHINE") {
        BehaviourStep s = stay("sit", 1.7f, "*whine*");
        s.shiver = 1.5f;
        s.crouch = 0.08f;
        s.tint = sf::Color(215, 215, 235);
        s.faceUser = true;
        behaviourPlan.push_back(s);
    } else if (action == "BARK") {
        BehaviourStep s = stay("bark", 1.1f, "WOOF!");
        s.frameInterval = 0.12f;
        s.hop = 12.0f;
        s.hopRate = 3.5f;
        behaviourPlan.push_back(s);
    } else if (action == "INSIST") {
        behaviourPlan.push_back(toPlayer(95.0f, 200.0f, 0.10f));
        BehaviourStep s = stay("bark", 1.7f, "!!");
        s.frameInterval = 0.14f;
        s.hop = 18.0f;
        s.hopRate = 2.5f;
        s.faceUser = true;
        behaviourPlan.push_back(s);
    } else if (action == "ACT_CONFUSED") {
        BehaviourStep s = stay("idle", 1.5f, "?");
        s.tilt = 9.0f;
        s.tiltRate = 0.9f;
        behaviourPlan.push_back(s);
    } else if (action == "GREETING") {
        behaviourPlan.push_back(toPlayer(100.0f, 170.0f, 0.12f));
        BehaviourStep s = stay("sit", 0.9f, "Hi!");
        s.hop = 7.0f;
        s.hopRate = 2.5f;
        s.faceUser = true;
        behaviourPlan.push_back(s);
    } else if (action == "NUZZLE") {
        behaviourPlan.push_back(toPlayer(60.0f, 150.0f, 0.14f));
        BehaviourStep s = stay("sit", 1.2f, "<3");
        s.tilt = 5.0f;
        s.tiltRate = 1.0f;
        s.faceUser = true;
        behaviourPlan.push_back(s);
    } else if (action == "PLAY") {
        behaviourPlan.push_back(toPlayer(130.0f, 210.0f, 0.09f));
        BehaviourStep s = stay("idle", 2.0f, "yay!");
        s.hop = 24.0f;
        s.hopRate = 2.0f;
        s.faceUser = true;
        behaviourPlan.push_back(s);
    } else if (action == "EAT") {
        behaviourPlan.push_back(walkTo(L.bowlX - 52.0f, L.bowlY + 4.0f, 130.0f, 0.14f));
        BehaviourStep eat = stay("idle", 2.0f, "yum!");
        eat.faceDir = 1;
        eat.tiltBias = 13.0f;
        eat.tilt = 4.0f;
        eat.tiltRate = 3.0f;
        eat.consumeFood = true;
        behaviourPlan.push_back(eat);
    } else if (action == "SLEEP_LAY") {
        behaviourPlan.push_back(walkTo(L.bedX, L.bedY + 2.0f, 100.0f, 0.20f));
        behaviourPlan.push_back(stay("lay", 0.6f, ""));
        BehaviourStep z = stay("sleep", 2.6f, "Zzz");
        z.frameInterval = 0.9f;
        behaviourPlan.push_back(z);
    } else {
        behaviourPlan.push_back(stay("idle", 0.8f, ""));
    }
    behaviourActive = true;
    stepIndex = 0;
    beginStep();
}

void CompanionUI::updatePlayer(float elapsedSeconds) {
    playerMoving = false;
    if (screen != Screen::Simulation || textInputFocused || !window.hasFocus()) return;
    auto held = [](sf::Keyboard::Key a, sf::Keyboard::Key b) {
        return sf::Keyboard::isKeyPressed(a) || sf::Keyboard::isKeyPressed(b);
    };
    float dx = 0.0f, dy = 0.0f;
    if (held(sf::Keyboard::Key::W, sf::Keyboard::Key::Up)) dy -= 1.0f;
    if (held(sf::Keyboard::Key::S, sf::Keyboard::Key::Down)) dy += 1.0f;
    if (held(sf::Keyboard::Key::A, sf::Keyboard::Key::Left)) dx -= 1.0f;
    if (held(sf::Keyboard::Key::D, sf::Keyboard::Key::Right)) dx += 1.0f;
    if (dx == 0.0f && dy == 0.0f) return;
    const float length = std::hypot(dx, dy);
    dx /= length;
    dy /= length;
    const SceneLayout L = sceneLayout();
    userX = std::clamp(userX + dx * kPlayerSpeed * elapsedSeconds, L.minX, L.maxX);
    userY = std::clamp(userY + dy * kPlayerSpeed * elapsedSeconds, L.minY, L.maxY);
    if (dx != 0.0f) playerFacingRight = dx > 0.0f;
    playerMoving = true;
}

void CompanionUI::updateAnimation(float elapsedSeconds) {
    elapsedSeconds = std::min(elapsedSeconds, 0.1f);   // avoid jumps after the window is dragged
    sceneTime += elapsedSeconds;
    proposed.advanceTime(elapsedSeconds);
    if (throwTimer > 0.0f) throwTimer = std::max(0.0f, throwTimer - elapsedSeconds);
    updatePlayer(elapsedSeconds);
    if (guidedStudyMode && studyPhase == "guided" && studyStepIndex == 8 &&
        !behaviourActive && interactionBuffer.empty()) {
        studyIdleElapsed += elapsedSeconds;
        if (studyIdleElapsed >= 30.0f) completeStudyStep("30-second idle observation completed");
    }
    if (guidedStudyMode && studyPhase == "free") {
        studyFreeElapsed += elapsedSeconds;
        if (studyFreeElapsed >= 180.0f) {
            studyFreeFinished = true;
            studyPhase = "rating";
            logStudyPhaseEvent("phase_ended", "free");
            logStudyPhaseEvent("phase_started", "rating");
            writeStudyManifest();
        }
    }
    const SceneLayout L = sceneLayout();

    if (ballFlying) {
        ballFlightTime += elapsedSeconds;
        const float p = std::min(1.0f, ballFlightTime / 0.85f);
        ballX = ballStartX + (ballLandX - ballStartX) * p;
        ballY = ballStartY + (ballLandY - ballStartY) * p;
        ballHeight = 85.0f * (1.0f - p) + 120.0f * 4.0f * p * (1.0f - p);
        if (p >= 1.0f) {
            ballFlying = false;
            ballX = ballLandX;
            ballY = ballLandY;
            ballHeight = 0.0f;
        }
    }

    if (behaviourActive && stepIndex < behaviourPlan.size()) {
        BehaviourStep& s = behaviourPlan[stepIndex];
        stepTime += elapsedSeconds;
        frameTimer += elapsedSeconds;
        if (frameTimer >= s.frameInterval) {
            frameTimer = 0.0f;
            ++animationFrame;
        }
        bool done = false;
        if (s.hasTarget) {
            float tx = s.targetX, ty = s.targetY;
            if (s.trackUser) {
                tx = std::clamp(userX + static_cast<float>(s.trackSide) * s.userOffsetX, L.minX, L.maxX);
                ty = std::clamp(userY + 4.0f, L.minY, L.maxY);
            }
            const float dx = tx - agentX, dy = ty - agentY;
            const float distance = std::hypot(dx, dy);
            if (s.faceDir == 0 && !s.faceUser && std::fabs(dx) > 1.0f) facingRight = dx > 0.0f;
            const float move = s.speed * elapsedSeconds;
            if (distance <= move) {
                agentX = tx;
                agentY = ty;
                done = true;
            } else {
                agentX += dx / distance * move;
                agentY += dy / distance * move;
            }
            if (stepTime > 10.0f) done = true;   // safety net
        } else if (stepTime >= s.duration) {
            done = true;
        }
        if (s.faceUser) facingRight = userX > agentX;
        if (done) finishStep();   // s may be invalid after this call
    }

    if (!behaviourActive && !interactionBuffer.empty()) {
        const auto next = interactionBuffer.front();
        interactionBuffer.pop_front();
        autonomousDecisionTimer = 0.0f;
        // Apply the scene change first, then derive proximity from the real positions.
        applySceneEvent(next.sceneEvent);
        EnvironmentObservation observation = next.observation;
        applyGeometryProximity(observation, next.proximityBasis);
        executeInteraction(next.label, observation, next.reward, next.preference,
                           next.preferencePositive, next.sceneEvent);
    } else if (!behaviourActive && interactionBuffer.empty()) {
        // The environment continues to evolve when the user is idle. Refresh
        // observable affordances and let both policies choose from that context;
        // only the selected A/B condition is rendered. No synthetic user reward
        // or memory event is recorded for this autonomous decision.
        autonomousDecisionTimer += elapsedSeconds;
        const bool autonomousAllowed = !guidedStudyMode || studyPhase == "free" ||
            (studyPhase == "guided" && studyStepIndex == 8);
        if (autonomousDecisionTimer >= autonomousDecisionInterval &&
            screen == Screen::Simulation && autonomousAllowed) {
            autonomousDecisionTimer = 0.0f;
            EnvironmentObservation ambient = latestObservation;
            ambient.goal_state = 2;       // no new user request: ambient context
            ambient.user_status = 0;
            ambient.task_status = 0;
            ambient.signal_strength = 0.0f;
            ambient.food_available = foodPresent;
            ambient.play_available = !ballCarried && !ballFlying;
            applyGeometryProximity(ambient, "nearest");

            if (guidedStudyMode) {
                latestBaselineAction = selectedAgentIsProposed ? "NOT_RUN" : baseline.selectAction(ambient);
                latestProposedAction = selectedAgentIsProposed ? proposed.selectAction(ambient) : "NOT_RUN";
            } else {
                latestBaselineAction = baseline.selectAction(ambient);
                latestProposedAction = proposed.selectAction(ambient);
            }
            latestObservation = ambient;
            latestStimulus = "The companion responds to its surroundings.";
            const std::string selectedAction = selectedAgentIsProposed ? latestProposedAction : latestBaselineAction;
            startBehaviour(selectedAction, "");
            if (guidedStudyMode) {
                nlohmann::json row = {
                    {"schema_version", 1}, {"protocol_id", "companion-believability-v0.1"},
                    {"run_id", runId}, {"participant_code", studyParticipantCode},
                    {"session_index", studySessionIndex}, {"condition_code", std::string(1, selectedAgentCode)},
                    {"condition", selectedAgentIsProposed ? "proposed" : "baseline"},
                    {"phase", studyPhase}, {"step_id", studyStepIndex + 1},
                    {"event_type", "autonomous_action"}, {"selected_action", selectedAction},
                    {"observation", {{"goal_state", ambient.goal_state},
                                     {"target_proximity", ambient.target_proximity},
                                     {"user_status", ambient.user_status},
                                     {"task_status", ambient.task_status},
                                     {"signal_strength", ambient.signal_strength}}},
                    {"timestamp", utcTimestamp()}, {"software_revision", IS2_GIT_REVISION},
                    {"policy_updates_enabled", false}
                };
                std::ofstream log(std::filesystem::path(outputDirectory) / ("interaction-events-" + runId + ".jsonl"), std::ios::app);
                if (log) log << row.dump() << '\n';
            }
            if (researcherMode) appendLog("Autonomous environment-driven action: " + selectedAction);
        }
    }
}

void CompanionUI::drawBubble(const std::string& value, float x, float bottomY) {
    sf::Text label(font, value, 20);
    label.setFillColor(kInk);
    const sf::FloatRect bounds = label.getLocalBounds();
    const float padX = 13.0f, padY = 7.0f;
    const float w = bounds.size.x + padX * 2.0f;
    const float h = bounds.size.y + padY * 2.0f;
    const float top = bottomY - h - 10.0f;
    drawRoundedBox(window, x - w * 0.5f, top + 3.0f, w, h, 12.0f, kShadowSoft);
    drawRoundedBox(window, x - w * 0.5f, top, w, h, 12.0f, kCream, sf::Color(196, 150, 112), 2.0f);
    sf::ConvexShape tail;
    tail.setPointCount(3);
    tail.setPoint(0, {x - 7.0f, top + h - 1.0f});
    tail.setPoint(1, {x + 7.0f, top + h - 1.0f});
    tail.setPoint(2, {x, bottomY});
    tail.setFillColor(kCream);
    window.draw(tail);
    label.setPosition({x - w * 0.5f + padX - bounds.position.x, top + padY - bounds.position.y});
    window.draw(label);
}

void CompanionUI::drawCompanion(float x, float y) {
    const BehaviourStep* step =
        (behaviourActive && stepIndex < behaviourPlan.size()) ? &behaviourPlan[stepIndex] : nullptr;
    const std::string pose = step ? step->pose : restingPose;
    const auto& frames = framesForPose(pose);
    pendingCue.clear();

    sf::CircleShape shadow(30.0f);
    shadow.setOrigin({30.0f, 30.0f});
    shadow.setScale({1.0f, 0.28f});
    shadow.setPosition({x, y + 2.0f});
    shadow.setFillColor(kShadow);
    window.draw(shadow);

    if (frames.empty()) {
        sf::CircleShape placeholder(24.0f);
        placeholder.setOrigin({24.0f, 48.0f});
        placeholder.setPosition({x, y});
        placeholder.setFillColor(kAccent);
        window.draw(placeholder);
        return;
    }

    float hopOffset = 0.0f, angle = 0.0f, shiver = 0.0f, crouch = 0.0f;
    sf::Color tint = sf::Color::White;
    if (step) {
        if (step->hop > 0.0f && step->hopRate > 0.0f)
            hopOffset = step->hop * std::fabs(std::sin(kPi * step->hopRate * stepTime));
        const float dirSign = facingRight ? 1.0f : -1.0f;   // positive tilt = nose down
        angle = step->tiltBias * dirSign + step->tilt * std::sin(2.0f * kPi * step->tiltRate * stepTime);
        shiver = step->shiver * std::sin(45.0f * stepTime);
        crouch = step->crouch;
        tint = step->tint;
    }

    std::size_t index = 0;
    if (step) {
        index = static_cast<std::size_t>(animationFrame) % frames.size();
    } else {
        const float interval = (pose == "sleep") ? 0.9f : 0.45f;
        index = static_cast<std::size_t>(sceneTime / interval) % frames.size();
    }

    const sf::Texture& texture = *frames[index];
    const sf::Vector2u size = texture.getSize();
    sf::Sprite dog(texture);
    dog.setOrigin({static_cast<float>(size.x) * 0.5f, static_cast<float>(size.y)});   // feet, centred
    // The supplied frames face left, so mirror when the dog faces right.
    dog.setScale({kDogScale * (facingRight ? -1.0f : 1.0f), kDogScale * (1.0f - crouch)});
    dog.setRotation(sf::degrees(angle));
    dog.setColor(tint);
    dog.setPosition({std::round(x + shiver), std::round(y - hopOffset)});
    window.draw(dog);

    if (ballCarried) {
        const float mouthX = x + (facingRight ? kMouthOffset : -kMouthOffset);
        const float mouthY = y - hopOffset - 27.0f;
        sf::CircleShape ball(9.0f);
        ball.setOrigin({9.0f, 9.0f});
        ball.setPosition({mouthX, mouthY});
        ball.setFillColor(sf::Color(204, 62, 56));
        ball.setOutlineColor(sf::Color(110, 30, 28));
        ball.setOutlineThickness(2.0f);
        window.draw(ball);
    }

    std::string cue = step ? step->cue : (restingPose == "sleep" ? std::string("Zzz") : std::string());
    if (!cue.empty()) {
        if (cue == "Zzz") cue = std::string("Zzz").substr(0, 1 + static_cast<std::size_t>(sceneTime * 1.5f) % 3);
        const float visiblePx = (pose == "sleep") ? 10.0f : (pose == "lay") ? 11.0f : 15.0f;
        pendingCue = cue;
        pendingCueX = x;
        pendingCueY = y - hopOffset - visiblePx * kDogScale * (1.0f - crouch) - 4.0f;
    }
}

void CompanionUI::drawPlayer(float x, float y) {
    // Simple silhouette; swap this function for a sprite-sheet player later.
    const float dir = playerFacingRight ? 1.0f : -1.0f;
    const sf::Color body(96, 72, 92);

    sf::CircleShape shadow(26.0f);
    shadow.setOrigin({26.0f, 26.0f});
    shadow.setScale({1.0f, 0.28f});
    shadow.setPosition({x, y + 2.0f});
    shadow.setFillColor(kShadow);
    window.draw(shadow);

    const float swing = playerMoving ? std::sin(sceneTime * 12.0f) * 5.0f : 0.0f;
    sf::RectangleShape leg({10.0f, 32.0f});
    leg.setFillColor(body);
    leg.setPosition({x - 12.0f, y - 32.0f + std::max(0.0f, swing)});
    window.draw(leg);
    leg.setPosition({x + 2.0f, y - 32.0f + std::max(0.0f, -swing)});
    window.draw(leg);
    sf::RectangleShape torso({28.0f, 44.0f});
    torso.setFillColor(body);
    torso.setPosition({x - 14.0f, y - 76.0f});
    window.draw(torso);
    sf::CircleShape head(12.0f);
    head.setFillColor(body);
    head.setPosition({x - 12.0f, y - 102.0f});
    window.draw(head);

    float armAngle = 6.0f;
    if (throwTimer > 0.0f) {
        const float p = 1.0f - throwTimer / 0.5f;
        armAngle = 6.0f - 170.0f * std::sin(kPi * p);   // swing up and forward when throwing
    }
    sf::RectangleShape arm({8.0f, 36.0f});
    arm.setFillColor(sf::Color(116, 90, 108));
    arm.setOrigin({4.0f, 4.0f});
    arm.setPosition({x + dir * 9.0f, y - 72.0f});
    arm.setRotation(sf::degrees(dir * armAngle));
    window.draw(arm);
}

void CompanionUI::drawSceneObjects() {
    const SceneLayout L = sceneLayout();

    // Dog bed (seen from above at an angle)
    sf::CircleShape bedOuter(62.0f);
    bedOuter.setOrigin({62.0f, 62.0f});
    bedOuter.setScale({1.0f, 0.34f});
    bedOuter.setPosition({L.bedX, L.bedY});
    bedOuter.setFillColor(sf::Color(121, 82, 64));
    window.draw(bedOuter);
    sf::CircleShape bedInner(48.0f);
    bedInner.setOrigin({48.0f, 48.0f});
    bedInner.setScale({1.0f, 0.28f});
    bedInner.setPosition({L.bedX, L.bedY - 2.0f});
    bedInner.setFillColor(sf::Color(176, 128, 98));
    window.draw(bedInner);

    // Food bowl
    sf::CircleShape bowlOuter(28.0f);
    bowlOuter.setOrigin({28.0f, 28.0f});
    bowlOuter.setScale({1.0f, 0.5f});
    bowlOuter.setPosition({L.bowlX, L.bowlY});
    bowlOuter.setFillColor(sf::Color(96, 120, 150));
    bowlOuter.setOutlineColor(sf::Color(60, 76, 98));
    bowlOuter.setOutlineThickness(2.0f);
    window.draw(bowlOuter);
    sf::CircleShape bowlInner(21.0f);
    bowlInner.setOrigin({21.0f, 21.0f});
    bowlInner.setScale({1.0f, 0.46f});
    bowlInner.setPosition({L.bowlX, L.bowlY - 1.0f});
    bowlInner.setFillColor(foodPresent ? sf::Color(150, 98, 60) : sf::Color(70, 88, 112));
    window.draw(bowlInner);
    if (foodPresent) {
        for (int i = 0; i < 6; ++i) {
            sf::CircleShape kibble(3.0f);
            kibble.setOrigin({3.0f, 3.0f});
            kibble.setPosition({L.bowlX - 12.0f + static_cast<float>(i) * 4.8f,
                                L.bowlY - 4.0f - static_cast<float>((i * 3) % 4)});
            kibble.setFillColor(sf::Color(190, 130, 80));
            window.draw(kibble);
        }
    }

    // Ball on the ground or in flight (the carried ball is drawn with the dog)
    if (!ballCarried) {
        sf::CircleShape ballShadow(9.0f);
        ballShadow.setOrigin({9.0f, 9.0f});
        ballShadow.setScale({1.0f, 0.32f});
        ballShadow.setPosition({ballX, ballY + 2.0f});
        ballShadow.setFillColor(kShadow);
        window.draw(ballShadow);
        sf::CircleShape ball(9.0f);
        ball.setOrigin({9.0f, 9.0f});
        ball.setPosition({ballX, ballY - ballHeight - 9.0f});
        ball.setFillColor(sf::Color(204, 62, 56));
        ball.setOutlineColor(sf::Color(110, 30, 28));
        ball.setOutlineThickness(2.0f);
        window.draw(ball);
        sf::CircleShape shine(2.5f);
        shine.setOrigin({2.5f, 2.5f});
        shine.setPosition({ballX - 3.0f, ballY - ballHeight - 12.0f});
        shine.setFillColor(sf::Color(255, 220, 215));
        window.draw(shine);
    }
}

void CompanionUI::drawControlHelper() {
    const SceneLayout L = sceneLayout();
    const float px = L.x + 14.0f, py = L.y + 14.0f;
    const bool paused = textInputFocused;
    drawRoundedBox(window, px, py + 3.0f, 252.0f, 106.0f, 14.0f, kShadowSoft);
    drawRoundedBox(window, px, py, 252.0f, 106.0f, 14.0f, sf::Color(255, 248, 234, 232), kBorder, 1.5f);
    drawText("MOVE", px + 12.0f, py + 6.0f, 13, kMuted);
    auto key = [&](const std::string& label, float kx, float ky, bool down) {
        const bool active = down && !paused && window.hasFocus();
        drawRoundedBox(window, kx, ky, 30.0f, 30.0f, 8.0f, active ? kAccent : kCream,
                       active ? kAccentDark : sf::Color(196, 150, 112), 1.5f);
        drawText(label, kx + 9.0f, ky + 4.0f, 17, active ? sf::Color::White : kInk);
    };
    auto pressed = [](sf::Keyboard::Key a, sf::Keyboard::Key b) {
        return sf::Keyboard::isKeyPressed(a) || sf::Keyboard::isKeyPressed(b);
    };
    key("W", px + 44.0f, py + 26.0f, pressed(sf::Keyboard::Key::W, sf::Keyboard::Key::Up));
    key("A", px + 10.0f, py + 60.0f, pressed(sf::Keyboard::Key::A, sf::Keyboard::Key::Left));
    key("S", px + 44.0f, py + 60.0f, pressed(sf::Keyboard::Key::S, sf::Keyboard::Key::Down));
    key("D", px + 78.0f, py + 60.0f, pressed(sf::Keyboard::Key::D, sf::Keyboard::Key::Right));
    drawText(paused ? "Typing: movement\npaused. Click the\nyard to move again."
                    : "Arrow keys also work.\nWalk up to the dog\nto greet or pet it.",
             px + 124.0f, py + 28.0f, 12, kInk);
}

void CompanionUI::drawHover() {
    const sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    const SceneLayout L = sceneLayout();
    if (mouse.x < L.x || mouse.x > L.x + L.w || mouse.y < L.y || mouse.y > L.y + L.h) return;
    const std::vector<SceneObject> objects = sceneObjects();
    const SceneObject* hovered = nullptr;
    for (const auto& object : objects)
        if (object.bounds.contains(mouse)) hovered = &object;
    if (hovered == nullptr) return;

    drawRoundedBox(window, hovered->bounds.position.x, hovered->bounds.position.y,
                   hovered->bounds.size.x, hovered->bounds.size.y, 10.0f,
                   sf::Color(255, 244, 220, 38), sf::Color(255, 244, 220, 235), 2.0f);

    sf::Text tip(font, hovered->label, 15);
    tip.setFillColor(kInk);
    const sf::FloatRect b = tip.getLocalBounds();
    const float px = mouse.x + 14.0f, py = mouse.y - 34.0f;
    drawRoundedBox(window, px, py + 2.0f, b.size.x + 20.0f, b.size.y + 14.0f, 9.0f, kShadowSoft);
    drawRoundedBox(window, px, py, b.size.x + 20.0f, b.size.y + 14.0f, 9.0f, kCream, sf::Color(196, 150, 112), 1.5f);
    tip.setPosition({px + 10.0f - b.position.x, py + 7.0f - b.position.y});
    window.draw(tip);
}


void CompanionUI::processInteraction(const std::string& label,
                                     const EnvironmentObservation& observation,
                                     float feedbackRewardValue,
                                     const std::string& preference,
                                     bool preferencePositive,
                                     const std::string& sceneEvent,
                                     const std::string& proximityBasis) {
    interactionQueuedAt = std::chrono::steady_clock::now();
    interactionBuffer.push_back(QueuedInteraction{label, observation, feedbackRewardValue, preference,
                                                  preferencePositive, sceneEvent, proximityBasis});
    appendLog("Queued interaction (" + std::to_string(interactionBuffer.size()) + " waiting): " + label);
}

void CompanionUI::executeInteraction(const std::string& label,
                                     const EnvironmentObservation& observation,
                                     float feedbackRewardValue,
                                     const std::string& preference,
                                     bool preferencePositive,
                                     const std::string& sceneEvent) {
    if (!preference.empty()) proposed.recordUserPreference(preference, preferencePositive);
    const auto affectBefore = proposed.getAffectiveState();
    const std::size_t memoryCountBefore = proposed.getMemoryRecords().size();
    const int interactionId = interactionCount + 1;
    lastActionInteractionId = interactionId;
    latestObservation = observation;
    latestStimulus = label;
    if (guidedStudyMode) {
        latestBaselineAction = selectedAgentIsProposed ? "NOT_RUN" : baseline.selectAction(observation);
        latestProposedAction = selectedAgentIsProposed ? proposed.selectAction(observation) : "NOT_RUN";
    } else {
        latestBaselineAction = baseline.selectAction(observation);
        latestProposedAction = proposed.selectAction(observation);
    }
    const std::size_t baselineStateBeforeOutcome = baseline.getCurrentState().encode();
    const std::size_t proposedStateBeforeOutcome = proposed.getCurrentState().encode();
    nlohmann::json memoryCandidates = nlohmann::json::array();
    for (const auto& memory : proposed.getMemoryRecords()) {
        if (memory.raw_fact.subject == latestProposedAction ||
            memory.raw_fact.relation == "prefers_action" || memory.raw_fact.relation == "dislikes_action" ||
            memory.raw_fact.relation == "likes" || memory.raw_fact.relation == "dislikes") {
            memoryCandidates.push_back({
                {"subject", memory.raw_fact.subject},
                {"relation", memory.raw_fact.relation},
                {"object", memory.raw_fact.object},
                {"confidence", memory.confidence},
                {"observation_count", memory.observation_count}
            });
        }
    }
    // Show the visible companion's chosen action in the scene.
    // The scene event was already applied (and proximity derived) before this call.
    startBehaviour(selectedAgentIsProposed ? latestProposedAction : latestBaselineAction, sceneEvent);
    const auto responseLatencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - interactionQueuedAt).count();

    ActionOutcome baselineOutcome;
    baselineOutcome.reward = feedbackRewardValue;
    baselineOutcome.terminal = true;
    baselineOutcome.outcome_tag = "simulation_" + (feedbackRewardValue > 0 ? std::string("positive") :
                                                    feedbackRewardValue < 0 ? std::string("negative") : std::string("neutral"));
    ActionOutcome proposedOutcome = baselineOutcome;
    if (guidedStudyMode) {
        // Only the visible condition receives the interaction. Learning stays off,
        // but the proposed condition still updates its affect/memory state from events.
        if (selectedAgentIsProposed)
            proposed.applyOutcome(observation, latestProposedAction, proposedOutcome);
        else
            baseline.applyOutcome(observation, latestBaselineAction, baselineOutcome);
    } else {
        baseline.applyOutcome(observation, latestBaselineAction, baselineOutcome);
        proposed.applyOutcome(observation, latestProposedAction, proposedOutcome);
    }

    baselineRewards.push_back(feedbackRewardValue);
    proposedRewards.push_back(feedbackRewardValue);
    ++interactionCount;
    appendLog("You: " + label);
    appendLog("Baseline: " + describeAction(latestBaselineAction));
    appendLog("Proposed: " + describeAction(latestProposedAction));
    {
        nlohmann::json row;
        row["schema_version"] = 1;
        row["run_id"] = runId;
        row["interaction_id"] = interactionId;
        row["sequence_index"] = interactionId - 1;
        row["timestamp"] = utcTimestamp();
        row["processing_status"] = "completed";
        row["condition_code"] = std::string(1, selectedAgentCode);
        row["condition"] = selectedAgentIsProposed ? "proposed" : "baseline";
        row["participant_code"] = guidedStudyMode ? studyParticipantCode : "";
        row["session_index"] = guidedStudyMode ? studySessionIndex : 0;
        row["phase"] = guidedStudyMode ? studyPhase : "simulation";
        row["step_id"] = guidedStudyMode && studyPhase == "guided" ? studyStepIndex + 1 : 0;
        row["software_revision"] = IS2_GIT_REVISION;
        row["policy_updates_enabled"] = false;
        row["selected_action"] = selectedAgentIsProposed ? latestProposedAction : latestBaselineAction;
        row["response_latency_ms"] = guidedStudyMode ? nlohmann::json(responseLatencyMs) : nlohmann::json(nullptr);
        row["input_summary"] = label;
        row["observation"] = {
            {"goal_state", observation.goal_state},
            {"target_proximity", observation.target_proximity},
            {"user_status", observation.user_status},
            {"task_status", observation.task_status},
            {"signal_strength", observation.signal_strength},
            {"food_available", observation.food_available},
            {"play_available", observation.play_available}
        };
        row["baseline"] = {{"action", latestBaselineAction}, {"reward", baselineOutcome.reward},
                           {"state_before_outcome", baselineStateBeforeOutcome},
                           {"state_after_outcome", baseline.getCurrentState().encode()},
                           {"learning_update_enabled", false}};
        row["proposed"] = {
            {"action", latestProposedAction}, {"reward", proposedOutcome.reward},
            {"state_before_outcome", proposedStateBeforeOutcome},
            {"state_after_outcome", proposed.getCurrentState().encode()},
            {"memory_candidates", memoryCandidates},
            {"learning_update_enabled", false},
            {"memory_count_before", memoryCountBefore},
            {"memory_count_after", proposed.getMemoryRecords().size()},
            {"affect_before", {
                {"affinity", affectBefore.affinity}, {"stress", affectBefore.stress},
                {"arousal", affectBefore.arousal}, {"engagement", affectBefore.engagement},
                {"valence", static_cast<int>(affectBefore.valence)}
            }},
            {"affect_after", {
                {"affinity", proposed.getAffectiveState().affinity},
                {"stress", proposed.getAffectiveState().stress},
                {"arousal", proposed.getAffectiveState().arousal},
                {"engagement", proposed.getAffectiveState().engagement},
                {"valence", static_cast<int>(proposed.getAffectiveState().valence)}
            }}
        };        std::ofstream log(std::filesystem::path(outputDirectory) / ("interaction-events-" + runId + ".jsonl"), std::ios::app);
        if (log) log << row.dump() << '\n';
        else appendLog("WARNING: interaction event log could not be written.");
    }
    if (guidedStudyMode && studyPhase == "guided" && studyInputMatches(label, false))
        completeStudyStep(label);
    saveSession();
}

std::string CompanionUI::interpretIntent(const std::string& text, EnvironmentObservation& observation, float& reward) {
    observation = EnvironmentObservation{0, 1, 0, 0, 0.45f};
    reward = 0.25f;
    std::string lowered = text;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    const std::string output = callNlp(text);
    try {
        if (!output.empty()) {
            const auto parsed = json::parse(output);
            const std::string intent = parsed.value("intent", std::string("unknown"));
            const bool negativeFeedback = intent == "negative_feedback" ||
                lowered.find("stop") != std::string::npos ||
                lowered.find("annoy") != std::string::npos ||
                lowered.find("scold") != std::string::npos ||
                lowered.find("dislike") != std::string::npos ||
                lowered.find("hate") != std::string::npos ||
                lowered.find("don't") != std::string::npos ||
                lowered.find("do not") != std::string::npos;
            // Named arguments: EnvironmentObservation's member order is goal, proximity,
            // USER status, TASK status. The previous positional list swapped the last two.
            observation = makeObservation(
                parsed.value("goal_state", 0), parsed.value("target_proximity", 1),
                negativeFeedback ? 2 : parsed.value("user_status", 0), parsed.value("task_status", 0),
                parsed.value("signal_strength", 0.45f),
                parsed.value("food_available", false), parsed.value("play_available", false));
            observation.food_available = observation.food_available ||
                lowered.find("food") != std::string::npos || lowered.find("feed") != std::string::npos;
            observation.play_available = observation.play_available ||
                lowered.find("play") != std::string::npos || lowered.find("toy") != std::string::npos;
            reward = parsed.value("reward", 0.25f);
            nlpMethod = parsed.value("method", std::string("unknown"));
            return intent;
        }
    } catch (const std::exception&) {
        // Continue with a local fallback if Python is missing or returns invalid JSON.
    }
    nlpMethod = "C++ keyword fallback";
    if (lowered.find("stop") != std::string::npos || lowered.find("annoy") != std::string::npos ||
        lowered.find("don't") != std::string::npos || lowered.find("do not") != std::string::npos || lowered.find("scold") != std::string::npos) {
        observation = EnvironmentObservation{0, 1, 2, 0, 0.45f}; reward = -1.0f; return "negative_feedback";
    }
    if (lowered.find("fetch") != std::string::npos || lowered.find("ball") != std::string::npos ||
        lowered.find("bring") != std::string::npos || lowered.find("retrieve") != std::string::npos) {
        observation = makeObservation(1, 1, 1, 1, 0.85f); reward = 0.65f; return "request";   // task_status must be 1 for FETCH to be valid
    }
    if (lowered.find("noise") != std::string::npos || lowered.find("scared") != std::string::npos ||
        lowered.find("loud") != std::string::npos) {
        observation = EnvironmentObservation{2, 0, 0, 0, 0.95f}; reward = 0.0f; return "disturbance";
    }
    if (lowered.find("feed") != std::string::npos || lowered.find("food") != std::string::npos ||
        lowered.find("eat") != std::string::npos) {
        observation = EnvironmentObservation{2, 0, 1, 0, 0.30f, true, false};
        reward = 0.5f; return "feeding";
    }
    if (lowered.find("play") != std::string::npos || lowered.find("toy") != std::string::npos) {
        observation = EnvironmentObservation{0, 1, 1, 0, 0.70f, false, true};
        reward = 0.5f; return "play";
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
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (guidedStudyMode && studyPhase == "guided" && !studyInputMatches(text, true)) {
        appendLog("Please follow the current guided-study prompt.");
        return;
    }
    if (guidedStudyMode && studyPhase != "guided" && studyPhase != "free") return;

    const bool explicitLike = lower.find("prefer") != std::string::npos ||
                              lower.find(" like ") != std::string::npos ||
                              lower.rfind("like ", 0) == 0 ||
                              lower.find("love") != std::string::npos;
    const bool explicitDislike = lower.find("dislike") != std::string::npos ||
                                 lower.find("hate") != std::string::npos ||
                                 lower.find("don't like") != std::string::npos ||
                                 lower.find("do not like") != std::string::npos ||
                                 lower.find("not like") != std::string::npos;

    // Preference statements are records first, not commands. They may also
    // refer to a recent action/event, but that relationship remains a candidate.
    if (explicitLike || explicitDislike) {
        std::string object;
        if (lower.find("ball") != std::string::npos || lower.find("toy") != std::string::npos ||
            lower.find("fetch") != std::string::npos) object = "ball";
        else if (lower.find("sound") != std::string::npos || lower.find("noise") != std::string::npos ||
                 lower.find("bark") != std::string::npos) object = "sound";
        else if (lower.find("rest") != std::string::npos || lower.find("sleep") != std::string::npos) object = "rest";
        else {
            std::size_t relationStart = std::string::npos;
            std::size_t relationLength = 0;
            for (const std::string& marker : {std::string("like"), std::string("love"),
                                              std::string("prefer"), std::string("hate"),
                                              std::string("dislike")}) {
                const std::size_t found = lower.find(marker);
                if (found != std::string::npos && (relationStart == std::string::npos || found < relationStart)) {
                    relationStart = found;
                    relationLength = marker.size();
                }
            }
            if (relationStart != std::string::npos) {
                object = lower.substr(relationStart + relationLength);
                while (!object.empty() && !std::isalnum(static_cast<unsigned char>(object.front()))) object.erase(object.begin());
                while (!object.empty() && !std::isalnum(static_cast<unsigned char>(object.back()))) object.pop_back();
                for (const std::string& article : {std::string("the "), std::string("a "), std::string("an ")}) {
                    if (object.rfind(article, 0) == 0) { object.erase(0, article.size()); break; }
                }
                if (object.size() > 80) object.resize(80);
            }
        }

        if (object.empty()) {
            ++interactionCount;
            appendLog("You: " + text + " [preference statement; object unresolved; no action selected]");
            nlohmann::json row;
            row["schema_version"] = 2;
            row["run_id"] = runId;
            row["interaction_id"] = interactionCount;
            row["sequence_index"] = interactionCount - 1;
            row["timestamp"] = utcTimestamp();
            row["processing_status"] = "statement_object_unresolved";
            row["condition_code"] = std::string(1, selectedAgentCode);
            row["condition"] = selectedAgentIsProposed ? "proposed" : "baseline";
            row["participant_code"] = guidedStudyMode ? studyParticipantCode : "";
            row["session_index"] = guidedStudyMode ? studySessionIndex : 0;
            row["phase"] = guidedStudyMode ? studyPhase : "simulation";
            row["step_id"] = guidedStudyMode && studyPhase == "guided" ? studyStepIndex + 1 : 0;
            row["software_revision"] = IS2_GIT_REVISION;
            row["policy_updates_enabled"] = false;
            row["input_summary"] = text;
            row["utterance_analysis"] = {{"kind", "preference_statement"}, {"memory_updated", false},
                                         {"response_required", false}, {"response_action", "NONE"},
                                         {"candidate_association", "unresolved"}};
            std::ofstream log(std::filesystem::path(outputDirectory) / ("interaction-events-" + runId + ".jsonl"), std::ios::app);
            if (log) log << row.dump() << '\n';
            else appendLog("WARNING: interaction event log could not be written.");
            saveSession();
            return;
        } else {
            const std::string relation = explicitDislike ? "dislikes" : "likes";
            const bool memoryUpdated = !guidedStudyMode || selectedAgentIsProposed;
            if (memoryUpdated) proposed.recordUserStatement(relation, object);
            const std::string previousContext = latestStimulus;
            std::string associationType = "none";
            std::string associationReason = "No recent matching action or event was found.";
            float associationConfidence = 0.0f;
            const bool hasPriorAction = interactionCount > 0 && lastActionInteractionId > 0 &&
                interactionCount == lastActionInteractionId;
            const bool mentionsSound = object == "sound";
            const bool mentionsBall = object == "ball";
            const bool mentionsRest = object == "rest";
            const bool contextMatches = hasPriorAction && (
                (mentionsSound && (previousContext.find("noise") != std::string::npos ||
                                   previousContext.find("sound") != std::string::npos ||
                                   latestBaselineAction == "BARK" || latestProposedAction == "BARK" ||
                                   latestBaselineAction == "WHINE" || latestProposedAction == "WHINE")) ||
                (mentionsBall && (previousContext.find("ball") != std::string::npos ||
                                  previousContext.find("fetch") != std::string::npos ||
                                  latestBaselineAction == "FETCH" || latestProposedAction == "FETCH")) ||
                (mentionsRest && (previousContext.find("rest") != std::string::npos ||
                                  previousContext.find("sleep") != std::string::npos ||
                                  latestBaselineAction == "SLEEP_LAY" || latestProposedAction == "SLEEP_LAY")));
            if (contextMatches) {
                associationType = "candidate_recent_context";
                associationConfidence = 0.65f;
                associationReason = "The statement object matches the immediately preceding event or action; this is not proof of causation.";
            }

            ++interactionCount;
            appendLog("You: " + text + " [preference statement]");
            appendLog(memoryUpdated ? "Memory: stored user " + relation + " " + object
                                    : "Preference statement recorded as study input; baseline memory remains disabled.");
            appendLog(contextMatches ? "Context: candidate association with preceding interaction."
                                     : "Context: no relevant preceding interaction; stored as a general preference record.");

            nlohmann::json row;
            row["schema_version"] = 2;
            row["run_id"] = runId;
            row["interaction_id"] = interactionCount;
            row["sequence_index"] = interactionCount - 1;
            row["timestamp"] = utcTimestamp();
            row["processing_status"] = "recorded_no_action";
            row["condition_code"] = std::string(1, selectedAgentCode);
            row["condition"] = selectedAgentIsProposed ? "proposed" : "baseline";
            row["participant_code"] = guidedStudyMode ? studyParticipantCode : "";
            row["session_index"] = guidedStudyMode ? studySessionIndex : 0;
            row["phase"] = guidedStudyMode ? studyPhase : "simulation";
            row["step_id"] = guidedStudyMode && studyPhase == "guided" ? studyStepIndex + 1 : 0;
            row["software_revision"] = IS2_GIT_REVISION;
            row["policy_updates_enabled"] = false;
            row["input_summary"] = text;
            row["utterance_analysis"] = {
                {"kind", "preference_statement"}, {"relation", relation}, {"object", object},
                {"memory_updated", memoryUpdated}, {"response_required", false}, {"response_action", "NONE"},
                {"candidate_association", associationType}, {"association_confidence", associationConfidence},
                {"association_reason", associationReason},
                {"referenced_interaction_id", contextMatches ? lastActionInteractionId : 0},
                {"preceding_context", hasPriorAction ? previousContext : ""},
                {"preceding_baseline_action", hasPriorAction ? latestBaselineAction : ""},
                {"preceding_proposed_action", hasPriorAction ? latestProposedAction : ""}
            };
            row["learning_update_enabled"] = false;
            std::ofstream log(std::filesystem::path(outputDirectory) / ("interaction-events-" + runId + ".jsonl"), std::ios::app);
            if (log) log << row.dump() << '\n';
            else appendLog("WARNING: interaction event log could not be written.");
            if (guidedStudyMode && studyPhase == "guided") completeStudyStep(text);
            saveSession();
            return;
        }
    }

    EnvironmentObservation observation;
    float reward = 0.25f;
    const std::string intent = interpretIntent(text, observation, reward);

    // Unknown utterances are not automatically treated as commands. Keep them
    // visible in the log without forcing a policy action or inventing a fact.
    if (intent == "unknown") {
        ++interactionCount;
        appendLog("You: " + text + " [unclassified statement; no action selected]");
        nlohmann::json row;
        row["schema_version"] = 2;
        row["run_id"] = runId;
        row["interaction_id"] = interactionCount;
        row["sequence_index"] = interactionCount - 1;
        row["timestamp"] = utcTimestamp();
        row["processing_status"] = "unclassified_no_action";
        row["condition_code"] = std::string(1, selectedAgentCode);
        row["condition"] = selectedAgentIsProposed ? "proposed" : "baseline";
        row["participant_code"] = guidedStudyMode ? studyParticipantCode : "";
        row["session_index"] = guidedStudyMode ? studySessionIndex : 0;
        row["phase"] = guidedStudyMode ? studyPhase : "simulation";
        row["step_id"] = guidedStudyMode && studyPhase == "guided" ? studyStepIndex + 1 : 0;
        row["software_revision"] = IS2_GIT_REVISION;
        row["policy_updates_enabled"] = false;
        row["input_summary"] = text;
        row["utterance_analysis"] = {{"kind", "unclassified"}, {"memory_updated", false},
                                     {"response_required", false}, {"response_action", "NONE"}};
        row["learning_update_enabled"] = false;
        std::ofstream log(std::filesystem::path(outputDirectory) / ("interaction-events-" + runId + ".jsonl"), std::ios::app);
        if (log) log << row.dump() << '\n';
        else appendLog("WARNING: interaction event log could not be written.");
        saveSession();
        return;
    }

    // Scene reaction for typed commands (ball placed / thrown, food put in the bowl).
    std::string sceneEvent;
    if (lower.find("throw") != std::string::npos) sceneEvent = "throw_ball";
    else if (intent == "request" && (lower.find("ball") != std::string::npos ||
                                     lower.find("fetch") != std::string::npos ||
                                     lower.find("bring") != std::string::npos)) sceneEvent = "fetch_ball";
    else if (intent == "feeding") sceneEvent = "feed";

    std::string proximityBasis;
    if (sceneEvent == "fetch_ball" || sceneEvent == "throw_ball") proximityBasis = "ball";
    else if (sceneEvent == "feed") proximityBasis = "bowl";
    else if (intent == "praise" || intent == "negative_feedback" || intent == "greeting" || intent == "play") proximityBasis = "user";
    else if (lower.find("rest") != std::string::npos || lower.find("sleep") != std::string::npos) proximityBasis = "bed";
    else proximityBasis = "nearest";

    processInteraction(text + " [" + intent + " / " + nlpMethod + "]", observation, reward, "", true, sceneEvent, proximityBasis);
}

void CompanionUI::handleClick(sf::Vector2f point) {
    const float x = point.x, y = point.y;
    if (screen == Screen::AgentSelection) {
        if (researcherMode && y >= 530 && y <= 600 && x >= 180 && x <= 580) {
            proposed.clearMemory();
            std::error_code ec;
            const auto memoryPath = std::filesystem::path(policyDirectory) / "proposed.qtable.memory.json";
            std::filesystem::remove(memoryPath, ec);
            appendLog(ec ? "Memory file deletion failed: " + ec.message() : "Persistent memory cleared.");
            return;
        }
        if (researcherMode && y >= 530 && y <= 600 && x >= 700 && x <= 1100) {
            startNewParticipant();
            return;
        }
        if (y >= 300 && y <= 380 && ((x >= 180 && x <= 580) || (x >= 700 && x <= 1100))) {
            selectedAgentCode = x < 640.0f ? 'A' : 'B';
            selectedAgentIsProposed = selectedAgentCode == 'A' ? agentAIsProposed : !agentAIsProposed;
            {
                nlohmann::json manifest;
                manifest["schema_version"] = 1;
                manifest["mode"] = "interactive_simulation";
                manifest["ui_mode"] = researcherMode ? "researcher" : "study";
                manifest["run_id"] = runId;
                manifest["started_at"] = utcTimestamp();
                manifest["policy_updates_enabled"] = false;
                manifest["selected_condition"] = std::string(1, selectedAgentCode);
                manifest["condition_key"] = {
                    {"A", agentAIsProposed ? "proposed" : "baseline"},
                    {"B", agentAIsProposed ? "baseline" : "proposed"}
                };
                std::ofstream manifestOut(std::filesystem::path(outputDirectory) / ("run-manifest-" + runId + ".json"));
                if (manifestOut) manifestOut << manifest.dump(2) << '\n';
            }
            resetScene();
            screen = Screen::Simulation;
            appendLog(std::string("Loaded blind condition ") + selectedAgentCode);
        }
        return;
    }
    if (y >= 14 && y <= 58) {
        if (x >= 28 && x < 173) {
            screen = Screen::Simulation;
        } else if (researcherMode) {
            if (x >= 185 && x < 330) screen = Screen::AgentSelection;
            else if (x >= 342 && x < 487) screen = Screen::AgentState;
            else if (x >= 499 && x < 644) screen = Screen::Comparison;
            else if (x >= 1030 && screen == Screen::Simulation) controlsVisible = !controlsVisible;
        } else {
            if (x >= 185 && x < 345) {
                if (guidedStudyMode) {
                    if (studyPhase == "free") {
                        studyFreeFinished = true;
                        studyPhase = "rating";
                        logStudyPhaseEvent("phase_ended", "free");
                        logStudyPhaseEvent("phase_started", "rating");
                        writeStudyManifest();
                    } else if (studyPhase == "rating" && studyFormOpenedCurrentCondition) {
                        if (studySessionIndex == 1) {
                            studySessionIndex = 2;
                            studyStartedAt = utcTimestamp();
                            selectedAgentCode = 'B';
                            selectedAgentIsProposed = !agentAIsProposed;
                            const auto ticks = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch()).count();
                            runId = "study-" + studyParticipantCode + "-" + std::to_string(ticks);
                            studyStepIndex = 0;
                            studyPhase = "guided";
                            studyIdleElapsed = 0.0f;
                            studyFreeElapsed = 0.0f;
                            studyGuidedComplete = false;
                            studyFreeFinished = false;
                            studyFormOpenedCurrentCondition = false;
                            eventLog.clear();
                            interactionCount = 0;
                            lastActionInteractionId = 0;
                            latestBaselineAction = "IDLE";
                            latestProposedAction = "IDLE";
                            baseline.reset();
                            proposed.resetForStudy();
                            resetScene();
                            screen = Screen::Simulation;
                            writeStudyManifest();
                        } else {
                            studyFinished = true;
                            logStudyPhaseEvent("phase_ended", "rating");
                            studyPhase = "finished";
                            logStudyPhaseEvent("phase_started", "finished");
                            writeStudyManifest();
                            saveSession();
                        }
                    }
                } else {
                    screen = Screen::AgentSelection;
                }
            } else if (x >= 357 && x < 517) {
                if (!guidedStudyMode || (studyPhase == "rating" && studyFreeFinished))
                    openStudyForm();
            }
        }
        return;
    }
    if (researcherMode && screen == Screen::AgentState && x >= 930 && y >= 750 && y <= 810) {
        proposed.clearMemory();
        std::error_code ec;
        const auto prefix = std::filesystem::path(policyDirectory) / "proposed.qtable";
        std::filesystem::remove(prefix.string() + ".memory.json", ec);
        if (ec) appendLog("Memory file deletion failed: " + ec.message());
        else appendLog("Persistent memory cleared by user request.");
        return;
    }
    if (screen != Screen::Simulation) return;
    if (guidedStudyMode && studyPhase != "guided" && studyPhase != "free") return;
    if (guidedStudyMode && studyPhase == "guided" &&
        (studyStepIndex == 5 || studyStepIndex == 8 || studyStepIndex == 9)) {
        // These steps require a typed statement or a timed observation, not a button.
    }
    if (!controlsVisible) return;

    const SceneLayout L = sceneLayout();
    const float firstRowY = L.y + L.h + 15.0f;
    for (std::size_t i = 0; i < kControls.size(); ++i) {
        const float bx = kButtonX0 + static_cast<float>(i % 5) * (kButtonW + kButtonGap);
        const float by = firstRowY + static_cast<float>(i / 5) * kButtonRowStep;
        if (x >= bx && x < bx + kButtonW && y >= by && y < by + kButtonH) {
            const ControlButton& control = kControls[i];
            if (guidedStudyMode) {
                if (studyPhase == "guided") {
                    if (behaviourActive || !interactionBuffer.empty() ||
                        !studyInputMatches(control.label, false)) return;
                } else if (studyPhase != "free") {
                    return;
                }
            }
            textInputFocused = false;
            processInteraction(control.stimulus,
                               makeObservation(control.goal, control.proximity, control.user, control.task,
                                               control.signal, control.food, control.play),
                               control.reward, "", true, control.sceneEvent, control.basis);
            return;
        }
    }
    textInputFocused = y >= kInputY && y <= kInputY + kInputH && x < 1020.0f;
    if (y >= kInputY && y <= kInputY + kInputH && x >= 1020.0f && !textInput.empty()) {
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
    const sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    const bool hover = mouse.x >= x && mouse.x < x + width && mouse.y >= y && mouse.y < y + height;
    drawRoundedBox(window, x, y + 3.0f, width, height, 12.0f, kShadowSoft);
    drawRoundedBox(window, x, y, width, height, 12.0f,
                   selected ? kAccentSoft : (hover ? sf::Color(255, 240, 214) : kPanel),
                   selected ? kAccent : kBorder, selected ? 2.0f : 1.5f);
    drawText(label, x + 14, y + 12, 16, selected ? kAccentDark : kInk);
}

void CompanionUI::drawBar(const std::string& label, float value, float x, float y, float width) {
    drawText(label + "  " + levelText(value), x, y, 15, kInk);
    drawRoundedBox(window, x, y + 25, width, 10, 5.0f, sf::Color(238, 224, 204));
    const float fillWidth = width * clamp01(value);
    if (fillWidth >= 2.0f) drawRoundedBox(window, x, y + 25, fillWidth, 10, 5.0f, kAccent);
}

void CompanionUI::appendLog(const std::string& value) {
    eventLog.push_back(value);
    if (eventLog.size() > 12) eventLog.erase(eventLog.begin());
}

void CompanionUI::drawAgentSelectionScreen() {
    drawText("CHOOSE COMPANION", 60, 120, 30, kInk);
    drawText(researcherMode
                 ? "Select A or B. The interface does not disclose the model behind either label."
                 : "Select a companion to interact with.",
             60, 170, 17, kMuted);
    drawButton("Load Agent A", 180, 300, 400, 80, selectedAgentCode == 'A');
    drawButton("Load Agent B", 700, 300, 400, 80, selectedAgentCode == 'B');
    if (!researcherMode) {
        drawText("After each interaction, use \"Answer Form\" to rate that companion.", 180, 430, 16, kMuted);
        return;
    }
    drawText("[researcher mode]", 60, 205, 15, kMuted);
    drawText(std::string("Mapping for this participant: A = ") + (agentAIsProposed ? "proposed" : "baseline") +
                 ", B = " + (agentAIsProposed ? "baseline" : "proposed"),
             180, 430, 16, kMuted);
    drawText("Memory persists between sessions for the proposed model.", 180, 460, 16, kMuted);
    drawButton("Clear Persistent Memory", 180, 540, 400, 54);
    drawButton("New Participant", 700, 540, 400, 54);
    drawText("Clear Persistent Memory deletes the proposed agent's saved memory only.", 180, 610, 14, kMuted);
    drawText("New Participant clears memory, resets the session and alternates the A/B mapping.", 180, 632, 14, kMuted);
}

void CompanionUI::drawNavigation() {
    drawButton("Simulation", 28, 12, 145, 42, screen == Screen::Simulation);
    if (researcherMode) {
        drawButton("Change Agent", 185, 12, 145, 42);
        drawButton("Agent State", 342, 12, 145, 42, screen == Screen::AgentState);
        drawButton("Comparison", 499, 12, 145, 42, screen == Screen::Comparison);
        if (screen == Screen::Simulation)
            drawButton(controlsVisible ? "Hide Controls" : "Show Controls", 1030, 12, 180, 42);
        drawText("ANIMAL AGENT / SFML [researcher]", 700, 22, 15, kMuted);
    } else if (guidedStudyMode) {
        std::string continueLabel = "Change Model (locked)";
        if (studyPhase == "free") continueLabel = "Finish Free Phase";
        else if (studyPhase == "rating" && studyFormOpenedCurrentCondition)
            continueLabel = studySessionIndex == 1 ? "Continue to Companion B" : "Finish Study";
        drawButton(continueLabel, 185, 12, 160, 42,
                   studyPhase == "free" || (studyPhase == "rating" && studyFormOpenedCurrentCondition));
        drawButton(studyFormOpenedCurrentCondition ? "Form Opened" : "Answer Form",
                   357, 12, 160, 42, studyFormOpenedCurrentCondition);
        drawText("Participant study | Session " + std::to_string(studySessionIndex) + "/2",
                 700, 22, 15, kMuted);
    } else {
        drawButton("Change Model", 185, 12, 160, 42);
        drawButton("Answer Form", 357, 12, 160, 42);
    }
}

void CompanionUI::drawBackdrop(float x, float y, float width, float height) {
    // Warm top-down yard: sun-washed grass with tufts and flowers, a hedge that fades
    // into the lawn, the house and tree at the back, a softly edged dirt path and a crop bed.
    const SceneLayout L = sceneLayout();

    sf::RectangleShape lawn({width, height});
    lawn.setPosition({x, y});
    lawn.setFillColor(sf::Color(141, 156, 80));      // plain grass tile colour, warmed slightly
    window.draw(lawn);

    // Hedge that fades into the lawn instead of ending in a hard line.
    sf::RectangleShape hedgeTop({width, height * 0.09f});
    hedgeTop.setPosition({x, y});
    hedgeTop.setFillColor(sf::Color(96, 122, 66));
    window.draw(hedgeTop);
    drawGradient(window, x, y + height * 0.09f, width, height * 0.12f,
                 sf::Color(96, 122, 66, 255), sf::Color(96, 122, 66, 0), true);

    // Deterministic grass tufts and flowers (same positions every frame).
    unsigned int seed = 20260101u;
    auto rnd = [&seed]() {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>((seed >> 8) & 0xFFFFu) / 65535.0f;
    };
    const float pathLeft = L.houseX + 147.0f * 0.5f - 48.0f;
    for (int i = 0; i < 110; ++i) {
        const float tx = x + 18.0f + rnd() * (width - 36.0f);
        const float ty = y + height * 0.22f + rnd() * (height * 0.76f);
        const bool lighter = rnd() > 0.5f;
        if (tx > pathLeft - 6.0f && tx < pathLeft + 102.0f && ty > L.minY - 20.0f) continue;   // keep the path clear
        for (int blade = 0; blade < 3; ++blade) {
            sf::RectangleShape grass({2.0f, 6.0f});
            grass.setOrigin({1.0f, 6.0f});
            grass.setPosition({tx + static_cast<float>(blade - 1) * 3.0f, ty});
            grass.setRotation(sf::degrees(static_cast<float>(blade - 1) * 22.0f));
            grass.setFillColor(lighter ? sf::Color(168, 182, 98, 200) : sf::Color(112, 134, 66, 200));
            window.draw(grass);
        }
    }
    const sf::Color petals[3] = {sf::Color(255, 244, 214), sf::Color(240, 176, 176), sf::Color(250, 214, 110)};
    for (int i = 0; i < 26; ++i) {
        const float fx = x + 24.0f + rnd() * (width - 48.0f);
        const float fy = y + height * 0.26f + rnd() * (height * 0.70f);
        const int colour = static_cast<int>(rnd() * 2.99f);
        if (fx > pathLeft - 10.0f && fx < pathLeft + 106.0f && fy > L.minY - 20.0f) continue;
        sf::CircleShape flower(3.0f);
        flower.setOrigin({3.0f, 3.0f});
        flower.setPosition({fx, fy});
        flower.setFillColor(petals[colour]);
        window.draw(flower);
        sf::CircleShape centre(1.2f);
        centre.setOrigin({1.2f, 1.2f});
        centre.setPosition({fx, fy});
        centre.setFillColor(sf::Color(214, 150, 70));
        window.draw(centre);
    }

    auto sprite = [&](sf::IntRect source, sf::Vector2f position, float scale) {
        if (!environmentTextureLoaded) return;
        sf::Sprite item(environmentTexture);
        item.setTextureRect(source);
        item.setScale({scale, scale});
        item.setPosition(position);
        window.draw(item);
    };

    // Dirt path from the house door down through the yard, with softened edges.
    if (environmentTextureLoaded) {
        const float tile = 48.0f;
        const float pathX = pathLeft;
        const float pathTop = L.minY - 16.0f;
        int variant = 0;
        for (float ty = pathTop; ty < y + height; ty += tile) {
            for (int column = 0; column < 2; ++column) {
                const float tileH = std::min(tile, y + height - ty);
                sf::Sprite dirt(environmentTexture);
                dirt.setTextureRect(sf::IntRect({80 + 16 * (variant % 3), 0}, {16, static_cast<int>(tileH / 3.0f)}));
                dirt.setScale({3.0f, 3.0f});
                dirt.setPosition({pathX + static_cast<float>(column) * tile, ty});
                window.draw(dirt);
                ++variant;
            }
        }
        const sf::Color edge(141, 156, 80, 190), clear(141, 156, 80, 0);
        drawGradient(window, pathX, pathTop, 12.0f, y + height - pathTop, edge, clear, false);
        drawGradient(window, pathX + 2.0f * tile - 12.0f, pathTop, 12.0f, y + height - pathTop, clear, edge, false);
    }

    sprite(sf::IntRect({0, 55}, {64, 73}), {L.houseX, L.minY - 16.0f - 73.0f * 2.3f}, 2.3f);
    sprite(sf::IntRect({144, 48}, {32, 32}), {L.treeX, L.minY - 8.0f - 32.0f * 3.2f}, 3.2f);
    for (int i = 0; i < 5; ++i) {   // a row of mature plants from the crop-growth sprites
        sprite(sf::IntRect({128 + 16 * (i % 2), 144}, {16, 16}),
               {L.x + 90.0f + static_cast<float>(i) * 46.0f, L.minY - 6.0f - 16.0f * 2.8f}, 2.8f);
    }
}

void CompanionUI::drawSimulationScreen() {
    const SceneLayout L = sceneLayout();
    drawText(researcherMode ? "MAIN SIMULATION" : "COMPANION SIMULATION", 30, 75, 26, kInk);
    drawText(researcherMode ? "Both policies receive the same interpreted interaction."
                            : "Move with W A S D, then use the buttons or the message box to interact with the companion.",
             30, 106, 15, kMuted);

    // Wooden-style rounded frame around the yard
    drawRoundedBox(window, L.x - 8.0f, L.y - 5.0f, L.w + 16.0f, L.h + 16.0f, 16.0f, kShadowSoft);
    drawRoundedBox(window, L.x - 7.0f, L.y - 7.0f, L.w + 14.0f, L.h + 14.0f, 14.0f, sf::Color(196, 150, 112),
                   sf::Color(150, 106, 74), 2.0f);
    drawBackdrop(L.x, L.y, L.w, L.h);
    drawSceneObjects();

    // Characters further up the screen are drawn first so nearer ones overlap them.
    if (userY <= agentY) {
        drawPlayer(userX, userY);
        drawCompanion(agentX, agentY);
    } else {
        drawCompanion(agentX, agentY);
        drawPlayer(userX, userY);
    }
    // Golden-hour wash and soft vignette keep the yard warm and take the edge off its corners.
    sf::RectangleShape wash({L.w, L.h});
    wash.setPosition({L.x, L.y});
    wash.setFillColor(sf::Color(255, 214, 150, 22));
    window.draw(wash);
    const sf::Color shade(74, 44, 30, 70), none(74, 44, 30, 0);
    drawGradient(window, L.x, L.y, L.w, 46.0f, shade, none, true);
    drawGradient(window, L.x, L.y + L.h - 46.0f, L.w, 46.0f, none, shade, true);
    drawGradient(window, L.x, L.y, 46.0f, L.h, shade, none, false);
    drawGradient(window, L.x + L.w - 46.0f, L.y, 46.0f, L.h, none, shade, false);
    if (!pendingCue.empty()) drawBubble(pendingCue, pendingCueX, pendingCueY);
    drawText("YOU", userX - 14.0f, userY + 12.0f, 13, kInk);
    drawText(std::string("Companion ") + selectedAgentCode, agentX - 46.0f, agentY + 12.0f, 13, kInk);
    drawControlHelper();

    if (guidedStudyMode) {
        if (studyPhase == "consent") {
            drawText("STUDY INFORMATION: This is a voluntary study of two simulated animal companions.",
                     L.x + 275.0f, L.y + 18.0f, 16, sf::Color::White);
            drawText("No names or contact details are requested. Press Y to agree and continue, or N to decline.",
                     L.x + 275.0f, L.y + 44.0f, 15, sf::Color::White);
        } else if (studyPhase == "warm_up") {
            drawText("WARM-UP: Move with W A S D or arrow keys. Press Enter when ready.",
                     L.x + 275.0f, L.y + 18.0f, 17, sf::Color::White);
        } else if (studyPhase == "guided") {
            drawText(currentStudyPrompt(), L.x + 275.0f, L.y + 18.0f, 17, sf::Color::White);
            if (studyStepIndex == 8) {
                drawText("Observation: " + std::to_string(std::min(30, static_cast<int>(studyIdleElapsed))) +
                         " / 30 seconds", L.x + 275.0f, L.y + 44.0f, 15, sf::Color::White);
            }
        } else if (studyPhase == "free") {
            drawText("Optional free interaction. Click Finish Free Phase when ready (maximum 3 minutes).",
                     L.x + 275.0f, L.y + 18.0f, 16, sf::Color::White);
        } else if (studyPhase == "rating") {
            drawText(studyFormOpenedCurrentCondition
                         ? "Complete the form in your browser, then return and continue."
                         : "Guided interaction complete. Open Answer Form before continuing.",
                     L.x + 275.0f, L.y + 18.0f, 16, sf::Color::White);
        } else if (studyPhase == "finished") {
            drawText("Study sessions complete. Thank you.", L.x + 275.0f, L.y + 18.0f, 17, sf::Color::White);
        }
    }

    if (researcherMode) {
        const std::string visibleAction = selectedAgentIsProposed ? latestProposedAction : latestBaselineAction;
        drawText(std::string("Agent ") + selectedAgentCode + " action: " + visibleAction, L.x + 280.0f, L.y + 14.0f, 16, sf::Color::White);
        drawText("Input interpretation: " + nlpMethod, L.x + 280.0f, L.y + 38.0f, 14, sf::Color::White);
    }
    if (!controlsVisible) {
        drawText("Controls hidden - use Show Controls to interact", L.x + 280.0f, L.y + 64.0f, 16, sf::Color::White);
        drawHover();
        return;
    }

    const float firstRowY = L.y + L.h + 15.0f;
    for (std::size_t i = 0; i < kControls.size(); ++i) {
        drawButton(kControls[i].label,
                   kButtonX0 + static_cast<float>(i % 5) * (kButtonW + kButtonGap),
                   firstRowY + static_cast<float>(i / 5) * kButtonRowStep, kButtonW, kButtonH);
    }
    drawButton(textInput.empty() ? "Click here to type a message, then press Enter" : textInput,
               35, kInputY, 980, kInputH, textInputFocused);
    drawButton("Send", 1030, kInputY, 120, kInputH);
    drawText("Enter to send | Esc to quit", 850, kInputY + 54.0f, 13, kMuted);

    if (researcherMode) {
        drawText("Recent activity", 35, 768, 18, kInk);
        float logY = 794;
        for (auto it = eventLog.rbegin(); it != eventLog.rend() && logY < 880; ++it, logY += 19)
            drawText(*it, 45, logY, 13, kMuted);
    }
    drawHover();
}


void CompanionUI::drawAgentStateScreen() {
    drawText("AGENT STATE", 30, 75, 26, kInk);
    drawText("Current context, affect and retrieved memory", 30, 106, 15, kMuted);
    drawButton("Clear Memory Files", 930, 750, 280, 44);
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
             affect.valence == AffectValence::NEGATIVE ? "Negative" : "Neutral", 150, 350, 15, kAccent);
    drawText("Current action: " + latestProposedAction, 50, 380, 15, kInk);

    drawText("Current interaction context", 660, 158, 19, kAccent);
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
    drawText("BASELINE AGENT", 55, 165, 20, kAccent);
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
    if (screen == Screen::AgentSelection) {
        drawAgentSelectionScreen();
    } else {
        drawNavigation();
        if (screen == Screen::Simulation) drawSimulationScreen();
        else if (screen == Screen::AgentState) drawAgentStateScreen();
        else drawComparisonScreen();
    }
    window.display();
}

void CompanionUI::saveSession() {
    try {
        const auto directory = std::filesystem::path(outputDirectory);
        std::filesystem::create_directories(directory);
        if (guidedStudyMode) {
            nlohmann::json session = {
                {"schema_version", 1}, {"protocol_id", "companion-believability-v0.1"},
                {"run_id", runId}, {"participant_code", studyParticipantCode},
                {"session_index", studySessionIndex},
                {"condition_code", std::string(1, selectedAgentCode)},
                {"condition", selectedAgentIsProposed ? "proposed" : "baseline"},
                {"phase", studyPhase}, {"interaction_count", interactionCount},
                {"software_revision", IS2_GIT_REVISION},
                {"policy_updates_enabled", false},
                {"latest_stimulus", latestStimulus},
                {"visible_action", selectedAgentIsProposed ? latestProposedAction : latestBaselineAction},
                {"interactions", eventLog}
            };
            std::ofstream out(directory / (runId + "-session.json"));
            if (out) out << session.dump(2) << '\n';
            return; // Study mode must never overwrite shared training policy files.
        }
        nlohmann::json session;
        session["run_id"] = runId;
        session["condition_code"] = std::string(1, selectedAgentCode);
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
        proposed.savePersistentContext((std::filesystem::path(policyDirectory) / "proposed.qtable").string());
    } catch (const std::exception& e) {
        std::cerr << "Unable to save simulation session: " << e.what() << '\n';
    }
}
