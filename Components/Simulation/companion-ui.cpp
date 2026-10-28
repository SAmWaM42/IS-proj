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
const sf::Color kBackground(239, 243, 247);
const sf::Color kPanel(255, 255, 255);
const sf::Color kInk(35, 42, 52);
const sf::Color kMuted(105, 115, 128);
const sf::Color kBlue(58, 112, 170);
const sf::Color kGreen(71, 145, 106);

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

// ---- Layout and scene constants -------------------------------------------
constexpr float kSceneX = 30.0f;
constexpr float kSceneY = 135.0f;
constexpr float kSceneW = 1218.0f;
constexpr float kSceneHControls = 430.0f;
constexpr float kSceneHFull = 710.0f;
constexpr float kDogScale = 6.0f;  // the supplied dog frames are 16 px tall
constexpr float kPi = 3.14159265f;
constexpr float kButtonW = 215.0f;
constexpr float kButtonH = 44.0f;
constexpr float kButtonGap = 20.0f;
constexpr float kButtonX0 = 40.0f;
constexpr float kButtonRowStep = 52.0f;
constexpr float kInputY = 692.0f;
constexpr float kInputH = 48.0f;
const sf::Color kShadow(0, 0, 0, 60);

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
};
const std::array<ControlButton, 10> kControls{{
    {"Greet",      "Greet the companion",                  0, 1, 0, 0, 0.55f, false, false,  0.65f, ""},
    {"Feed",       "Offer the companion food",             2, 0, 1, 0, 0.30f, true,  false,  0.50f, "feed"},
    {"Play",       "Invite the companion to play",         0, 1, 1, 0, 0.70f, false, true,   0.50f, ""},
    {"Praise",     "Praise the companion",                 0, 0, 1, 0, 0.75f, false, false,  1.00f, ""},
    {"Scold",      "Scold the companion",                  0, 1, 2, 0, 0.45f, false, false, -1.00f, ""},
    {"Explore",    "Let the companion explore",            2, 1, 0, 0, 0.30f, false, false,  0.25f, ""},
    {"Rest",       "The companion should rest",            2, 2, 0, 0, 0.20f, false, false,  0.25f, ""},
    {"Loud noise", "A sudden loud noise occurs",           2, 0, 0, 0, 0.95f, false, false, -0.15f, ""},
    {"Fetch ball", "Ask the companion to fetch the ball",  1, 1, 1, 1, 0.85f, false, false,  0.85f, "fetch_ball"},
    {"Throw ball", "Throw the ball for the companion",     1, 1, 1, 1, 0.85f, false, false,  0.85f, "throw_ball"},
}};
}

CompanionUI::CompanionUI(const std::string& policyDirectoryValue,
                         const std::string& outputDirectoryValue,
                         bool researcherModeValue)
    : window(sf::VideoMode(sf::Vector2u(1280u, 900u)), "Animal Agent | Companion Simulation"),
      policyDirectory(policyDirectoryValue),
      outputDirectory(outputDirectoryValue),
      baseline(EnvironmentConfig{}),
      proposed(EnvironmentConfig{}) {
    researcherMode = researcherModeValue;
    baseline.setLearningEnabled(false);
    proposed.setLearningEnabled(false);
    baseline.setExplorationRate(0.0f);
    proposed.setExplorationRate(0.0f);
    const auto runTicks = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    runId = "run-" + std::to_string(runTicks);
    std::random_device conditionRandom;
    agentAIsProposed = (conditionRandom() % 2u) == 0u;
    if (!font.openFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")) {
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
    if (std::filesystem::exists(basePath)) baseline.loadPersistentState(basePath.string());
    if (std::filesystem::exists(proposedPath)) proposed.loadPersistentState(proposedPath.string());
    latestObservation = EnvironmentObservation{0, 1, 0, 0, 0.35f};
    resetScene();
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

CompanionUI::SceneLayout CompanionUI::sceneLayout() const {
    SceneLayout L;
    L.x = kSceneX;
    L.y = kSceneY;
    L.w = kSceneW;
    L.h = controlsVisible ? kSceneHControls : kSceneHFull;
    L.groundY = L.y + L.h * 0.80f;   // the line every character and object stands on
    L.userX = L.x + 160.0f;
    L.bedX = L.x + 390.0f;
    L.ballHomeX = L.x + 560.0f;
    L.bowlX = L.x + 720.0f;
    L.treeX = L.x + 820.0f;          // left edge of the tree sprite
    L.houseX = L.x + 975.0f;         // left edge of the house sprite
    L.minX = L.userX + 80.0f;        // the dog never walks through the player
    L.maxX = L.x + 900.0f;
    return L;
}

std::vector<CompanionUI::SceneObject> CompanionUI::sceneObjects() const {
    const SceneLayout L = sceneLayout();
    std::vector<SceneObject> objects;
    // Background first; the LAST match wins when hovering, so foreground items go last.
    objects.push_back(SceneObject{"House", sf::FloatRect({L.houseX, L.groundY - 30.0f - 219.0f}, {192.0f, 219.0f})});
    objects.push_back(SceneObject{"Tree", sf::FloatRect({L.treeX, L.groundY - 20.0f - 128.0f}, {128.0f, 128.0f})});
    objects.push_back(SceneObject{"Dog bed", sf::FloatRect({L.bedX - 62.0f, L.groundY - 30.0f}, {124.0f, 40.0f})});
    objects.push_back(SceneObject{foodPresent ? "Food bowl (full)" : "Food bowl (empty)",
                                  sf::FloatRect({L.bowlX - 30.0f, L.groundY - 26.0f}, {60.0f, 32.0f})});
    objects.push_back(SceneObject{"You", sf::FloatRect({L.userX - 22.0f, L.groundY - 122.0f}, {44.0f, 122.0f})});
    objects.push_back(SceneObject{"Companion", sf::FloatRect({agentX - 48.0f, L.groundY - 96.0f}, {96.0f, 96.0f})});
    if (!ballCarried)
        objects.push_back(SceneObject{"Ball", sf::FloatRect({ballX - 15.0f, L.groundY - ballHeight - 26.0f}, {30.0f, 30.0f})});
    return objects;
}

const std::vector<std::unique_ptr<sf::Texture>>& CompanionUI::framesForPose(const std::string& pose) const {
    if (pose == "walk") return walkFrames;
    if (pose == "sit") return sitFrames;
    if (pose == "lay") return layFrames;
    if (pose == "sleep") return sleepFrames;
    if (pose == "bark") return barkFrames;
    return idleFrames;
}

void CompanionUI::resetScene() {
    const SceneLayout L = sceneLayout();
    totalSimulationElapsedSeconds = 0.0f;
    behaviourPlan.clear();
    stepIndex = 0;
    stepTime = 0.0f;
    behaviourActive = false;
    restingPose = "idle";
    animationFrame = 0;
    frameTimer = 0.0f;
    agentX = L.userX + 360.0f;
    facingRight = false;
    ballX = L.ballHomeX;
    ballLandX = L.ballHomeX;
    ballHeight = 0.0f;
    ballCarried = false;
    ballFlying = false;
    throwTimer = 0.0f;
    foodPresent = false;
    interactionBuffer.clear();
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

void CompanionUI::openStudyForm() {
    const std::string code(1, selectedAgentCode);
    const char* configuredUrl = std::getenv("ANIMAL_AGENT_STUDY_FORM_URL");
    if (configuredUrl == nullptr || *configuredUrl == '\0') {
        appendLog("Study questionnaire URL is not configured. Set ANIMAL_AGENT_STUDY_FORM_URL.");
        return;
    }
    const std::string baseUrl(configuredUrl);
    const std::string url = baseUrl + (baseUrl.find('?') == std::string::npos ? "?" : "&") +
                            "condition=" + code + "&run=" + runId;
    {
        nlohmann::json row;
        row["event"] = "study_form_opened";
        row["run_id"] = runId;
        row["condition_code"] = code;
        row["timestamp"] = utcTimestamp();
        row["interaction_count"] = interactionCount;
        std::ofstream log(std::filesystem::path(outputDirectory) / ("study-events-" + runId + ".jsonl"), std::ios::app);
        if (log) log << row.dump() << '\n';
    }
    appendLog("Opened study form for condition " + code);
    const pid_t browser = fork();
    if (browser == 0) {
        execlp("xdg-open", "xdg-open", url.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
    if (browser < 0)
        appendLog("Could not start the browser for questionnaire URL: " + url);
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
        ballLandX = L.ballHomeX;
    } else if (sceneEvent == "throw_ball") {
        std::uniform_real_distribution<float> landing(L.userX + 380.0f, L.maxX);
        ballCarried = false;
        ballStartX = L.userX + 30.0f;
        ballLandX = landing(sceneRng);
        ballX = ballStartX;
        ballHeight = 105.0f;
        ballFlightTime = 0.0f;
        ballFlying = true;
        throwTimer = 0.5f;
    }
}

void CompanionUI::beginStep() {
    BehaviourStep& s = behaviourPlan[stepIndex];
    stepTime = 0.0f;
    animationFrame = 0;
    frameTimer = 0.0f;
    if (s.faceDir != 0) facingRight = s.faceDir > 0;
    if (s.pickUpBall) {
        ballCarried = true;
        ballFlying = false;
        ballHeight = 0.0f;
    }
    if (s.dropBall) {
        const SceneLayout L = sceneLayout();
        ballCarried = false;
        ballX = std::max(L.userX + 45.0f, agentX + (facingRight ? 34.0f : -34.0f));
        ballLandX = ballX;
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
    const float nearPlayerX = L.userX + 120.0f;

    auto walkTo = [&](float x, float speed, float frameInterval) {
        BehaviourStep s;
        s.pose = "walk";
        s.targetX = std::clamp(x, L.minX, L.maxX);
        s.speed = speed;
        s.frameInterval = frameInterval;
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
        // Ball position: where it will land (throw) or where it currently lies.
        const float ballTarget = ballFlying ? ballLandX : ballX;
        if (sceneEvent == "throw_ball") {
            BehaviourStep watch = stay("idle", 0.55f, "!");   // watch the ball leave the hand
            watch.faceDir = 1;
            behaviourPlan.push_back(watch);
        }
        const float mouthOffset = (ballTarget >= agentX) ? -34.0f : 34.0f;   // stop so the mouth reaches the ball
        behaviourPlan.push_back(walkTo(ballTarget + mouthOffset, 300.0f, 0.08f));
        BehaviourStep pick = stay("idle", 0.35f, "");
        pick.pickUpBall = true;
        behaviourPlan.push_back(pick);
        behaviourPlan.push_back(walkTo(L.userX + 95.0f, 240.0f, 0.09f));
        BehaviourStep drop = stay("sit", 1.0f, "Got it!");   // ball is placed at the player's feet
        drop.dropBall = true;
        drop.faceDir = -1;
        behaviourPlan.push_back(drop);
    } else if (action == "SNIFF") {
        const float target = (!ballCarried && ballX > L.userX + 150.0f) ? ballX : L.bowlX;
        const float stand = (target >= agentX) ? target - 55.0f : target + 55.0f;
        BehaviourStep w = walkTo(stand, 70.0f, 0.22f);
        w.tiltBias = 6.0f;
        behaviourPlan.push_back(w);
        BehaviourStep s = stay("idle", 1.2f, "sniff");
        s.tiltBias = 9.0f;
        s.tilt = 3.0f;
        s.tiltRate = 2.0f;
        s.faceDir = (target >= stand) ? 1 : -1;
        behaviourPlan.push_back(s);
    } else if (action == "WANDER") {
        std::uniform_real_distribution<float> pick(L.minX, L.maxX);
        float target = pick(sceneRng);
        if (std::fabs(target - agentX) < 140.0f)
            target = (agentX < (L.minX + L.maxX) * 0.5f) ? agentX + 220.0f : agentX - 220.0f;
        behaviourPlan.push_back(walkTo(target, 85.0f, 0.18f));
        BehaviourStep look = stay("idle", 0.7f, "");
        look.tilt = 5.0f;
        look.tiltRate = 1.0f;
        behaviourPlan.push_back(look);
    } else if (action == "APPROACH_FOLLOW") {
        behaviourPlan.push_back(walkTo(nearPlayerX, 150.0f, 0.12f));
        BehaviourStep s = stay("idle", 0.5f, "");
        s.faceDir = -1;
        behaviourPlan.push_back(s);
    } else if (action == "HIDE_AVOID") {
        BehaviourStep run = walkTo(L.maxX, 210.0f, 0.09f);
        run.crouch = 0.10f;
        run.tint = sf::Color(205, 205, 225);
        behaviourPlan.push_back(run);
        BehaviourStep cower = stay("sit", 1.4f, "...");
        cower.shiver = 2.0f;
        cower.crouch = 0.12f;
        cower.tint = sf::Color(205, 205, 225);
        cower.faceDir = -1;
        behaviourPlan.push_back(cower);
    } else if (action == "WHINE") {
        BehaviourStep s = stay("sit", 1.7f, "*whine*");
        s.shiver = 1.5f;
        s.crouch = 0.08f;
        s.tint = sf::Color(215, 215, 235);
        s.faceDir = -1;
        behaviourPlan.push_back(s);
    } else if (action == "BARK") {
        BehaviourStep s = stay("bark", 1.1f, "WOOF!");
        s.frameInterval = 0.12f;
        s.hop = 14.0f;
        s.hopRate = 3.5f;
        behaviourPlan.push_back(s);
    } else if (action == "INSIST") {
        behaviourPlan.push_back(walkTo(nearPlayerX, 200.0f, 0.10f));
        BehaviourStep s = stay("bark", 1.7f, "!!");
        s.frameInterval = 0.14f;
        s.hop = 22.0f;
        s.hopRate = 2.5f;
        s.faceDir = -1;
        behaviourPlan.push_back(s);
    } else if (action == "ACT_CONFUSED") {
        BehaviourStep s = stay("idle", 1.5f, "?");
        s.tilt = 9.0f;
        s.tiltRate = 0.9f;
        behaviourPlan.push_back(s);
    } else if (action == "GREETING") {
        behaviourPlan.push_back(walkTo(nearPlayerX, 170.0f, 0.12f));
        BehaviourStep s = stay("sit", 0.9f, "Hi!");
        s.hop = 8.0f;
        s.hopRate = 2.5f;
        s.faceDir = -1;
        behaviourPlan.push_back(s);
    } else if (action == "NUZZLE") {
        behaviourPlan.push_back(walkTo(L.userX + 85.0f, 150.0f, 0.14f));
        BehaviourStep s = stay("sit", 1.2f, "<3");
        s.tilt = 5.0f;
        s.tiltRate = 1.0f;
        s.faceDir = -1;
        behaviourPlan.push_back(s);
    } else if (action == "PLAY") {
        behaviourPlan.push_back(walkTo(nearPlayerX + 40.0f, 210.0f, 0.09f));
        BehaviourStep s = stay("idle", 2.0f, "yay!");
        s.hop = 30.0f;
        s.hopRate = 2.0f;
        s.faceDir = -1;
        behaviourPlan.push_back(s);
    } else if (action == "EAT") {
        behaviourPlan.push_back(walkTo(L.bowlX - 75.0f, 130.0f, 0.14f));
        BehaviourStep eat = stay("idle", 2.0f, "yum!");
        eat.faceDir = 1;
        eat.tiltBias = 13.0f;
        eat.tilt = 4.0f;
        eat.tiltRate = 3.0f;
        eat.consumeFood = true;
        behaviourPlan.push_back(eat);
    } else if (action == "SLEEP_LAY") {
        behaviourPlan.push_back(walkTo(L.bedX, 100.0f, 0.20f));
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

void CompanionUI::updateAnimation(float elapsedSeconds) {
    elapsedSeconds = std::min(elapsedSeconds, 0.1f);   // avoid jumps after the window is dragged
    sceneTime += elapsedSeconds;
    totalSimulationElapsedSeconds += elapsedSeconds;
    proposed.advanceTime(elapsedSeconds);
    if (throwTimer > 0.0f) throwTimer = std::max(0.0f, throwTimer - elapsedSeconds);

    if (ballFlying) {
        ballFlightTime += elapsedSeconds;
        const float p = std::min(1.0f, ballFlightTime / 0.85f);
        ballX = ballStartX + (ballLandX - ballStartX) * p;
        ballHeight = 105.0f * (1.0f - p) + 150.0f * 4.0f * p * (1.0f - p);
        if (p >= 1.0f) {
            ballFlying = false;
            ballX = ballLandX;
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
        if (s.targetX >= 0.0f) {
            const float dx = s.targetX - agentX;
            if (s.faceDir == 0 && std::fabs(dx) > 1.0f) facingRight = dx > 0.0f;
            const float move = s.speed * elapsedSeconds;
            if (std::fabs(dx) <= move) {
                agentX = s.targetX;
                done = true;
            } else {
                agentX += (dx > 0.0f) ? move : -move;
            }
            if (stepTime > 10.0f) done = true;   // safety net
        } else if (stepTime >= s.duration) {
            done = true;
        }
        if (done) finishStep();   // s may be invalid after this call
    }

    if (!behaviourActive && !interactionBuffer.empty()) {
        const auto next = interactionBuffer.front();
        interactionBuffer.pop_front();
        autonomousDecisionTimer = 0.0f;
        executeInteraction(next.label, next.observation, next.reward, next.preference,
                           next.preferencePositive, next.sceneEvent);
    } else if (!behaviourActive && interactionBuffer.empty()) {
        // The environment continues to evolve when the user is idle. Refresh
        // observable affordances and let both policies choose from that context;
        // only the selected A/B condition is rendered. No synthetic user reward
        // or memory event is recorded for this autonomous decision.
        autonomousDecisionTimer += elapsedSeconds;
        if (autonomousDecisionTimer >= autonomousDecisionInterval &&
            screen == Screen::Simulation) {
            const float idleSecondsBeforeDecision = autonomousDecisionTimer;
            autonomousDecisionTimer = 0.0f;
            EnvironmentObservation ambient = latestObservation;
            ambient.goal_state = 2;       // no new user request: ambient context
            ambient.user_status = 0;
            ambient.task_status = 0;
            ambient.signal_strength = 0.0f;
            ambient.food_available = foodPresent;
            ambient.play_available = !ballCarried && !ballFlying;
            const float targetDistance = std::fabs(agentX - ballX);
            ambient.target_proximity = targetDistance < 120.0f ? 0 :
                                       targetDistance < 300.0f ? 1 : 2;

            latestBaselineAction = baseline.selectAction(ambient);
            latestProposedAction = proposed.selectAction(ambient);

            // Researcher-only trace of the real UI idle loop. Unlike the
            // fixed-context sweep, this records the actual scene-derived
            // observation after the previous behaviour has completed.
            if (researcherMode) {
                const ActionDecisionTrace& trace = proposed.getLastDecisionTrace();
                nlohmann::json qValues = nlohmann::json::array();
                for (const auto& item : trace.valid_action_q_values) {
                    qValues.push_back({{"action", item.first}, {"q_value", item.second}});
                }
                nlohmann::json row;
                row["event"] = "autonomous_policy_decision";
                row["run_id"] = runId;
                row["simulation_elapsed_seconds"] = totalSimulationElapsedSeconds;
                row["idle_seconds_before_decision"] = idleSecondsBeforeDecision;
                row["idle_threshold_seconds"] = autonomousDecisionInterval;
                row["selected_condition"] = selectedAgentIsProposed ? "proposed" : "baseline";
                row["baseline_action"] = latestBaselineAction;
                row["proposed_action"] = latestProposedAction;
                row["observation"] = {
                    {"goal_state", ambient.goal_state},
                    {"target_proximity", ambient.target_proximity},
                    {"user_status", ambient.user_status},
                    {"task_status", ambient.task_status},
                    {"signal_strength", ambient.signal_strength},
                    {"food_available", ambient.food_available},
                    {"play_available", ambient.play_available}
                };
                row["proposed"] = {
                    {"encoded_state", trace.encoded_state},
                    {"contextual_fallback", trace.contextual_fallback},
                    {"policy_action_before_overrides", trace.policy_action},
                    {"final_action_after_overrides", trace.final_action},
                    {"general_preference_override", trace.general_preference_override},
                    {"rejection_override_applied", trace.rejection_override_applied},
                    {"memory_record_count", trace.memory_record_count},
                    {"affect", {
                        {"valence", static_cast<int>(trace.affect.valence)},
                        {"energy", static_cast<int>(trace.affect.energy)},
                        {"arousal", trace.affect.arousal},
                        {"stress", trace.affect.stress},
                        {"engagement", trace.affect.engagement},
                        {"affinity", trace.affect.affinity}
                    }},
                    {"valid_action_q_values", qValues}
                };
                std::ofstream diagnostic(
                    std::filesystem::path(outputDirectory) / "idle-runtime-diagnostics.jsonl",
                    std::ios::app);
                if (diagnostic) diagnostic << row.dump() << '\n';
            }

            latestObservation = ambient;
            latestStimulus = "The companion responds to its surroundings.";
            startBehaviour(selectedAgentIsProposed ? latestProposedAction : latestBaselineAction, "");
            if (researcherMode) {
                appendLog("Autonomous environment-driven action: " +
                          (selectedAgentIsProposed ? latestProposedAction : latestBaselineAction));
            }
        }
    }
}

void CompanionUI::drawBubble(const std::string& value, float x, float bottomY) {
    sf::Text label(font, value, 20);
    label.setFillColor(kInk);
    const sf::FloatRect bounds = label.getLocalBounds();
    const float padX = 12.0f, padY = 7.0f;
    const float w = bounds.size.x + padX * 2.0f;
    const float h = bounds.size.y + padY * 2.0f;
    const float top = bottomY - h - 10.0f;
    sf::RectangleShape box({w, h});
    box.setPosition({x - w * 0.5f, top});
    box.setFillColor(sf::Color(255, 255, 255, 240));
    box.setOutlineColor(kInk);
    box.setOutlineThickness(2.0f);
    window.draw(box);
    sf::ConvexShape tail;
    tail.setPointCount(3);
    tail.setPoint(0, {x - 7.0f, top + h});
    tail.setPoint(1, {x + 7.0f, top + h});
    tail.setPoint(2, {x, bottomY});
    tail.setFillColor(sf::Color(255, 255, 255, 240));
    window.draw(tail);
    label.setPosition({x - w * 0.5f + padX - bounds.position.x, top + padY - bounds.position.y});
    window.draw(label);
}

void CompanionUI::drawCompanion(float x, float groundY) {
    const BehaviourStep* step =
        (behaviourActive && stepIndex < behaviourPlan.size()) ? &behaviourPlan[stepIndex] : nullptr;
    const std::string pose = step ? step->pose : restingPose;
    const auto& frames = framesForPose(pose);

    sf::CircleShape shadow(40.0f);
    shadow.setOrigin({40.0f, 40.0f});
    shadow.setScale({1.0f, 0.18f});
    shadow.setPosition({x, groundY + 2.0f});
    shadow.setFillColor(kShadow);
    window.draw(shadow);

    if (frames.empty()) {
        sf::CircleShape placeholder(30.0f);
        placeholder.setOrigin({30.0f, 60.0f});
        placeholder.setPosition({x, groundY});
        placeholder.setFillColor(kBlue);
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
    dog.setPosition({std::round(x + shiver), std::round(groundY - hopOffset)});
    window.draw(dog);

    if (ballCarried) {
        const float mouthX = x + (facingRight ? 34.0f : -34.0f);
        const float mouthY = groundY - hopOffset - 36.0f;
        sf::CircleShape ball(11.0f);
        ball.setOrigin({11.0f, 11.0f});
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
        drawBubble(cue, x, groundY - hopOffset - visiblePx * kDogScale * (1.0f - crouch) - 4.0f);
    }
}

void CompanionUI::drawPlayer(float x, float groundY) {
    // Simple silhouette; swap this function for a sprite-sheet player later.
    const sf::Color body(46, 52, 64);
    sf::RectangleShape leg({11.0f, 38.0f});
    leg.setFillColor(body);
    leg.setPosition({x - 13.0f, groundY - 38.0f});
    window.draw(leg);
    leg.setPosition({x + 2.0f, groundY - 38.0f});
    window.draw(leg);
    sf::RectangleShape torso({30.0f, 50.0f});
    torso.setFillColor(body);
    torso.setPosition({x - 15.0f, groundY - 88.0f});
    window.draw(torso);
    sf::CircleShape head(14.0f);
    head.setFillColor(body);
    head.setPosition({x - 14.0f, groundY - 116.0f});
    window.draw(head);
    float armAngle = 6.0f;
    if (throwTimer > 0.0f) {
        const float p = 1.0f - throwTimer / 0.5f;
        armAngle = 6.0f - 170.0f * std::sin(kPi * p);   // swing up and forward when throwing
    }
    sf::RectangleShape arm({9.0f, 40.0f});
    arm.setFillColor(sf::Color(62, 69, 82));
    arm.setOrigin({4.5f, 4.0f});
    arm.setPosition({x + 10.0f, groundY - 82.0f});
    arm.setRotation(sf::degrees(armAngle));
    window.draw(arm);
}

void CompanionUI::drawSceneObjects() {
    const SceneLayout L = sceneLayout();

    // Dog bed
    sf::CircleShape bedOuter(62.0f);
    bedOuter.setOrigin({62.0f, 62.0f});
    bedOuter.setScale({1.0f, 0.30f});
    bedOuter.setPosition({L.bedX, L.groundY + 2.0f});
    bedOuter.setFillColor(sf::Color(121, 82, 64));
    window.draw(bedOuter);
    sf::CircleShape bedInner(48.0f);
    bedInner.setOrigin({48.0f, 48.0f});
    bedInner.setScale({1.0f, 0.26f});
    bedInner.setPosition({L.bedX, L.groundY});
    bedInner.setFillColor(sf::Color(176, 128, 98));
    window.draw(bedInner);

    // Food bowl
    sf::ConvexShape bowl;
    bowl.setPointCount(4);
    bowl.setPoint(0, {L.bowlX - 28.0f, L.groundY - 22.0f});
    bowl.setPoint(1, {L.bowlX + 28.0f, L.groundY - 22.0f});
    bowl.setPoint(2, {L.bowlX + 20.0f, L.groundY + 2.0f});
    bowl.setPoint(3, {L.bowlX - 20.0f, L.groundY + 2.0f});
    bowl.setFillColor(sf::Color(96, 120, 150));
    bowl.setOutlineColor(sf::Color(60, 76, 98));
    bowl.setOutlineThickness(2.0f);
    window.draw(bowl);
    sf::RectangleShape rim({60.0f, 5.0f});
    rim.setPosition({L.bowlX - 30.0f, L.groundY - 25.0f});
    rim.setFillColor(sf::Color(130, 155, 185));
    window.draw(rim);
    if (foodPresent) {
        for (int i = 0; i < 8; ++i) {
            sf::CircleShape kibble(4.0f);
            kibble.setOrigin({4.0f, 4.0f});
            kibble.setPosition({L.bowlX - 21.0f + static_cast<float>(i) * 6.0f,
                                L.groundY - 27.0f - static_cast<float>((i * 3) % 4)});
            kibble.setFillColor(sf::Color(150, 98, 60));
            window.draw(kibble);
        }
    }

    // Ball on the ground or in flight (the carried ball is drawn with the dog)
    if (!ballCarried) {
        if (ballHeight > 1.0f) {
            sf::CircleShape ballShadow(10.0f);
            ballShadow.setOrigin({10.0f, 10.0f});
            ballShadow.setScale({1.0f, 0.3f});
            ballShadow.setPosition({ballX, L.groundY + 2.0f});
            ballShadow.setFillColor(kShadow);
            window.draw(ballShadow);
        }
        sf::CircleShape ball(11.0f);
        ball.setOrigin({11.0f, 11.0f});
        ball.setPosition({ballX, L.groundY - ballHeight - 11.0f});
        ball.setFillColor(sf::Color(204, 62, 56));
        ball.setOutlineColor(sf::Color(110, 30, 28));
        ball.setOutlineThickness(2.0f);
        window.draw(ball);
        sf::CircleShape shine(3.0f);
        shine.setOrigin({3.0f, 3.0f});
        shine.setPosition({ballX - 4.0f, L.groundY - ballHeight - 15.0f});
        shine.setFillColor(sf::Color(255, 220, 215));
        window.draw(shine);
    }
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

    sf::RectangleShape outline({hovered->bounds.size.x, hovered->bounds.size.y});
    outline.setPosition(hovered->bounds.position);
    outline.setFillColor(sf::Color(255, 255, 255, 30));
    outline.setOutlineColor(sf::Color(255, 255, 255, 230));
    outline.setOutlineThickness(2.0f);
    window.draw(outline);

    sf::Text tip(font, hovered->label, 15);
    tip.setFillColor(kInk);
    const sf::FloatRect b = tip.getLocalBounds();
    const float px = mouse.x + 14.0f, py = mouse.y - 34.0f;
    sf::RectangleShape panel({b.size.x + 18.0f, b.size.y + 14.0f});
    panel.setPosition({px, py});
    panel.setFillColor(sf::Color(255, 255, 255, 245));
    panel.setOutlineColor(kMuted);
    panel.setOutlineThickness(1.0f);
    window.draw(panel);
    tip.setPosition({px + 9.0f - b.position.x, py + 6.0f - b.position.y});
    window.draw(tip);
}

void CompanionUI::processInteraction(const std::string& label,
                                     const EnvironmentObservation& observation,
                                     float feedbackRewardValue,
                                     const std::string& preference,
                                     bool preferencePositive,
                                     const std::string& sceneEvent) {
    interactionBuffer.push_back(QueuedInteraction{label, observation, feedbackRewardValue, preference,
                                                  preferencePositive, sceneEvent});
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
    latestBaselineAction = baseline.selectAction(observation);
    latestProposedAction = proposed.selectAction(observation);
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
    applySceneEvent(sceneEvent);
    startBehaviour(selectedAgentIsProposed ? latestProposedAction : latestBaselineAction, sceneEvent);

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
            proposed.recordUserStatement(relation, object);
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
            appendLog("Memory: stored user " + relation + " " + object);
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
            row["input_summary"] = text;
            row["utterance_analysis"] = {
                {"kind", "preference_statement"}, {"relation", relation}, {"object", object},
                {"memory_updated", true}, {"response_required", false}, {"response_action", "NONE"},
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

    processInteraction(text + " [" + intent + " / " + nlpMethod + "]", observation, reward, "", true, sceneEvent);
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
            if (x >= 185 && x < 345) screen = Screen::AgentSelection;       // Change Model
            else if (x >= 357 && x < 517) openStudyForm();                  // Answer Form
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
    if (!controlsVisible) return;

    const SceneLayout L = sceneLayout();
    const float firstRowY = L.y + L.h + 15.0f;
    for (std::size_t i = 0; i < kControls.size(); ++i) {
        const float bx = kButtonX0 + static_cast<float>(i % 5) * (kButtonW + kButtonGap);
        const float by = firstRowY + static_cast<float>(i / 5) * kButtonRowStep;
        if (x >= bx && x < bx + kButtonW && y >= by && y < by + kButtonH) {
            const ControlButton& control = kControls[i];
            textInputFocused = false;
            processInteraction(control.stimulus,
                               makeObservation(control.goal, control.proximity, control.user, control.task,
                                               control.signal, control.food, control.play),
                               control.reward, "", true, control.sceneEvent);
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
    } else {
        drawButton("Change Model", 185, 12, 160, 42);
        drawButton("Answer Form", 357, 12, 160, 42);
    }
}

void CompanionUI::drawBackdrop(float x, float y, float width, float height) {
    // Side-on yard: sky, lawn, a dirt strip that every character stands on, then
    // atlas sprites. Source rectangles were measured from free.png (the house is
    // 64x73 starting at y=55; the earlier crop cut the door off).
    const SceneLayout L = sceneLayout();
    const float horizon = y + height * 0.42f;

    sf::RectangleShape sky({width, horizon - y});
    sky.setPosition({x, y});
    sky.setFillColor(sf::Color(158, 205, 232));
    window.draw(sky);

    sf::RectangleShape lawn({width, y + height - horizon});
    lawn.setPosition({x, horizon});
    lawn.setFillColor(sf::Color(122, 145, 78));
    window.draw(lawn);

    sf::RectangleShape track({width, 44.0f});
    track.setPosition({x, L.groundY - 10.0f});
    track.setFillColor(sf::Color(160, 135, 83));
    window.draw(track);

    auto sprite = [&](sf::IntRect source, sf::Vector2f position, float scale) {
        if (!environmentTextureLoaded) return;
        sf::Sprite item(environmentTexture);
        item.setTextureRect(source);
        item.setScale({scale, scale});
        item.setPosition(position);
        window.draw(item);
    };

    sprite(sf::IntRect({0, 55}, {64, 73}), {L.houseX, L.groundY - 30.0f - 73.0f * 3.0f}, 3.0f);
    sprite(sf::IntRect({144, 48}, {32, 32}), {L.treeX, L.groundY - 20.0f - 32.0f * 4.0f}, 4.0f);
    for (int i = 0; i < 4; ++i) {
        sprite(sf::IntRect({160, 133}, {16, 26}),
               {x + width - 250.0f + static_cast<float>(i) * 44.0f, y + height - 8.0f - 26.0f * 2.5f}, 2.5f);
    }
}

void CompanionUI::drawSimulationScreen() {
    const SceneLayout L = sceneLayout();
    drawText(researcherMode ? "MAIN SIMULATION" : "COMPANION SIMULATION", 30, 75, 26, kInk);
    drawText(researcherMode ? "Both policies receive the same interpreted interaction."
                            : "Use the buttons or the message box to interact with the companion.",
             30, 106, 15, kMuted);

    sf::RectangleShape scene({L.w, L.h});
    scene.setPosition({L.x, L.y});
    scene.setFillColor(sf::Color(122, 145, 78));
    scene.setOutlineColor(sf::Color(83, 104, 55));
    scene.setOutlineThickness(1);
    window.draw(scene);
    drawBackdrop(L.x, L.y, L.w, L.h);
    drawSceneObjects();
    drawPlayer(L.userX, L.groundY);
    drawCompanion(agentX, L.groundY);
    drawText("YOU", L.userX - 14.0f, L.groundY + 14.0f, 13, kInk);
    drawText(std::string("Companion ") + selectedAgentCode, agentX - 46.0f, L.groundY + 14.0f, 13, kInk);

    if (researcherMode) {
        const std::string visibleAction = selectedAgentIsProposed ? latestProposedAction : latestBaselineAction;
        drawText(std::string("Agent ") + selectedAgentCode + " action: " + visibleAction, 50, 150, 16, kGreen);
        drawText("Input interpretation: " + nlpMethod, 50, 172, 14, kMuted);
    }
    if (!controlsVisible) {
        drawText("Controls hidden - use Show Controls to interact", 50, 200, 16, kInk);
        drawHover();
        return;
    }

    const float firstRowY = L.y + L.h + 15.0f;
    for (std::size_t i = 0; i < kControls.size(); ++i) {
        drawButton(kControls[i].label,
                   kButtonX0 + static_cast<float>(i % 5) * (kButtonW + kButtonGap),
                   firstRowY + static_cast<float>(i / 5) * kButtonRowStep, kButtonW, kButtonH);
    }
    drawButton(textInput.empty() ? "Type a message here, then press Enter" : textInput,
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
