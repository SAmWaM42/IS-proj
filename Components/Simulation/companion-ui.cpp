#include "companion-ui.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <ctime>
#include <cmath>
#include <cstdio>
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

enum class AnimationGroup { Idle, Walk, Sleep, Bark, Sit, Lay };

AnimationGroup animationGroupForAction(const std::string& action) {
    if (action == "FETCH" || action == "WANDER" || action == "APPROACH_FOLLOW" ||
        action == "SNIFF" || action == "HIDE_AVOID" || action == "PLAY")
        return AnimationGroup::Walk;
    if (action == "SLEEP_LAY") return AnimationGroup::Sleep;
    if (action == "BARK" || action == "ACT_CONFUSED" || action == "WHINE")
        return AnimationGroup::Bark;
    if (action == "NUZZLE" || action == "GREETING" || action == "EAT")
        return AnimationGroup::Sit;
    if (action == "INSIST")
        return AnimationGroup::Lay;
    return AnimationGroup::Idle;
}

const std::vector<std::unique_ptr<sf::Texture>>& framesForAction(
    const std::string& action,
    const std::vector<std::unique_ptr<sf::Texture>>& idle,
    const std::vector<std::unique_ptr<sf::Texture>>& walk,
    const std::vector<std::unique_ptr<sf::Texture>>& sleep,
    const std::vector<std::unique_ptr<sf::Texture>>& bark,
    const std::vector<std::unique_ptr<sf::Texture>>& sit,
    const std::vector<std::unique_ptr<sf::Texture>>& lay) {
    switch (animationGroupForAction(action)) {
        case AnimationGroup::Walk: return walk;
        case AnimationGroup::Sleep: return sleep;
        case AnimationGroup::Bark: return bark;
        case AnimationGroup::Sit: return sit;
        case AnimationGroup::Lay: return lay;
        case AnimationGroup::Idle: return idle;
    }
    return idle;
}
std::string levelText(float value) {
    if (value < 0.34f) return "Low";
    if (value < 0.67f) return "Medium";
    return "High";
}
float clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }

struct RuntimeTransition {
    EnvironmentObservation next;
    float reward = 0.0f;
    float energy = 0.8f;
    float stress = 0.2f;
    float satisfaction = 0.5f;
    bool completed = false;
};

RuntimeTransition stepInteractiveWorld(const std::string& action,
                                       const EnvironmentObservation& observation,
                                       float energyBefore,
                                       float stressBefore,
                                       float satisfactionBefore,
                                       float userFeedback,
                                       std::mt19937& rng) {
    RuntimeTransition result;
    result.next = observation;
    float energy = clamp01(energyBefore);
    float stress = clamp01(stressBefore);
    float satisfaction = clamp01(satisfactionBefore);
    float effort = 0.012f;
    float fit = -0.10f;

    if (observation.goal_state == 1 && observation.task_status == 1) {
        if (action == "FETCH") fit = 0.75f;
        else if (action == "APPROACH_FOLLOW" && observation.target_proximity >= 2) fit = 0.30f;
        else fit = -0.25f;
    } else if (observation.food_available) {
        fit = action == "EAT" ? 0.85f : (action == "SNIFF" ? 0.20f : -0.12f);
    } else if (observation.play_available) {
        fit = action == "PLAY" ? 0.85f : (action == "APPROACH_FOLLOW" ? 0.30f : -0.12f);
    } else if (observation.goal_state == 2 && observation.signal_strength >= 0.80f) {
        fit = (action == "HIDE_AVOID" || action == "WHINE" || action == "BARK") ? 0.70f : -0.22f;
    } else if (observation.user_status == 2) {
        fit = (action == "HIDE_AVOID" || action == "ACT_CONFUSED" || action == "WHINE") ? 0.55f : -0.30f;
    } else if (observation.goal_state == 0) {
        fit = (action == "GREETING" || action == "NUZZLE" || action == "APPROACH_FOLLOW" || action == "PLAY") ? 0.65f : -0.08f;
    } else if (action == "SLEEP_LAY" || action == "WANDER" || action == "SNIFF") {
        fit = 0.25f;
    }

    if (action == "FETCH" && observation.task_status == 1 && observation.target_proximity < 3) {
        const float baseProbability[] = {0.96f, 0.78f, 0.52f};
        const int proximity = std::clamp(observation.target_proximity, 0, 2);
        std::bernoulli_distribution success(baseProbability[proximity] * (0.55f + 0.45f * energy));
        result.completed = success(rng);
        effort = (proximity == 0 ? 0.035f : proximity == 1 ? 0.085f : 0.16f) *
                 (1.0f + 0.8f * (1.0f - energy));
        if (result.completed) result.next.task_status = 0;
        else satisfaction -= 0.06f;
    } else if (action == "APPROACH_FOLLOW" && observation.target_proximity > 0 &&
               observation.target_proximity < 3 && observation.user_status != 2) {
        effort = 0.045f * (1.0f + 0.8f * (1.0f - energy));
        result.next.target_proximity = std::max(0, observation.target_proximity - 1);
    } else if (action == "SLEEP_LAY") {
        energy = std::min(1.0f, energy + 0.28f);
        stress = std::max(0.0f, stress - 0.28f);
        effort = 0.0f;
    } else if (action == "EAT" && observation.food_available) {
        energy = std::min(1.0f, energy + 0.24f);
        result.next.food_available = false;
        effort = 0.0f;
    } else if (action == "PLAY" && observation.play_available) {
        effort = 0.055f * (1.0f + 0.8f * (1.0f - energy));
        result.next.play_available = false;
        if (stress < 0.75f) stress = std::max(0.0f, stress - 0.08f);
    } else {
        energy = std::max(0.0f, energy - effort * 1.5f);
    }

    if (observation.goal_state == 2 && observation.signal_strength >= 0.80f)
        stress = std::min(1.0f, stress + 0.16f);
    if (observation.user_status == 2)
        stress = std::min(1.0f, stress + 0.10f);
    if (result.completed) stress = std::max(0.0f, stress - 0.04f);

    // User feedback is an observed external signal; action fit and world
    // consequences remain separate so rewards are not a fixed button lookup.
    const float feedbackTerm = 0.20f * std::clamp(userFeedback, -1.0f, 1.0f);
    const float completionTerm = result.completed ? 0.62f : 0.0f;
    const float driveTerm = action == "EAT" && observation.food_available ? 0.18f * (1.0f - energyBefore) :
                            action == "SLEEP_LAY" ? 0.18f * stressBefore : 0.0f;
    result.reward = std::clamp(fit + feedbackTerm + completionTerm + driveTerm - effort, -1.0f, 1.0f);
    if (result.completed) satisfaction = std::min(1.0f, satisfaction + 0.10f);
    else if (fit < 0.0f) satisfaction = std::max(0.0f, satisfaction - 0.025f);
    else satisfaction = std::min(1.0f, satisfaction + 0.025f);

    result.energy = clamp01(energy);
    result.stress = clamp01(stress);
    result.satisfaction = clamp01(satisfaction);
    return result;
}
std::string utcTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t value = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
    gmtime_r(&value, &utc);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return buffer;
}
}

CompanionUI::CompanionUI(const std::string& policyDirectoryValue,
                         const std::string& outputDirectoryValue)
    : window(sf::VideoMode(sf::Vector2u(1280u, 900u)), "Animal Agent | Companion Simulation"),
      policyDirectory(policyDirectoryValue),
      outputDirectory(outputDirectoryValue),
      baseline(EnvironmentConfig{}),
      proposed(EnvironmentConfig{}) {
    baseline.setLearningEnabled(true);
    proposed.setLearningEnabled(true);
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
    const std::string dogSheet = "Data/Assets/DogMegaPackFree/DogMegaPackFree/Dogs.png";
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
    dogTextureLoaded = dogTexture.loadFromFile(dogSheet);
    if (dogTextureLoaded) {
        const auto size = dogTexture.getSize();
        appendLog("Loaded dog sprite sheet (" + std::to_string(size.x) + "x" + std::to_string(size.y) + ")");
    } else {
        appendLog("Dog sprite sheet unavailable; using vector companion fallback.");
    }
    std::filesystem::create_directories(outputDirectory);
    std::filesystem::create_directories(policyDirectory);
    const auto basePath = std::filesystem::path(policyDirectory) / "baseline.qtable";
    const auto proposedPath = std::filesystem::path(policyDirectory) / "proposed.qtable";
    if (std::filesystem::exists(basePath)) baseline.loadPersistentState(basePath.string());
    if (std::filesystem::exists(proposedPath)) proposed.loadPersistentState(proposedPath.string());
    latestObservation = EnvironmentObservation{0, 1, 0, 0, 0.35f};
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

void CompanionUI::updateAnimation(float elapsedSeconds) {
    const std::string& visibleAction = selectedAgentIsProposed ? latestProposedAction : latestBaselineAction;
    if (actionDisplayTimer > 0.0f) {
        actionDisplayTimer = std::max(0.0f, actionDisplayTimer - elapsedSeconds);
        animationClock += elapsedSeconds;
        if (animationClock >= 0.16f) {
            animationClock = 0.0f;
            const auto* frames = &framesForAction(
                visibleAction, idleFrames, walkFrames, sleepFrames, barkFrames, sitFrames, layFrames);
            const std::size_t count = std::max<std::size_t>(1, frames->size());
            animationFrame = (animationFrame + 1) % static_cast<int>(count);
        }
        auto moveAgent = [&](const std::string& action, float& x) {
            if (action == "FETCH" || action == "WANDER" || action == "APPROACH_FOLLOW" ||
                action == "SNIFF" || action == "HIDE_AVOID" || action == "PLAY") {
                const float direction = (action == "WANDER" || action == "HIDE_AVOID") ? -1.0f : 1.0f;
                x = std::clamp(x + direction * elapsedSeconds * 70.0f, 320.0f, 950.0f);
            }
        };
        moveAgent(latestBaselineAction, baselineAgentX);
        moveAgent(latestProposedAction, proposedAgentX);
        if (actionDisplayTimer <= 0.0f) {
            animationActive = false;
            animationFrame = 0;
        }
    }
    if (actionDisplayTimer <= 0.0f && !interactionBuffer.empty()) {
        const auto next = interactionBuffer.front();
        interactionBuffer.pop_front();
        executeInteraction(next.label, next.observation, next.reward, next.preference, next.preferencePositive);
    }
}

void CompanionUI::drawCompanion(float x, float y, const std::string& visibleAction) {
    const auto* frames = &framesForAction(
        visibleAction, idleFrames, walkFrames, sleepFrames, barkFrames, sitFrames, layFrames);
    if (!frames->empty()) {
        const std::size_t index = animationActive ? static_cast<std::size_t>(animationFrame) % frames->size() : 0u;
        sf::Sprite dog(*(*frames)[index]);
        const auto size = (*frames)[index]->getSize();
        const float scale = 72.0f / static_cast<float>(std::max(1u, size.y));
        dog.setScale({scale, scale});
        dog.setPosition({x, y});
        window.draw(dog);
        return;
    }
    if (dogTextureLoaded) {
        const sf::Vector2u size = dogTexture.getSize();
        const unsigned int frameWidth = std::max(1u, size.x / 7u);
        const unsigned int frameHeight = std::max(1u, size.y / 4u);
        sf::Sprite dog(dogTexture);
        dog.setTextureRect(sf::IntRect({0, 0}, {static_cast<int>(frameWidth), static_cast<int>(frameHeight)}));
        const float scale = 64.0f / static_cast<float>(frameHeight);
        dog.setScale({scale, scale});
        dog.setPosition({x, y});
        window.draw(dog);
    } else {
        sf::CircleShape companion(22);
        companion.setPosition({x, y});
        companion.setFillColor(kBlue);
        window.draw(companion);
    }
}

void CompanionUI::processInteraction(const std::string& label,
                                     const EnvironmentObservation& observation,
                                     float feedbackRewardValue,
                                     const std::string& preference,
                                     bool preferencePositive) {
    interactionBuffer.push_back(QueuedInteraction{label, observation, feedbackRewardValue, preference, preferencePositive});
    appendLog("Queued interaction (" + std::to_string(interactionBuffer.size()) + " waiting): " + label);
}

void CompanionUI::executeInteraction(const std::string& label,
                                     const EnvironmentObservation& observation,
                                     float feedbackRewardValue,
                                     const std::string& preference,
                                     bool preferencePositive) {
    if (!preference.empty()) proposed.recordUserPreference(preference, preferencePositive);
    const auto affectBefore = proposed.getAffectiveState();
    const std::size_t memoryCountBefore = proposed.getMemoryRecords().size();
    const int interactionId = interactionCount + 1;
    lastActionInteractionId = interactionId;
    const std::string previousStimulus = latestStimulus;
    latestObservation = observation;
    latestStimulus = label;

    auto isFetchRequest = [](const std::string& value) {
        std::string lower = value;
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return lower.find("fetch") != std::string::npos || lower.find("ball") != std::string::npos;
    };
    const bool continuingFetch = isFetchRequest(previousStimulus) && isFetchRequest(label);
    EnvironmentObservation baselineObservation = observation;
    EnvironmentObservation proposedObservation = observation;
    // A repeated fetch command continues a still-active task in each paired world.
    // A completed task or a different user command starts from the new observation.
    if (continuingFetch && baselineWorldObservation.goal_state == 1 &&
        baselineWorldObservation.task_status == 1)
        baselineObservation = baselineWorldObservation;
    if (continuingFetch && proposedWorldObservation.goal_state == 1 &&
        proposedWorldObservation.task_status == 1)
        proposedObservation = proposedWorldObservation;

    latestBaselineAction = baseline.selectAction(baselineObservation);
    latestProposedAction = proposed.selectAction(proposedObservation);
    const std::size_t baselineStateBeforeOutcome = baseline.getCurrentState().encode();
    const std::size_t proposedStateBeforeOutcome = proposed.getCurrentState().encode();

    const float baselineEnergyBefore = baselineEnergy;
    const float proposedEnergyBefore = proposedEnergy;
    std::mt19937 baselineRng(static_cast<unsigned int>(interactionId * 7919));
    std::mt19937 proposedRng(static_cast<unsigned int>(interactionId * 7919));
    const RuntimeTransition baselineTransition = stepInteractiveWorld(
        latestBaselineAction, baselineObservation, baselineEnergyBefore, baselineStress,
        baselineSatisfaction, feedbackRewardValue, baselineRng);
    const RuntimeTransition proposedTransition = stepInteractiveWorld(
        latestProposedAction, proposedObservation, proposedEnergyBefore, proposedStress,
        proposedSatisfaction, feedbackRewardValue, proposedRng);

    ActionOutcome baselineOutcome;
    baselineOutcome.action_id = companionActionId(latestBaselineAction);
    baselineOutcome.reward = baselineTransition.reward;
    baselineOutcome.terminated = false;
    baselineOutcome.truncated = false;
    baselineOutcome.outcome_tag = baselineTransition.completed ? "goal_completed" : "interaction_outcome";
    baselineOutcome.energy_after = baselineTransition.energy;

    ActionOutcome proposedOutcome;
    proposedOutcome.action_id = companionActionId(latestProposedAction);
    proposedOutcome.reward = proposedTransition.reward;
    proposedOutcome.terminated = false;
    proposedOutcome.truncated = false;
    proposedOutcome.outcome_tag = proposedTransition.completed ? "goal_completed" : "interaction_outcome";
    proposedOutcome.energy_after = proposedTransition.energy;

    baseline.applyOutcome(baselineObservation, latestBaselineAction, baselineOutcome, baselineTransition.next);
    proposed.applyOutcome(proposedObservation, latestProposedAction, proposedOutcome, proposedTransition.next);

    baselineWorldObservation = baselineTransition.next;
    proposedWorldObservation = proposedTransition.next;
    baselineEnergy = baselineTransition.energy;
    proposedEnergy = proposedTransition.energy;
    baselineStress = baselineTransition.stress;
    proposedStress = proposedTransition.stress;
    baselineSatisfaction = baselineTransition.satisfaction;
    proposedSatisfaction = proposedTransition.satisfaction;

    animationActive = true;
    animationClock = 0.0f;
    animationFrame = 0;
    actionDisplayTimer = 0.85f;
    baselineRewards.push_back(baselineOutcome.reward);
    proposedRewards.push_back(proposedOutcome.reward);
    ++interactionCount;
    appendLog("You: " + label);
    appendLog("Baseline: " + describeAction(latestBaselineAction) +
              " (reward " + std::to_string(baselineOutcome.reward) + ")");
    appendLog("Proposed: " + describeAction(latestProposedAction) +
              " (reward " + std::to_string(proposedOutcome.reward) + ")");

    nlohmann::json memoryCandidates = nlohmann::json::array();
    for (const auto& memory : proposed.getMemoryRecords()) {
        if (memory.raw_fact.subject == latestProposedAction ||
            memory.raw_fact.relation == "prefers_action" ||
            memory.raw_fact.relation == "dislikes_action" ||
            memory.raw_fact.relation == "likes" ||
            memory.raw_fact.relation == "dislikes" ||
            memory.raw_fact.relation == "response_to_action") {
            memoryCandidates.push_back({
                {"subject", memory.raw_fact.subject},
                {"relation", memory.raw_fact.relation},
                {"object", memory.raw_fact.object},
                {"confidence", memory.confidence},
                {"observation_count", memory.observation_count}
            });
        }
    }

    nlohmann::json row;
    row["schema_version"] = 2;
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
    row["baseline"] = {
        {"observation", {
            {"goal_state", baselineObservation.goal_state},
            {"target_proximity", baselineObservation.target_proximity},
            {"user_status", baselineObservation.user_status},
            {"task_status", baselineObservation.task_status},
            {"food_available", baselineObservation.food_available},
            {"play_available", baselineObservation.play_available}
        }},
        {"action", latestBaselineAction}, {"action_id", baselineOutcome.action_id},
        {"reward", baselineOutcome.reward}, {"state_before_outcome", baselineStateBeforeOutcome},
        {"state_after_outcome", baseline.getCurrentState().encode()},
        {"energy_before", baselineEnergyBefore}, {"energy_after", baselineTransition.energy}, {"stress_after", baselineTransition.stress},
        {"satisfaction_after", baselineTransition.satisfaction},
        {"terminated", baselineOutcome.terminated}, {"truncated", baselineOutcome.truncated},
        {"completed", baselineTransition.completed}, {"learning_update_enabled", true}
    };
    row["proposed"] = {
        {"observation", {
            {"goal_state", proposedObservation.goal_state},
            {"target_proximity", proposedObservation.target_proximity},
            {"user_status", proposedObservation.user_status},
            {"task_status", proposedObservation.task_status},
            {"food_available", proposedObservation.food_available},
            {"play_available", proposedObservation.play_available}
        }},
        {"action", latestProposedAction}, {"action_id", proposedOutcome.action_id},
        {"reward", proposedOutcome.reward}, {"state_before_outcome", proposedStateBeforeOutcome},
        {"state_after_outcome", proposed.getCurrentState().encode()},
        {"memory_candidates", memoryCandidates},
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
        }},
        {"energy_before", proposedEnergyBefore}, {"energy_after", proposedTransition.energy},
        {"stress_after", proposedTransition.stress},
        {"satisfaction_after", proposedTransition.satisfaction},
        {"terminated", proposedOutcome.terminated}, {"truncated", proposedOutcome.truncated},
        {"completed", proposedTransition.completed}, {"learning_update_enabled", true}
    };
    std::ofstream log(std::filesystem::path(outputDirectory) /
                      ("interaction-events-" + runId + ".jsonl"), std::ios::app);
    if (log) log << row.dump() << '\n';
    else appendLog("WARNING: interaction event log could not be written.");
    saveSession();
}

std::string CompanionUI::interpretIntent(const std::string& text, EnvironmentObservation& observation, float& reward) {
    observation = EnvironmentObservation{0, 1, 0, 0, 0.45f};
    reward = 0.0f;
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
            observation = EnvironmentObservation{
                parsed.value("goal_state", 0), parsed.value("target_proximity", 1),
                negativeFeedback ? 2 : parsed.value("user_status", 0),
                parsed.value("task_status", 0), parsed.value("signal_strength", 0.45f),
                parsed.value("food_available", false), parsed.value("play_available", false)};
            observation.food_available = observation.food_available ||
                lowered.find("food") != std::string::npos || lowered.find("feed") != std::string::npos;
            observation.play_available = observation.play_available ||
                lowered.find("play") != std::string::npos || lowered.find("toy") != std::string::npos;
            // Commands do not carry a preset reward. Only explicit praise or
            // rejection is treated as direct user feedback; action quality and
            // world consequences are computed by the interaction transition.
            reward = negativeFeedback ? -1.0f : 0.0f;
            if (!negativeFeedback &&
                (intent == "praise" || lowered.find("praise") != std::string::npos ||
                 lowered.find("good") != std::string::npos ||
                 lowered.find("well done") != std::string::npos ||
                 lowered.find("thank") != std::string::npos))
                reward = 1.0f;
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
        observation = EnvironmentObservation{1, 1, 1, 0, 0.85f}; reward = 0.65f; return "request";
    }
    if (lowered.find("noise") != std::string::npos || lowered.find("scared") != std::string::npos ||
        lowered.find("loud") != std::string::npos) {
        observation = EnvironmentObservation{2, 0, 0, 0, 0.95f}; reward = 0.0f; return "disturbance";
    }
    if (lowered.find("feed") != std::string::npos || lowered.find("food") != std::string::npos ||
        lowered.find("eat") != std::string::npos) {
        observation = EnvironmentObservation{2, 0, 1, 0, 0.30f, true, false};
        reward = 0.0f; return "feeding";
    }
    if (lowered.find("play") != std::string::npos || lowered.find("toy") != std::string::npos) {
        observation = EnvironmentObservation{0, 1, 1, 0, 0.70f, false, true};
        reward = 0.0f; return "play";
    }
    if (lowered.find("rest") != std::string::npos || lowered.find("tired") != std::string::npos ||
        lowered.find("quiet") != std::string::npos) {
        observation = EnvironmentObservation{2, 2, 0, 0, 0.20f}; reward = 0.0f; return "rest";
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

    processInteraction(text + " [" + intent + " / " + nlpMethod + "]", observation, reward);
}

void CompanionUI::handleClick(sf::Vector2f point) {
    const float x = point.x, y = point.y;
    if (screen == Screen::AgentSelection) {
        if (y >= 530 && y <= 600 && x >= 180 && x <= 580) {
            proposed.clearMemory();
            std::error_code ec;
            const auto memoryPath = std::filesystem::path(policyDirectory) / "proposed.qtable.memory.json";
            std::filesystem::remove(memoryPath, ec);
            appendLog(ec ? "Memory file deletion failed: " + ec.message() : "Persistent memory cleared.");
            return;
        }
        if (y >= 300 && y <= 380 && ((x >= 180 && x <= 580) || (x >= 700 && x <= 1100))) {
            selectedAgentCode = x < 640.0f ? 'A' : 'B';
            selectedAgentIsProposed = selectedAgentCode == 'A' ? agentAIsProposed : !agentAIsProposed;
            {
                nlohmann::json manifest;
                manifest["schema_version"] = 1;
                manifest["mode"] = "interactive_simulation";
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
            screen = Screen::Simulation;
            appendLog(std::string("Loaded blind condition ") + selectedAgentCode);
        }
        return;
    }
    if (y >= 14 && y <= 58) {
        if (x < 180) screen = Screen::Simulation;
        else if (x < 330) screen = Screen::AgentSelection;
        else if (x < 490) screen = Screen::AgentState;
        else if (x < 650) screen = Screen::Comparison;
        else if (x > 1030) controlsVisible = !controlsVisible;
        return;
    }
    if (screen == Screen::AgentState && x >= 930 && y >= 750 && y <= 810) {
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
    textInputFocused = y >= 755 && y <= 805;
    if (y >= 550 && y <= 594 && x >= 35.0f && x < 1235.0f) {
        const int buttonIndex = static_cast<int>((x - 35.0f) / 134.0f);
        switch (buttonIndex) {
            case 0: processInteraction("Greet the companion", {0, 1, 0, 0, 0.55f}, 0.0f); break;
            case 1: processInteraction("Ask the companion to fetch the ball", {1, 1, 1, 1, 0.85f}, 0.0f); break;
            case 2: processInteraction("Feed the companion", {2, 0, 1, 0, 0.30f, true, false}, 0.0f); break;
            case 3: processInteraction("Invite the companion to play", {0, 1, 1, 0, 0.70f, false, true}, 0.0f); break;
            case 4: processInteraction("Praise the companion", {0, 0, 1, 0, 0.75f}, 1.0f); break;
            case 5: processInteraction("Scold the companion", {0, 1, 2, 0, 0.45f}, -1.0f); break;
            case 6: processInteraction("Explore the yard", {2, 2, 0, 0, 0.40f}, 0.0f); break;
            case 7: processInteraction("The companion should rest", {2, 2, 0, 0, 0.20f}, 0.0f); break;
            case 8:
                if (interactionCount > 0)
                    processInteraction("Repeat: " + latestStimulus, latestObservation, 0.0f);
                else
                    appendLog("Interact once before repeating a scenario.");
                break;
            default: break;
        }
        return;
    }
    if (y >= 755 && y <= 805 && x >= 1020 && !textInput.empty()) {
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
    drawText("Select A or B. The interface does not disclose the model behind either label.", 60, 170, 17, kMuted);
    drawText("The A/B assignment is randomised for this application session.", 60, 205, 15, kMuted);
    drawButton("Load Agent A", 180, 300, 400, 80, selectedAgentCode == 'A');
    drawButton("Load Agent B", 700, 300, 400, 80, selectedAgentCode == 'B');
    drawText("Memory persists between sessions for the proposed model.", 180, 430, 16, kMuted);
    drawText("The research condition remains hidden from the participant-facing screen.", 180, 460, 16, kMuted);
    drawButton("Clear Persistent Memory", 180, 540, 400, 54);
    drawText("This deletes the proposed agent's saved memory file; it does not delete its policy.", 180, 610, 14, kMuted);
}

void CompanionUI::drawNavigation() {
    drawButton("Simulation", 28, 12, 145, 42, screen == Screen::Simulation);
    drawButton("Agent State", 185, 12, 145, 42, screen == Screen::AgentState);
    drawButton("Comparison", 342, 12, 145, 42, screen == Screen::Comparison);
    if (screen == Screen::Simulation)
        drawButton(controlsVisible ? "Hide Controls" : "Show Controls", 1030, 12, 180, 42);
    drawText("ANIMAL AGENT / SFML", 760, 22, 15, kMuted);
}

void CompanionUI::drawBackdrop(float x, float y, float width, float height) {
    // free.png is a transparent object atlas (plants, house, tree, flowers and
    // garden produce), not a seamless grass tile sheet. Build a yard base and
    // place its actual transparent sprites on top instead of stretching the atlas.
    sf::RectangleShape lawn({width, height});
    lawn.setPosition({x, y});
    lawn.setFillColor(sf::Color(122, 145, 78));
    window.draw(lawn);

    auto sprite = [&](sf::IntRect source, sf::Vector2f position, float scale) {
        if (!environmentTextureLoaded) return;
        sf::Sprite item(environmentTexture);
        item.setTextureRect(source);
        item.setScale({scale, scale});
        item.setPosition(position);
        window.draw(item);
    };

    // A winding dirt track provides a clear interaction area in the yard.
    sf::ConvexShape path;
    path.setPointCount(6);
    path.setPoint(0, {x + width * 0.28f, y + height});
    path.setPoint(1, {x + width * 0.43f, y + height});
    path.setPoint(2, {x + width * 0.60f, y + height * 0.52f});
    path.setPoint(3, {x + width * 0.56f, y + height * 0.43f});
    path.setPoint(4, {x + width * 0.48f, y + height * 0.52f});
    path.setPoint(5, {x + width * 0.36f, y + height * 0.77f});
    path.setFillColor(sf::Color(160, 135, 83));
    window.draw(path);

    // The house and tree are cropped from the supplied atlas using source
    // pixel rectangles, preserving transparency and aspect ratio.
    sprite(sf::IntRect({0, 48}, {64, 64}), {x + width - 205.0f, y + 18.0f}, 2.35f);
    sprite(sf::IntRect({144, 48}, {32, 32}), {x + width - 270.0f, y + 16.0f}, 2.2f);

    // Decorative shrub patches around the perimeter (atlas source crops).
    sprite(sf::IntRect({0, 0}, {32, 32}), {x + 24.0f, y + 12.0f}, 2.0f);
    sprite(sf::IntRect({48, 0}, {32, 24}), {x + 205.0f, y + 12.0f}, 1.8f);
    sprite(sf::IntRect({96, 0}, {32, 24}), {x + width * 0.43f, y + 16.0f}, 1.8f);
    sprite(sf::IntRect({144, 0}, {32, 24}), {x + width * 0.62f, y + 18.0f}, 1.8f);

    // Small flowers and garden plants make the scene read as a backyard.
    for (int i = 0; i < 9; ++i) {
        const int flowerX = (i % 4) * 16;
        sprite(sf::IntRect({flowerX, 112}, {16, 16}),
               {x + 18.0f + static_cast<float>(i) * 25.0f, y + height - 38.0f}, 1.15f);
    }
    for (int i = 0; i < 5; ++i) {
        sprite(sf::IntRect({112 + (i % 3) * 16, 128}, {16, 16}),
               {x + width - 220.0f + static_cast<float>(i) * 27.0f, y + height - 72.0f}, 1.25f);
    }
}

void CompanionUI::drawSimulationScreen() {
    drawText("MAIN SIMULATION", 30, 75, 26, kInk);
    drawText("Both policies receive the same interpreted interaction.", 30, 106, 15, kMuted);
    const float sceneHeight = controlsVisible ? 390.0f : 710.0f;
    sf::RectangleShape scene({1218, sceneHeight});
    scene.setPosition({30, 135});
    scene.setFillColor(sf::Color(122, 145, 78));
    scene.setOutlineColor(sf::Color(83, 104, 55));
    scene.setOutlineThickness(1);
    window.draw(scene);
    drawBackdrop(30, 135, 1218, sceneHeight);

    sf::CircleShape person(18);
    person.setPosition({userX, userY + 30});
    person.setFillColor(sf::Color(216, 122, 110));
    window.draw(person);
    drawText("YOU", userX - 3, userY + 67, 12, kInk);
    drawCompanion(baselineAgentX, agentY, latestBaselineAction);
    drawText("BASELINE", baselineAgentX - 12, agentY + 64, 12, kBlue);
    drawCompanion(proposedAgentX, agentY, latestProposedAction);
    drawText("PROPOSED", proposedAgentX - 12, agentY + 64, 12, kGreen);
    drawText("Baseline: " + latestBaselineAction + " | reward " + std::to_string(baseline.getSessionContext().last_reward), 50, 150, 15, kBlue);
    drawText("Proposed: " + latestProposedAction + " | reward " + std::to_string(proposed.getSessionContext().last_reward), 50, 176, 15, kGreen);
    drawText("Input interpretation: " + nlpMethod, 50, 202, 14, kMuted);
    if (!controlsVisible) {
        drawText("Controls hidden - use Show Controls to interact", 50, 235, 16, kInk);
        return;
    }
    const std::array<std::string, 9> labels{{"Greet", "Fetch", "Feed", "Play", "Praise", "Scold", "Explore", "Rest", "Repeat last"}};
    for (std::size_t i = 0; i < labels.size(); ++i)
        drawButton(labels[i], 35.0f + static_cast<float>(i) * 134.0f, 550, 128, 44);
    drawText("Recent activity", 35, 610, 18, kInk);
    float logY = 637;
    for (auto it = eventLog.rbegin(); it != eventLog.rend() && logY < 720; ++it, logY += 20)
        drawText(*it, 45, logY, 13, kMuted);
    drawButton(textInput.empty() ? "Type a message here, then press Enter" : textInput, 35, 755, 980, 48, textInputFocused);
    drawButton("Send", 1030, 755, 120, 48);
    drawText("Enter to send | Esc to quit", 850, 815, 13, kMuted);
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
    drawBar("Energy", proposedEnergy, 50, 320, 235);
    drawText("Valence", 340, 350, 15, kInk);
    drawText(affect.valence == AffectValence::POSITIVE ? "Positive" :
             affect.valence == AffectValence::NEGATIVE ? "Negative" : "Neutral", 440, 350, 15, kBlue);
    drawText("Current action: " + latestProposedAction, 50, 385, 15, kInk);

    drawText("Current interaction context", 660, 158, 19, kBlue);
    drawText("Stimulus: " + latestStimulus, 660, 200, 15);
    drawText("Intent code: " + std::to_string(latestObservation.goal_state), 660, 230, 15);
    drawText("Target proximity: " + std::to_string(latestObservation.target_proximity), 660, 260, 15);
    drawText("Task active: " + std::string(latestObservation.task_status ? "Yes" : "No"), 660, 290, 15);
    drawText("Signal strength: " + std::to_string(latestObservation.signal_strength), 660, 320, 15);
    drawText("Interactions: " + std::to_string(interactionCount), 660, 350, 15);
    drawText("Current user context: local simulation session", 660, 380, 15);

    drawText("Relevant stored memories (proposed agent)", 50, 445, 19, kGreen);
    const auto memories = rankRelevantRecords(proposed.getMemoryRecords(), latestStimulus);
    if (memories.empty()) drawText("No relevant interaction memories have been retrieved yet.", 50, 490, 16, kMuted);
    float y = 490;
    for (auto it = memories.begin(); it != memories.end() && y < 690; ++it, y += 38) {
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
        baseline.savePersistentState((std::filesystem::path(policyDirectory) / "baseline.qtable").string());
        proposed.savePersistentState((std::filesystem::path(policyDirectory) / "proposed.qtable").string());
    } catch (const std::exception& e) {
        std::cerr << "Unable to save simulation session: " << e.what() << '\n';
    }
}
