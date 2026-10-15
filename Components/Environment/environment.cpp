#include "environment.h"

#include <chrono>
#include <ctime>
#include <random>
#include <thread>

namespace {
Level levelFromValue(int value) {
    const int bounded = std::clamp(value, 0, 2);
    switch (bounded) {
        case 0:
            return Level::LOW;
        case 1:
            return Level::MEDIUM;
        case 2:
        default:
            return Level::HIGH;
    }
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

void setStateFromStimulus(state& target,
                         const std::string& proximityLabel,
                         const SessionContext& context,
                         const AffectiveState& affect,
                         float memorySignal,
                         int userStatus) {
    distance proximity = distance::FAR;
    if (proximityLabel == "nearby") {
        proximity = distance::CLOSE;
    } else if (proximityLabel == "medium") {
        proximity = distance::MEDIUM;
    } else if (proximityLabel == "far") {
        proximity = distance::FAR;
    }

    target = makeStateWithContext(
        context.current_intent,
        proximity,
        levelFromValue(context.energy_level),
        levelFromValue(context.stress_level),
        affect.engagement > 0.7f ? Level::HIGH : Level::MEDIUM,
        affect.affinity > 0.7f ? Level::HIGH : Level::LOW,
        userStatus,
        memorySignal,
        std::clamp(memorySignal, 0.0f, 1.0f),
        std::clamp(memorySignal + 0.2f, 0.0f, 1.0f),
        std::clamp(1.0f - memorySignal, 0.0f, 1.0f),
        affect);
}
} // namespace

void tab::updateParameters(std::map<std::string, std::string> parameters_to_update) {
    parameters_to_display.insert(parameters_to_update.begin(), parameters_to_update.end());
}

void window::updateWindow() {
    for (auto& tabEntry : window_tabs) {
        (void)tabEntry;
    }
}

void simulation_environment::initiate() {
    active_window.clear();
    current_user.clear();
}

void simulation_environment::update() {
    clockT += dt;
}

void simulation_environment::fetchRepresentation() {
    // placeholder until richer simulation and rendering are introduced.
}

void simulation_environment::addEntity(std::string entityType) {
    (void)entityType;
}

void simulation_environment::renderObjects() {
    // placeholder for future rendering or visualization work.
}

BackyardSimulation::BackyardSimulation()
    : qPolicy(0.25f, 0.8f, 0.15f) {
    initialiseScriptedEvents();
    sessionContext.current_action = "IDLE";
    sessionContext.last_timestamp = "2026-10-13T08:00:00Z";
    runtimeContext.session_context = sessionContext;
    runtimeContext.affective_state = affectiveState;
    runtimeContext.latest_event = {"2026-10-13T08:00:00Z", "IDLE", "No active stimulus", 0.0f, "idle", 0.0f};
    qPolicy.setActions({"FETCH", "WANDER", "BARK", "ACT_CONFUSED", "SLEEP_LAY", "NUZZLE", "INSIST", "GREETING"});

    if (!hudFont.openFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")) {
        std::cerr << "Warning: default font unavailable; HUD text will use the SFML default fallback." << std::endl;
    }
    hudText = std::make_unique<sf::Text>(hudFont, "");
    hudText->setCharacterSize(18);
    hudText->setFillColor(sf::Color::Black);
    hudText->setPosition({20.0f, 20.0f});
    currentModelState = makeState(0, distance::FAR, Level::MEDIUM, Level::LOW, Level::MEDIUM, Level::LOW, 0, 0.0f);
    updateHudText();

    window.create(sf::VideoMode(sf::Vector2u(960u, 600u)), "IS-2 Backyard Simulation");
    window.setFramerateLimit(60);

    ground.setSize({960.0f, 600.0f});
    ground.setFillColor(sf::Color(90, 170, 90));

    path.setSize({240.0f, 120.0f});
    path.setPosition({350.0f, 360.0f});
    path.setFillColor(sf::Color(200, 180, 120));

    house.setSize({180.0f, 140.0f});
    house.setPosition({660.0f, 180.0f});
    house.setFillColor(sf::Color(220, 210, 210));

    tree.setRadius(40.0f);
    tree.setPosition({120.0f, 120.0f});
    tree.setFillColor(sf::Color(40, 120, 40));

    agent.setRadius(18.0f);
    agent.setFillColor(sf::Color(80, 140, 220));
    agent.setPosition({agentX, agentY});

    user.setRadius(24.0f);
    user.setFillColor(sf::Color(230, 110, 110));
    user.setPosition({userX, userY});
}

void BackyardSimulation::initialiseScriptedEvents() {
    scriptedEvents = {
        {"2026-10-13T08:00:00Z", "backyard gate greeting", "User waves from the gate and says hello.", "nearby", 0.42f, 0.35f, "GREETING"},
        {"2026-10-13T08:00:30Z", "garden request", "User asks for the ball to be fetched from the yard.", "medium", 0.73f, 0.85f, "FETCH"},
        {"2026-10-13T08:01:00Z", "unexpected loud sound", "A sudden noise startles the agent in the yard.", "nearby", 0.66f, -0.2f, "ACT_CONFUSED"},
        {"2026-10-13T08:01:45Z", "quiet rest cue", "The user settles down and the environment becomes calmer.", "far", 0.22f, 0.15f, "SLEEP_LAY"},
        {"2026-10-13T08:02:30Z", "follow-up greeting", "User resumes contact and asks if the agent is paying attention.", "nearby", 0.58f, 0.50f, "GREETING"}
    };
}

std::string BackyardSimulation::chooseBehaviorForStimulus(const std::string& trigger) const {
    const std::string lowered = toLower(trigger);
    if (lowered.find("greeting") != std::string::npos || lowered.find("hello") != std::string::npos || lowered.find("attention") != std::string::npos) {
        return "GREETING";
    }
    if (lowered.find("request") != std::string::npos || lowered.find("fetch") != std::string::npos || lowered.find("ball") != std::string::npos) {
        return "FETCH";
    }
    if (lowered.find("noise") != std::string::npos || lowered.find("startle") != std::string::npos || lowered.find("unexpected") != std::string::npos) {
        return "ACT_CONFUSED";
    }
    if (lowered.find("rest") != std::string::npos || lowered.find("settles") != std::string::npos || lowered.find("calm") != std::string::npos) {
        return "SLEEP_LAY";
    }
    if (lowered.find("look") != std::string::npos || lowered.find("question") != std::string::npos) {
        return "LOOK_UP";
    }
    return "OBSERVE";
}

std::string BackyardSimulation::inferResultSummary(float reward, const std::string& behavior) const {
    if (behavior == "GREETING") {
        return reward > 0.0f ? "friendly social contact is acknowledged" : "social tone was uncertain";
    }
    if (behavior == "FETCH") {
        return reward > 0.0f ? "request was accepted and user engagement improved" : "request produced a weak or mixed response";
    }
    if (behavior == "ACT_CONFUSED") {
        return reward > 0.0f ? "the agent reoriented after the startle" : "the sudden stimulus created uncertainty";
    }
    if (behavior == "SLEEP_LAY") {
        return reward > 0.0f ? "the environment settled and the agent relaxed" : "the agent remained cautious during rest";
    }
    return reward > 0.0f ? "the stimulus was processed without conflict" : "the stimulus was interpreted as subtly negative";
}

std::string BackyardSimulation::inferUserResponse(const std::string& action, float reward, const std::string& stimulus) const {
    if (action == "GREETING") {
        return reward > 0.0f ? "user responds warmly and continues the interaction" : "user responds cautiously then pauses";
    }
    if (action == "FETCH") {
        return reward > 0.0f ? "user approves and follows up with a task request" : "user hesitates and rephrases the request";
    }
    if (action == "ACT_CONFUSED") {
        return reward > 0.0f ? "user reassures the agent and clarifies the situation" : "user becomes impatient and keeps distance";
    }
    if (action == "SLEEP_LAY") {
        return reward > 0.0f ? "user relaxes and leaves the agent in a resting state" : "user remains alert and checks on the agent";
    }
    if (action == "NUZZLE") {
        return reward > 0.0f ? "user accepts the affectionate behavior and stays engaged" : "user withdraws slightly and does not reciprocate";
    }
    if (action == "INSIST") {
        return reward > 0.0f ? "user moves to comply after a brief delay" : "user resists and the interaction becomes tense";
    }
    if (action == "BARK") {
        return reward > 0.0f ? "user is startled but then responds with attention" : "user is unsettled and moves farther away";
    }
    if (stimulus.find("request") != std::string::npos || stimulus.find("ask") != std::string::npos) {
        return reward > 0.0f ? "user is satisfied with the response" : "user remains unsatisfied and asks again";
    }
    return reward > 0.0f ? "user remains engaged with the current tone" : "user becomes less engaged and the interaction softens";
}

float BackyardSimulation::inferRewardForAction(const std::string& action, const std::string& stimulus) const {
    const std::string lowered = toLower(stimulus);
    float baseReward = 0.1f;

    if (action == "FETCH") {
        baseReward += lowered.find("request") != std::string::npos ? 0.6f : 0.3f;
    } else if (action == "GREETING") {
        baseReward += lowered.find("greeting") != std::string::npos || lowered.find("hello") != std::string::npos ? 0.5f : 0.2f;
    } else if (action == "ACT_CONFUSED") {
        baseReward += lowered.find("loud") != std::string::npos || lowered.find("startle") != std::string::npos ? 0.1f : -0.2f;
    } else if (action == "SLEEP_LAY") {
        baseReward += lowered.find("rest") != std::string::npos || lowered.find("calm") != std::string::npos ? 0.45f : 0.1f;
    } else if (action == "NUZZLE") {
        baseReward += 0.3f;
    } else if (action == "INSIST") {
        baseReward += lowered.find("request") != std::string::npos ? 0.2f : -0.15f;
    } else if (action == "BARK") {
        baseReward += lowered.find("loud") != std::string::npos ? 0.1f : -0.25f;
    }

    if (currentSignalStrength > 0.7f && action == "FETCH") {
        baseReward += 0.2f;
    }
    if (sessionContext.stress_level > 0 && action == "SLEEP_LAY") {
        baseReward += 0.15f;
    }

    return std::clamp(baseReward, -1.0f, 1.0f);
}

void BackyardSimulation::processScenarioEvent(const ScriptedScenarioEvent& event) {
    const float memorySignal = summarizeWeightedMemorySignal(memoryRecords);
    state currentState;
    setStateFromStimulus(currentState, event.proximity_label, sessionContext, affectiveState, memorySignal, sessionContext.user_status);
    currentState.encoded_user_input = static_cast<float>(event.signal_strength > 0.7f ? 2 : 1);
    currentModelState = currentState;

    const std::string behavior = qPolicy.chooseAction(currentState);
    const float reward = inferRewardForAction(behavior, event.trigger);
    const std::string userResponse = inferUserResponse(behavior, reward, event.trigger);

    currentBehaviorText = behavior;
    currentObservationText = event.user_signal + " | proximity=" + event.proximity_label + " | behavior=" + behavior + " | user_response=" + userResponse;
    currentSignalStrength = event.signal_strength;

    sessionContext.current_intent = event.signal_strength > 0.7f ? 2 : (event.signal_strength > 0.4f ? 1 : 0);
    sessionContext.target_proximity = event.proximity_label == "nearby" ? 0 : (event.proximity_label == "medium" ? 1 : 2);
    sessionContext.user_status = event.signal_strength > 0.5f ? 1 : 0;
    sessionContext.energy_level = behavior == "SLEEP_LAY" ? 0 : (behavior == "ACT_CONFUSED" ? 1 : 2);
    sessionContext.stress_level = (behavior == "ACT_CONFUSED" || reward < 0.0f) ? 1 : 0;
    sessionContext.current_action = behavior;
    sessionContext.last_timestamp = event.timestamp;
    sessionContext.last_reward = reward;

    state nextState = currentState;
    nextState.environmentCondition["target_proximity"] = distance::CLOSE;
    nextState.memory = reward;
    nextState.memory_bias = summarizeMemoryBias(std::clamp(memorySignal + reward, 0.0f, 1.0f), 0.5f, 0.5f);
    nextState.affect_trend = summarizeAffectTrend(affectiveState, levelFromValue(sessionContext.energy_level));
    qPolicy.update(currentState, behavior, reward, nextState);

    affectiveState.updateFromFeedback(reward, reward > 0.0f, reward < -0.3f);

    record memoryEntry;
    memoryEntry.raw_fact.subject = "user";
    memoryEntry.raw_fact.relation = "stimulus";
    memoryEntry.raw_fact.object = event.trigger;
    memoryEntry.confidence = std::clamp(event.signal_strength, 0.0f, 1.0f);
    memoryEntry.generalization_factor = 0.6f;
    memoryEntry.current_context = record::USER_SPECIFIC;
    memoryEntry.source = record::USER_STATEMENT;
    memoryEntry.observation_count = 1;
    memoryEntry.last_updated = event.timestamp;
    memoryEntry.relevance = std::clamp(std::abs(reward) + 0.3f, 0.0f, 1.0f);
    memoryEntry.recency = std::clamp(event.signal_strength + 0.2f, 0.0f, 1.0f);
    memoryRecords.push_back(memoryEntry);

    const std::string inferredResult = inferResultSummary(reward, behavior);
    InteractionEvent interaction{event.timestamp, behavior, userResponse + " => " + inferredResult, reward, event.trigger + " @ " + event.proximity_label, event.signal_strength};
    interactionHistory.push_back(interaction);

    runtimeContext.latest_event = interaction;
    runtimeContext.session_context = sessionContext;
    runtimeContext.affective_state = affectiveState;
    runtimeContext.memory_summary = buildMemorySummary(memoryRecords, event.trigger);
    runtimeContext.recent_actions.push_back(behavior);
    if (runtimeContext.recent_actions.size() > 6) {
        runtimeContext.recent_actions.erase(
            runtimeContext.recent_actions.begin(),
            runtimeContext.recent_actions.begin() + static_cast<std::ptrdiff_t>(runtimeContext.recent_actions.size() - 6));
    }

    std::cout << "[sim][" << event.timestamp << "] trigger=" << event.trigger
              << " policy_behaviour=" << behavior
              << " reward=" << reward
              << " user_response=" << userResponse
              << std::endl;

    updateHudText();
}

void BackyardSimulation::updateStimulusTimeline(float dt) {
    elapsedSimulationTime += dt;
    timeUntilNextEvent -= dt;
    if (timeUntilNextEvent <= 0.0f) {
        triggerLearningDrivenStimulus();
        static std::mt19937 rng(static_cast<unsigned int>(std::time(nullptr)));
        std::uniform_real_distribution<float> dist(4.0f, 9.0f);
        timeUntilNextEvent = dist(rng);
    }
}

void BackyardSimulation::triggerLearningDrivenStimulus() {
    static std::mt19937 rng(static_cast<unsigned int>(std::time(nullptr)) + static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<int> typeDist(0, 4);
    std::uniform_real_distribution<float> signalDist(0.2f, 1.0f);
    std::uniform_int_distribution<int> proximityDist(0, 2);

    const std::vector<std::string> stimulusTypes = {
        "garden request",
        "backyard greeting",
        "unexpected loud sound",
        "quiet rest cue",
        "follow-up question"
    };
    const std::vector<std::string> proximityLabels = {"nearby", "medium", "far"};

    ScriptedScenarioEvent event;
    event.trigger = stimulusTypes[typeDist(rng)];
    event.user_signal = "The user emits a " + event.trigger + " cue";
    event.proximity_label = proximityLabels[proximityDist(rng)];
    event.signal_strength = signalDist(rng);
    event.reward = 0.0f;
    event.timestamp = "";
    event.selected_behavior = "";

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm gmt{};
    gmtime_r(&time, &gmt);
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%SZ", &gmt);
    event.timestamp = std::string(stamp);

    processScenarioEvent(event);
    currentEventIndex += 1;
}

void BackyardSimulation::persistSessionState() {
    if (sessionSaved) {
        return;
    }

    const std::filesystem::path projectRoot = std::filesystem::current_path();
    const std::filesystem::path docsDir = projectRoot / "Docs";
    std::filesystem::create_directories(docsDir);

    const std::filesystem::path memoryPath = docsDir / "memory_records.json";
    const std::filesystem::path affectPath = docsDir / "affect_state.json";
    const std::filesystem::path sessionPath = docsDir / "session_context.json";
    const std::filesystem::path historyPath = docsDir / "interaction_history.json";
    const std::filesystem::path aggregatePath = docsDir / "aggregated_facts.json";

    saveMemoryRecords(memoryPath.string(), memoryRecords);
    saveAffectiveState(affectPath.string(), affectiveState);
    saveSessionContext(sessionPath.string(), sessionContext);
    saveInteractionHistory(historyPath.string(), interactionHistory);
    saveAggregateFacts(aggregatePath.string(), aggregateRestStateFacts(interactionHistory, memoryRecords));

    sessionSaved = true;
    std::cout << "[sim] persisted runtime state to Docs/" << std::endl;
}

void BackyardSimulation::runScenarioHistory() {
    static std::mt19937 rng(static_cast<unsigned int>(std::time(nullptr)));
    std::uniform_int_distribution<int> countDist(5, 9);
    std::uniform_real_distribution<float> intervalDist(4.0f, 9.0f);
    std::uniform_int_distribution<int> typeDist(0, 4);
    std::uniform_int_distribution<int> proximityDist(0, 2);
    std::uniform_real_distribution<float> signalDist(0.2f, 1.0f);

    const int eventCount = countDist(rng);
    for (int index = 0; index < eventCount; ++index) {
        const std::vector<std::string> stimulusTypes = {
            "garden request",
            "backyard greeting",
            "unexpected loud sound",
            "quiet rest cue",
            "follow-up question"
        };
        const std::vector<std::string> proximityLabels = {"nearby", "medium", "far"};

        ScriptedScenarioEvent event;
        event.trigger = stimulusTypes[typeDist(rng)];
        event.user_signal = "The user emits a " + event.trigger + " cue";
        event.proximity_label = proximityLabels[proximityDist(rng)];
        event.signal_strength = signalDist(rng);
        event.timestamp = "";

        const auto now = std::chrono::system_clock::now() + std::chrono::seconds(index * 5);
        const auto time = std::chrono::system_clock::to_time_t(now);
        std::tm gmt{};
        gmtime_r(&time, &gmt);
        char stamp[32];
        std::strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%SZ", &gmt);
        event.timestamp = stamp;

        processScenarioEvent(event);
        if (index + 1 < eventCount) {
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(intervalDist(rng) * 100.0f)));
        }
    }

    persistSessionState();
}

std::string BackyardSimulation::getCurrentBehavior() const {
    return currentBehaviorText;
}

std::string BackyardSimulation::getCurrentObservation() const {
    return currentObservationText;
}

std::vector<InteractionEvent> BackyardSimulation::getInteractionHistory() const {
    return interactionHistory;
}

SessionContext BackyardSimulation::getSessionContext() const {
    return sessionContext;
}

RuntimeContext BackyardSimulation::getRuntimeContext() const {
    return runtimeContext;
}

void BackyardSimulation::handleInput(float dt) {
    const float moveSpeed = 180.0f * dt;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) {
        agentY -= moveSpeed;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) {
        agentY += moveSpeed;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) {
        agentX -= moveSpeed;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) {
        agentX += moveSpeed;
    }

    agentX = std::clamp(agentX, 20.0f, 900.0f);
    agentY = std::clamp(agentY, 20.0f, 560.0f);
    agent.setPosition({agentX, agentY});
}

void BackyardSimulation::updateSimulation(float dt) {
    updateStimulusTimeline(dt);
    stepCount += 1;
}

void BackyardSimulation::updateHudText() {
    std::ostringstream hud;
    const auto proximity = currentModelState.environmentCondition.find("target_proximity");
    const std::string proximityLabel = (proximity != currentModelState.environmentCondition.end())
        ? (proximity->second == distance::CLOSE ? "close" : (proximity->second == distance::MEDIUM ? "medium" : (proximity->second == distance::FAR ? "far" : "n/a")))
        : "n/a";

    hud << "Model state\n";
    hud << "action=" << currentBehaviorText << "\n";
    hud << "intent=" << static_cast<int>(currentModelState.encoded_user_input) << " | proximity=" << proximityLabel << "\n";
    hud << "energy=" << static_cast<int>(currentModelState.energy) << " | stress=" << static_cast<int>(currentModelState.stress) << "\n";
    hud << "curiosity=" << static_cast<int>(currentModelState.curiosity) << " | attachment=" << static_cast<int>(currentModelState.attachment) << "\n";
    hud << "memory=" << currentModelState.memory << " | memory_bias=" << currentModelState.memory_bias << "\n";
    hud << "affect_trend=" << currentModelState.affect_trend << " | reward=" << sessionContext.last_reward << "\n";
    hud << "signal=" << currentSignalStrength << " | observation=" << currentObservationText;

    hudText->setString(hud.str());
}

void BackyardSimulation::drawScene() {
    window.clear(sf::Color(110, 190, 230));
    window.draw(ground);
    window.draw(path);
    window.draw(house);
    window.draw(tree);
    window.draw(agent);
    window.draw(user);
    if (hudText) {
        window.draw(*hudText);
    }
    window.display();
}

void BackyardSimulation::run() {
    while (window.isOpen()) {
        const float dt = clock.restart().asSeconds();

        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
        }

        handleInput(dt);
        updateSimulation(dt);
        drawScene();

        if (currentEventIndex >= scriptedEvents.size() && !sessionSaved) {
            persistSessionState();
        }
    }

    persistSessionState();
}
