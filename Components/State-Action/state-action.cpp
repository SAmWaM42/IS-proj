#include "state-action.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
const std::vector<std::string>& kActions = companionActionSet();

bool nearlyEqual(double lhs, double rhs, double tolerance = 1e-6) {
    return std::abs(lhs - rhs) <= tolerance;
}

float normalizeEnergyLevel(Level energy) {
    switch (energy) {
        case Level::LOW:
            return 0.0f;
        case Level::MEDIUM:
            return 0.5f;
        case Level::HIGH:
            return 1.0f;
    }
    return 0.0f;
}

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

distance proximityFromValue(int value) {
    switch (std::clamp(value, 0, 3)) {
        case 0:
            return distance::CLOSE;
        case 1:
            return distance::MEDIUM;
        case 2:
            return distance::FAR;
        case 3:
        default:
            return distance::UNAVAILABLE;
    }
}

} // namespace

// State builders are defined below the self-test functions that exercise them.
state makeState(int intent, distance proximity, Level energy, Level stress,
                Level curiosity, Level attachment, int userStatus, float memory);
state makeStateWithContext(int intent, distance proximity, Level energy, Level stress,
                           Level curiosity, Level attachment, int userStatus, float memory,
                           float confidence, float relevance, float recency,
                           const AffectiveState& affect);
state makeStateFromMemoryRecords(int intent, distance proximity, Level energy, Level stress,
                                 Level curiosity, Level attachment, int userStatus, float memory,
                                 const std::vector<record>& records, const AffectiveState& affect);
state makeRuntimeState(const RuntimeContext& runtimeContext);

int summarizeMemoryBias(float confidence, float relevance, float recency) {
    const float normalized = std::clamp(
        0.45f * confidence + 0.35f * relevance + 0.20f * recency,
        0.0f,
        1.0f);

    if (normalized < 0.33f) {
        return 0;
    }
    if (normalized < 0.66f) {
        return 1;
    }
    return 2;
}

int summarizeAffectTrend(const AffectiveState& affect, Level energy) {
    const float energyNorm = normalizeEnergyLevel(energy);
    const float energyLowBias = std::pow(1.0f - energyNorm, 2.0f);

    float valenceWeight = 0.0f;
    switch (affect.valence) {
        case AffectValence::POSITIVE:
            valenceWeight = 1.0f;
            break;
        case AffectValence::NEGATIVE:
            valenceWeight = -1.0f;
            break;
        case AffectValence::NEUTRAL:
        default:
            valenceWeight = 0.0f;
            break;
    }

    const float affectScore = std::clamp(
        0.25f * valenceWeight +
            0.25f * affect.arousal +
            0.20f * affect.engagement +
            0.15f * affect.affinity -
            0.20f * affect.stress -
            0.35f * energyLowBias,
        -1.0f,
        1.0f);

    if (affectScore < -0.10f) {
        return 0;
    }
    if (std::abs(affectScore) <= 0.10f) {
        return 1;
    }
    return 2;
}

std::size_t mixEncode(const std::map<std::string, distance>& environmentCondition,
                      Level energy,
                      Level stress,
                      Level curiosity,
                      Level attachment,
                      const AffectiveState& affect,
                      float encoded_user_input,
                      float memory,
                      int memoryBias,
                      int affectTrend) {
    const int intent = std::clamp(static_cast<int>(std::round(encoded_user_input)), 0, 3);

    int proximity = 0;
    const auto proximityIt = environmentCondition.find("target_proximity");
    if (proximityIt != environmentCondition.end()) {
        switch (proximityIt->second) {
            case distance::CLOSE:
                proximity = 0;
                break;
            case distance::MEDIUM:
                proximity = 1;
                break;
            case distance::FAR:
                proximity = 2;
                break;
            case distance::UNAVAILABLE:
                proximity = 3;
                break;
        }
    }

    int userStatus = 0;
    const auto userIt = environmentCondition.find("user_status");
    if (userIt != environmentCondition.end()) {
        switch (userIt->second) {
            case distance::CLOSE:
                userStatus = 0;
                break;
            case distance::MEDIUM:
                userStatus = 1;
                break;
            case distance::FAR:
                userStatus = 2;
                break;
            case distance::UNAVAILABLE:
                userStatus = 3;
                break;
        }
    }

    const auto energyIdx = static_cast<int>(energy);
    const auto stressIdx = static_cast<int>(stress);
    const auto curiosityIdx = static_cast<int>(curiosity);
    const auto attachmentIdx = static_cast<int>(attachment);

    const int valenceIdx = static_cast<int>(affect.valence);
    const int affectEnergyIdx = static_cast<int>(affect.energy);
    const int arousalIdx = std::clamp(static_cast<int>(std::round(affect.arousal * 2.0f)), 0, 2);
    const int engagementIdx = std::clamp(static_cast<int>(std::round(affect.engagement * 2.0f)), 0, 2);

    const std::size_t base =
        (((((((((intent * 4 + proximity) * 4 + userStatus) * 4 + energyIdx) * 4 + stressIdx) * 4 + curiosityIdx) * 4 + attachmentIdx) * 3 + valenceIdx) * 3 + affectEnergyIdx) * 3 + arousalIdx) * 3 + engagementIdx;
    const std::size_t encoded = (base * 3 + static_cast<std::size_t>(memoryBias)) * 3 + static_cast<std::size_t>(affectTrend);

    return encoded + static_cast<std::size_t>(std::round(memory));
}

bool testBasicQUpdate() {
    QTable table(0.1, 0.9);
    const state current = makeState(1, distance::CLOSE, Level::LOW, Level::LOW, Level::MEDIUM, Level::LOW, 0, 0.0f);
    const state next = makeState(1, distance::MEDIUM, Level::LOW, Level::LOW, Level::HIGH, Level::LOW, 0, 0.0f);

    const std::size_t currentState = current.encode();
    const std::size_t nextState = next.encode();
    table.update(currentState, "FETCH", 1.0, nextState);

    const double qValue = table.getQValue(currentState, "FETCH");
    const double expected = 0.1;
    return nearlyEqual(qValue, expected, 1e-6);
}

bool testContextSummaryChangesState() {
    const AffectiveState neutralAffect{};
    const state baseline = makeStateWithContext(
        1,
        distance::CLOSE,
        Level::LOW,
        Level::LOW,
        Level::MEDIUM,
        Level::LOW,
        0,
        0.0f,
        0.1f,
        0.1f,
        0.1f,
        neutralAffect);

    const state highMemory = makeStateWithContext(
        1,
        distance::CLOSE,
        Level::LOW,
        Level::LOW,
        Level::MEDIUM,
        Level::LOW,
        0,
        0.0f,
        0.9f,
        0.9f,
        0.9f,
        neutralAffect);

    state lowEnergyVariant = baseline;
    lowEnergyVariant.affect = AffectiveState{AffectValence::NEGATIVE, AffectEnergy::LOW, 0.2f, 0.1f, 0.8f, 0.1f};
    lowEnergyVariant.energy = Level::LOW;
    lowEnergyVariant.affect_trend = summarizeAffectTrend(lowEnergyVariant.affect, lowEnergyVariant.energy);

    return baseline.encode() != highMemory.encode() && baseline.encode() != lowEnergyVariant.encode();
}

bool testLiveMemorySummariesChangeState() {
    const AffectiveState neutralAffect{};
    const std::vector<record> noMemory;

    record strongEntryA;
    strongEntryA.raw_fact.subject = "user";
    strongEntryA.raw_fact.relation = "prefers";
    strongEntryA.raw_fact.object = "quiet";
    strongEntryA.confidence = 0.9f;
    strongEntryA.relevance = 0.8f;
    strongEntryA.recency = 0.9f;

    record strongEntryB;
    strongEntryB.raw_fact.subject = "user";
    strongEntryB.raw_fact.relation = "likes";
    strongEntryB.raw_fact.object = "music";
    strongEntryB.confidence = 0.7f;
    strongEntryB.relevance = 0.9f;
    strongEntryB.recency = 0.8f;

    const std::vector<record> strongMemory = {strongEntryA, strongEntryB};

    const state neutralState = makeStateFromMemoryRecords(
        1,
        distance::CLOSE,
        Level::MEDIUM,
        Level::LOW,
        Level::MEDIUM,
        Level::LOW,
        0,
        0.0f,
        noMemory,
        neutralAffect);

    const state memoryAwareState = makeStateFromMemoryRecords(
        1,
        distance::CLOSE,
        Level::MEDIUM,
        Level::LOW,
        Level::MEDIUM,
        Level::LOW,
        0,
        0.0f,
        strongMemory,
        neutralAffect);

    return neutralState.encode() != memoryAwareState.encode();
}

bool testPersistenceRoundTripAndAggregation() {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "is2_persistence_roundtrip";
    std::filesystem::create_directories(dir);

    record memoryRecord;
    memoryRecord.raw_fact.subject = "user";
    memoryRecord.raw_fact.relation = "prefers";
    memoryRecord.raw_fact.object = "quiet";
    memoryRecord.confidence = 0.8f;
    memoryRecord.relevance = 0.7f;
    memoryRecord.recency = 0.9f;
    memoryRecord.current_context = record::USER_SPECIFIC;
    const std::vector<record> records = {memoryRecord};

    const std::filesystem::path memoryPath = dir / "memory_records.json";
    const std::filesystem::path affectPath = dir / "affect_state.json";
    const std::filesystem::path sessionPath = dir / "session_context.json";
    const std::filesystem::path historyPath = dir / "interaction_history.json";
    const std::filesystem::path aggPath = dir / "aggregated_facts.json";

    if (!saveMemoryRecords(memoryPath.string(), records)) {
        return false;
    }
    const auto loadedRecords = loadMemoryRecords(memoryPath.string());

    const AffectiveState affect{AffectValence::POSITIVE, AffectEnergy::HIGH, 0.8f, 0.9f, 0.2f, 0.7f};
    if (!saveAffectiveState(affectPath.string(), affect)) {
        return false;
    }
    const auto loadedAffect = loadAffectiveState(affectPath.string());

    SessionContext context;
    context.current_intent = 2;
    context.target_proximity = 1;
    context.user_status = 0;
    context.energy_level = 1;
    context.stress_level = 0;
    context.current_action = "FETCH";
    context.last_timestamp = "2026-10-09";
    context.last_reward = 1.0f;
    if (!saveSessionContext(sessionPath.string(), context)) {
        return false;
    }
    const auto loadedContext = loadSessionContext(sessionPath.string());

    const std::vector<InteractionEvent> history = {
        {"2026-10-09T10:00:00Z", "FETCH", "user prefers quiet space", 0.5f, "quiet space", 0.4f},
        {"2026-10-09T10:05:00Z", "FETCH", "user prefers quiet space", 0.6f, "quiet space", 0.5f},
        {"2026-10-09T10:10:00Z", "FETCH", "user prefers quiet space", 0.7f, "quiet space", 0.6f}
    };
    if (!saveInteractionHistory(historyPath.string(), history)) {
        return false;
    }
    const auto loadedHistory = loadInteractionHistory(historyPath.string());

    const auto aggregated = aggregateRestStateFacts(loadedHistory, loadedRecords);
    if (!saveAggregateFacts(aggPath.string(), aggregated)) {
        return false;
    }
    const auto loadedAggregate = loadAggregateFacts(aggPath.string());

    return !loadedRecords.empty() &&
           loadedRecords.front().raw_fact.subject == "user" &&
           loadedAffect.affinity == affect.affinity &&
           loadedContext.current_action == "FETCH" &&
           !loadedHistory.empty() &&
           !loadedAggregate.empty() &&
           loadedAggregate.front().confidence <= 0.35f;
}

bool testUnseenStateHandling() {
    QLearning learner(0.1f, 0.9f, 0.0f, 42);
    learner.setActions(kActions);
    const state unseen = makeState(3, distance::FAR, Level::HIGH, Level::MEDIUM, Level::LOW, Level::MEDIUM, 2, 0.0f);
    const std::string action = learner.chooseAction(unseen);
    return std::find(kActions.begin(), kActions.end(), action) != kActions.end();
}

bool testContextualFallbackMatchesIntent() {
    EnvironmentConfig config;
    config.seed = 7;
    BaselineAgent agent(config);
    agent.setExplorationRate(0.0f);
    EnvironmentObservation greeting;
    greeting.goal_state = 0; greeting.user_status = 0; greeting.target_proximity = 1; greeting.task_status = 0; greeting.signal_strength = 0.45f;
    const bool greetingOk = agent.selectAction(greeting) == "GREETING";
    EnvironmentObservation request;
    request.goal_state = 1; request.user_status = 1; request.target_proximity = 1; request.task_status = 1; request.signal_strength = 0.90f;
    const bool requestOk = agent.selectAction(request) == "FETCH";
    EnvironmentObservation disturbance;
    disturbance.goal_state = 2; disturbance.user_status = 0; disturbance.target_proximity = 0; disturbance.task_status = 0; disturbance.signal_strength = 0.95f;
    const bool disturbanceOk = agent.selectAction(disturbance) == "BARK";
    EnvironmentObservation rest = disturbance;
    rest.signal_strength = 0.20f;
    const bool restOk = agent.selectAction(rest) == "SLEEP_LAY";
    return greetingOk && requestOk && disturbanceOk && restOk;
}

bool testProposedAgentAdaptsAfterNegativeFeedback() {
    EnvironmentConfig config;
    config.seed = 11;
    ProposedAgent agent(config);
    agent.setExplorationRate(0.0f);
    agent.setLearningEnabled(false);
    EnvironmentObservation disturbance;
    disturbance.goal_state = 2; disturbance.user_status = 0; disturbance.target_proximity = 0; disturbance.task_status = 0; disturbance.signal_strength = 0.95f;
    const std::string firstAction = agent.selectAction(disturbance);
    ActionOutcome negative;
    negative.reward = -1.0f; negative.terminal = true; negative.outcome_tag = "participant_negative";
    agent.applyOutcome(disturbance, firstAction, negative);
    const std::string nextAction = agent.selectAction(disturbance);
    return firstAction == "BARK" && nextAction == "ACT_CONFUSED";
}
bool testSemanticPreferenceSurvivesInterveningInteractions() {
    EnvironmentConfig config;
    ProposedAgent agent(config);
    agent.setExplorationRate(0.0f);
    agent.setLearningEnabled(false);
    agent.recordUserPreference("BARK", false);

    const EnvironmentObservation greeting{0, 1, 0, 0, 0.55f};
    ActionOutcome positive;
    positive.reward = 1.0f;
    positive.terminal = true;
    positive.outcome_tag = "positive";
    for (int i = 0; i < 3; ++i) {
        const std::string action = agent.selectAction(greeting);
        agent.applyOutcome(greeting, action, positive);
    }

    const EnvironmentObservation disturbance{2, 0, 0, 0, 0.95f};
    return agent.selectAction(disturbance) == "ACT_CONFUSED";
}

bool testMemoryRetentionAfterInterveningInteractions() {
    EnvironmentConfig config;
    ProposedAgent agent(config);
    agent.setExplorationRate(0.0f);
    agent.setLearningEnabled(false);
    EnvironmentObservation disturbance{2, 0, 0, 0, 0.95f};
    const std::string firstAction = agent.selectAction(disturbance);
    ActionOutcome negative;
    negative.reward = -1.0f;
    negative.terminal = true;
    negative.outcome_tag = "negative";
    agent.applyOutcome(disturbance, firstAction, negative);

    const EnvironmentObservation greeting{0, 1, 0, 0, 0.55f};
    ActionOutcome positive;
    positive.reward = 1.0f;
    positive.terminal = true;
    positive.outcome_tag = "positive";
    for (int i = 0; i < 4; ++i) {
        const std::string action = agent.selectAction(greeting);
        agent.applyOutcome(greeting, action, positive);
    }

    const std::string laterAction = agent.selectAction(disturbance);
    return firstAction == "BARK" &&
           laterAction == "ACT_CONFUSED" &&
           !agent.getMemoryRecords().empty();
}

bool testProposedAgentAvoidsRejectedAction() {
    EnvironmentConfig config;
    config.seed = 13;
    ProposedAgent agent(config);
    agent.setExplorationRate(0.0f);
    agent.setLearningEnabled(false);
    EnvironmentObservation greeting;
    greeting.goal_state = 0; greeting.user_status = 0; greeting.target_proximity = 1; greeting.task_status = 0; greeting.signal_strength = 0.55f;
    const std::string firstAction = agent.selectAction(greeting);
    ActionOutcome negative;
    negative.reward = -1.0f; negative.terminal = true; negative.outcome_tag = "participant_negative";
    agent.applyOutcome(greeting, firstAction, negative);
    const std::string nextAction = agent.selectAction(greeting);
    return firstAction == "GREETING" &&
           nextAction != firstAction &&
           isActionValidForObservation(nextAction, greeting) &&
           !agent.getMemoryRecords().empty();
}
bool testTaskAndSignalAreEncoded() {
    state base = makeState(1, distance::CLOSE, Level::MEDIUM, Level::LOW, Level::MEDIUM, Level::LOW, 1, 0.0f);
    base.task_status = 0;
    base.signal_strength_bucket = 0;
    state taskVariant = base;
    taskVariant.task_status = 1;
    state signalVariant = base;
    signalVariant.signal_strength_bucket = 2;
    return base.encode() != taskVariant.encode() && base.encode() != signalVariant.encode();
}
bool testPersistence() {
    const std::filesystem::path savePath = std::filesystem::temp_directory_path() / "is2_qtable_test.txt";

    QTable original(0.1, 0.9);
    const state s = makeState(1, distance::MEDIUM, Level::LOW, Level::LOW, Level::MEDIUM, Level::LOW, 1, 0.0f);
    const std::size_t stateId = s.encode();
    original.update(stateId, "BARK", 0.75, stateId);
    original.save(savePath.string());

    QTable loaded(0.1, 0.9);
    loaded.load(savePath.string());
    const double reloaded = loaded.getQValue(stateId, "BARK");
    return nearlyEqual(reloaded, original.getQValue(stateId, "BARK"), 1e-6);
}

bool testTerminalHandling() {
    QTable table(0.1, 0.9);
    const state s = makeState(2, distance::FAR, Level::MEDIUM, Level::LOW, Level::LOW, Level::LOW, 0, 0.0f);
    const state terminal = makeState(2, distance::UNAVAILABLE, Level::LOW, Level::LOW, Level::LOW, Level::LOW, 0, 0.0f);
    table.table[terminal.encode()]["FETCH"] = 100.0;
    table.update(s.encode(), "ACT_CONFUSED", -1.0, terminal.encode(), true);
    const double value = table.getQValue(s.encode(), "ACT_CONFUSED");
    return nearlyEqual(value, -0.1, 1e-6);
}

bool testRuntimeLifecycleAndEnvironmentContract() {
    RuntimeContext runtime;
    runtime.session_context.current_intent = 2;
    runtime.session_context.target_proximity = 1;
    runtime.session_context.user_status = 1;
    runtime.session_context.energy_level = 1;
    runtime.session_context.stress_level = 0;
    runtime.session_context.current_action = "FETCH";
    runtime.memory_summary.memory_bias = 0.8f;
    runtime.memory_summary.confidence_weight = 0.8f;
    runtime.memory_summary.signal_quality = 0.7f;
    runtime.memory_summary.recency_weight = 0.9f;
    runtime.affective_state = AffectiveState{AffectValence::POSITIVE, AffectEnergy::HIGH, 0.8f, 0.9f, 0.2f, 0.7f};

    const state runtimeState = makeRuntimeState(runtime);
    const auto normalized = normaliseRuntimeObservation(runtime);
    const bool stateChanged = runtimeState.encode() != makeState(2, distance::MEDIUM, Level::MEDIUM, Level::LOW, Level::MEDIUM, Level::LOW, 1, 0.0f).encode();

    EnvironmentObservation observation;
    observation.goal_state = 1;
    observation.target_proximity = 1;
    observation.user_status = 1;
    observation.task_status = 1;
    observation.signal_strength = 0.75f;

    ActionResult actionResult;
    actionResult.action_id = 0;
    actionResult.reward = 1.0f;
    actionResult.terminal = false;
    actionResult.outcome_tag = "observed";

    return stateChanged && normalized.memory_bias > 0.0f && normalized.affect_trend > 0.0f &&
           observation.target_proximity == 1 && actionResult.reward > 0.0f;
}

state makeState(int intent,
                distance proximity,
                Level energy,
                Level stress,
                Level curiosity,
                Level attachment,
                int userStatus,
                float memory) {
    return makeStateWithContext(
        intent,
        proximity,
        energy,
        stress,
        curiosity,
        attachment,
        userStatus,
        memory,
        0.0f,
        0.0f,
        0.0f,
        AffectiveState{});
}

state makeStateWithContext(int intent,
                          distance proximity,
                          Level energy,
                          Level stress,
                          Level curiosity,
                          Level attachment,
                          int userStatus,
                          float memory,
                          float confidence,
                          float relevance,
                          float recency,
                          const AffectiveState& affect) {
    state s;
    s.energy = energy;
    s.stress = stress;
    s.curiosity = curiosity;
    s.attachment = attachment;
    s.affect = affect;
    s.memory = memory;
    s.encoded_user_input = static_cast<float>(intent);
    s.environmentCondition["target_proximity"] = proximity;
    s.environmentCondition["user_status"] = static_cast<distance>(userStatus);
    s.memory_bias = summarizeMemoryBias(confidence, relevance, recency);
    s.affect_trend = summarizeAffectTrend(s.affect, s.energy);
    return s;
}

state makeStateFromMemoryRecords(int intent,
                                distance proximity,
                                Level energy,
                                Level stress,
                                Level curiosity,
                                Level attachment,
                                int userStatus,
                                float memory,
                                const std::vector<record>& records,
                                const AffectiveState& affect) {
    const std::vector<record> rankedRecords = rankRelevantRecords(records, "");
    const float memorySignal = summarizeWeightedMemorySignal(rankedRecords);
    return makeStateWithContext(
        intent,
        proximity,
        energy,
        stress,
        curiosity,
        attachment,
        userStatus,
        memory,
        memorySignal,
        memorySignal,
        memorySignal,
        affect);
}

state makeRuntimeState(const RuntimeContext& runtimeContext) {
    const float confidence = std::clamp(runtimeContext.memory_summary.confidence_weight, 0.0f, 1.0f);
    const float relevance = std::clamp(runtimeContext.memory_summary.signal_quality, 0.0f, 1.0f);
    const float recency = std::clamp(runtimeContext.memory_summary.recency_weight, 0.0f, 1.0f);

    return makeStateWithContext(
        runtimeContext.session_context.current_intent,
        proximityFromValue(runtimeContext.session_context.target_proximity),
        levelFromValue(runtimeContext.session_context.energy_level),
        levelFromValue(runtimeContext.session_context.stress_level),
        Level::MEDIUM,
        Level::LOW,
        runtimeContext.session_context.user_status,
        static_cast<float>(runtimeContext.memory_summary.memory_bias),
        confidence,
        relevance,
        recency,
        runtimeContext.affective_state);
}

std::size_t state::encode() const {
    const std::size_t base = mixEncode(
        environmentCondition,
        energy,
        stress,
        curiosity,
        attachment,
        affect,
        encoded_user_input,
        memory,
        memory_bias,
        affect_trend);
    const int boundedTask = std::clamp(task_status, 0, 1);
    const int boundedSignal = std::clamp(signal_strength_bucket, 0, 2);
    const std::size_t withSignal =
        (base * 2u + static_cast<std::size_t>(boundedTask)) * 3u +
        static_cast<std::size_t>(boundedSignal);
    return (withSignal * 2u + static_cast<std::size_t>(food_available)) * 2u +
           static_cast<std::size_t>(play_available);
}

QTable::QTable(double learningRate, double discountFactor)
    : learningRate(learningRate), discountFactor(discountFactor) {}

double QTable::getQValue(std::size_t stateKey, std::string action) const {
    const auto stateIt = table.find(stateKey);
    if (stateIt == table.end()) {
        return 0.0;
    }

    const auto actionIt = stateIt->second.find(action);
    if (actionIt == stateIt->second.end()) {
        return 0.0;
    }

    return actionIt->second;
}

void QTable::update(std::size_t stateKey, std::string action, double reward, std::size_t nextState, bool terminal) {
    const double oldValue = getQValue(stateKey, action);
    double bestNextValue = 0.0;

    if (!terminal) {
        const auto nextIt = table.find(nextState);
        if (nextIt != table.end()) {
            for (const auto& entry : nextIt->second) {
                if (entry.second > bestNextValue) {
                    bestNextValue = entry.second;
                }
            }
        }
    }

    const double newValue = oldValue + learningRate * (reward + (terminal ? 0.0 : discountFactor * bestNextValue) - oldValue);
    table[stateKey][action] = newValue;
}


void QTable::save(const std::string& filename) {
    std::ofstream out(filename);
    if (!out.is_open()) {
        throw std::runtime_error("Unable to open Q-table save file: " + filename);
    }

    for (const auto& [stateKey, actionMap] : table) {
        for (const auto& [action, value] : actionMap) {
            out << stateKey << " " << action << " " << value << '\n';
        }
    }
}

void QTable::load(const std::string& filename) {
    std::ifstream in(filename);
    if (!in.is_open()) {
        return;
    }

    table.clear();
    std::size_t stateKey = 0;
    std::string action;
    double value = 0.0;
    while (in >> stateKey >> action >> value) {
        table[stateKey][action] = value;
    }
}

QLearning::QLearning(float learningRate, float discountFactor, float epsilon, unsigned int seed)
    : q_table(learningRate, discountFactor),
      learning_rate(learningRate),
      discount_factor(discountFactor),
      epsilon(epsilon),
      rng(seed) {}


void QLearning::setActions(const std::vector<std::string>& newActions) {
    actions = newActions;
}

std::string QLearning::chooseAction(const state& stateRef) const {
    const std::string fallback = actions.empty() ? "WANDER" : actions.front();
    return chooseAction(stateRef, fallback);
}

std::string QLearning::chooseAction(const state& stateRef,
                                    const std::string& contextualFallback) const {
    return chooseAction(stateRef, contextualFallback, actions);
}

std::string QLearning::chooseAction(const state& stateRef,
                                    const std::string& contextualFallback,
                                    const std::vector<std::string>& validActions) const {
    if (validActions.empty())
        return "WANDER";

    const auto isValid = [&](const std::string& action) {
        return std::find(validActions.begin(), validActions.end(), action) != validActions.end();
    };
    const std::string fallback = isValid(contextualFallback) ? contextualFallback : validActions.front();
    const auto stateIt = q_table.table.find(stateRef.encode());

    if (epsilon > 0.0f) {
        std::uniform_real_distribution<float> chance(0.0f, 1.0f);
        if (chance(rng) < epsilon) {
            std::uniform_int_distribution<std::size_t> choice(0, validActions.size() - 1);
            return validActions[choice(rng)];
        }
    }

    if (stateIt == q_table.table.end())
        return fallback;

    // Greedy choice is restricted to valid actions. Unvisited actions do not
    // silently beat learned negative values through their implicit Q=0.
    std::string bestAction;
    double bestValue = -std::numeric_limits<double>::infinity();
    for (const auto& action : validActions) {
        const auto found = stateIt->second.find(action);
        if (found != stateIt->second.end() && found->second > bestValue) {
            bestValue = found->second;
            bestAction = action;
        }
    }
    return bestAction.empty() ? fallback : bestAction;
}

void QLearning::update(const state& current_state, const std::string& action, float reward,
                       const state& next_state, bool terminal,
                       const std::vector<std::string>& validNextActions) {
    const double currentValue = q_table.getQValue(current_state.encode(), action);
    double bestNextValue = 0.0;
    const auto& bootstrapActions = validNextActions.empty() ? actions : validNextActions;
    if (!terminal) {
        for (const auto& candidate : bootstrapActions) {
            const double candidateValue = q_table.getQValue(next_state.encode(), candidate);
            if (candidateValue > bestNextValue) {
                bestNextValue = candidateValue;
            }
        }
    }

    const double bootstrapValue = terminal ? 0.0 : discount_factor * bestNextValue;
    const double updatedValue = currentValue + learning_rate * (reward + bootstrapValue - currentValue);
    q_table.table[current_state.encode()][action] = updatedValue;
}

void QLearning::setEpsilon(float epsilonValue) {
    epsilon = epsilonValue;
}

void QLearning::loadQTable(const std::string& path) {
    q_table.load(path);
}

void QLearning::saveQTable(const std::string& path) {
    q_table.save(path);
}

bool testEvaluationDoesNotUpdatePolicy() {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "is2_eval_policy_test";
    std::filesystem::create_directories(dir);
    const std::filesystem::path beforePath = dir / "before.qtable";
    const std::filesystem::path afterPath = dir / "after.qtable";

    BaselineAgent agent;
    agent.setLearningEnabled(false);
    agent.setExplorationRate(0.0f);
    agent.savePersistentState(beforePath.string());

    EnvironmentObservation observation;
    observation.goal_state = 1;
    observation.target_proximity = 0;
    observation.user_status = 1;
    observation.task_status = 1;
    observation.signal_strength = 0.9f;
    const std::string action = agent.selectAction(observation);
    ActionOutcome outcome;
    outcome.reward = 1.0f;
    outcome.terminal = true;
    outcome.outcome_tag = "evaluation_test";
    agent.applyOutcome(observation, action, outcome);
    agent.savePersistentState(afterPath.string());

    std::ifstream beforeFile(beforePath);
    std::ifstream afterFile(afterPath);
    const std::string before((std::istreambuf_iterator<char>(beforeFile)), std::istreambuf_iterator<char>());
    const std::string after((std::istreambuf_iterator<char>(afterFile)), std::istreambuf_iterator<char>());
    std::filesystem::remove_all(dir);
    return before == after;
}

bool testBaselineExcludesMemoryAndAffectInputs() {
    EnvironmentConfig config;
    BaselineAgent agent(config);
    EnvironmentObservation observation;
    observation.goal_state = 2;
    observation.target_proximity = 1;
    observation.user_status = 1;
    observation.task_status = 1;
    observation.signal_strength = 0.9f;
    agent.selectAction(observation);
    const state& baselineState = agent.getCurrentState();
    return baselineState.memory == 0.0f &&
           baselineState.memory_bias == 0 &&
           baselineState.affect.valence == AffectValence::NEUTRAL;
}

bool testSeededExplorationIsReproducible() {
    const state sample = makeState(1, distance::CLOSE, Level::MEDIUM, Level::LOW, Level::MEDIUM, Level::LOW, 1, 0.0f);
    QLearning first(0.25f, 0.8f, 1.0f, 12345u);
    QLearning second(0.25f, 0.8f, 1.0f, 12345u);
    first.setActions(companionActionSet());
    second.setActions(companionActionSet());
    for (int i = 0; i < 100; ++i) {
        if (first.chooseAction(sample) != second.chooseAction(sample)) {
            return false;
        }
    }
    return true;
}

bool testActionMaskUsesOnlyObservableAvailability() {
    if (companionActionSet().size() != 14) return false;

    const EnvironmentObservation ordinary{1, 1, 1, 1, 0.85f};
    state ordinaryState = makeState(1, distance::CLOSE, Level::MEDIUM, Level::LOW,
                                    Level::MEDIUM, Level::LOW, 1, 0.0f);
    state foodState = ordinaryState;
    foodState.food_available = true;
    state playState = ordinaryState;
    playState.play_available = true;
    if (ordinaryState.encode() == foodState.encode() ||
        ordinaryState.encode() == playState.encode() ||
        foodState.encode() == playState.encode()) {
        return false;
    }

    const auto ordinaryActions = validActionsForObservation(ordinary);
    if (std::find(ordinaryActions.begin(), ordinaryActions.end(), "FETCH") == ordinaryActions.end() ||
        std::find(ordinaryActions.begin(), ordinaryActions.end(), "EAT") != ordinaryActions.end() ||
        std::find(ordinaryActions.begin(), ordinaryActions.end(), "PLAY") != ordinaryActions.end()) {
        return false;
    }

    const EnvironmentObservation food{2, 0, 1, 0, 0.30f, true, false};
    const auto foodActions = validActionsForObservation(food);
    if (std::find(foodActions.begin(), foodActions.end(), "EAT") == foodActions.end() ||
        std::find(foodActions.begin(), foodActions.end(), "PLAY") != foodActions.end()) {
        return false;
    }

    const EnvironmentObservation play{0, 1, 1, 0, 0.70f, false, true};
    const auto playActions = validActionsForObservation(play);
    if (std::find(playActions.begin(), playActions.end(), "PLAY") == playActions.end() ||
        std::find(playActions.begin(), playActions.end(), "EAT") != playActions.end()) {
        return false;
    }

    const EnvironmentObservation noResponse{1, 1, 0, 1, 0.82f};
    const auto noResponseActions = validActionsForObservation(noResponse);
    if (std::find(noResponseActions.begin(), noResponseActions.end(), "INSIST") == noResponseActions.end())
        return false;

    const EnvironmentObservation rejection{0, 1, 2, 0, 0.45f};
    const auto rejectionActions = validActionsForObservation(rejection);
    if (std::find(rejectionActions.begin(), rejectionActions.end(), "INSIST") != rejectionActions.end())
        return false;
    for (const auto& disallowed : {"GREETING", "NUZZLE", "APPROACH_FOLLOW"}) {
        if (std::find(rejectionActions.begin(), rejectionActions.end(), disallowed) != rejectionActions.end())
            return false;
    }
    if (std::find(rejectionActions.begin(), rejectionActions.end(), "HIDE_AVOID") == rejectionActions.end() ||
        std::find(rejectionActions.begin(), rejectionActions.end(), "WHINE") == rejectionActions.end()) {
        return false;
    }

    EnvironmentConfig config;
    BaselineAgent baseline(config);
    ProposedAgent proposed(config);
    baseline.setExplorationRate(1.0f);
    proposed.setExplorationRate(1.0f);
    for (const auto& observation : {ordinary, food, play, noResponse, rejection}) {
        const std::string baselineAction = baseline.selectAction(observation);
        const std::string proposedAction = proposed.selectAction(observation);
        if (!isActionValidForObservation(baselineAction, observation) ||
            !isActionValidForObservation(proposedAction, observation)) {
            return false;
        }
    }
    return true;
}

bool testMemoryAffectStateEncoding() {
    const state neutral = makeState(1, distance::CLOSE, Level::LOW, Level::LOW, Level::MEDIUM, Level::LOW, 0, 0.0f);
    state positive = neutral;
    positive.affect = AffectiveState{AffectValence::POSITIVE, AffectEnergy::HIGH, 0.8f, 0.9f, 0.2f, 0.7f};
    state negative = neutral;
    negative.affect = AffectiveState{AffectValence::NEGATIVE, AffectEnergy::LOW, 0.2f, 0.1f, 0.8f, 0.1f};

    return neutral.encode() != positive.encode() && neutral.encode() != negative.encode() && positive.encode() != negative.encode();
}

struct TestResultRecord {
    std::string name;
    std::string status;
    bool passed = false;
    std::string category;
};

void writeValidationReport(const std::vector<TestResultRecord>& results, const std::string& outputPath) {
    std::filesystem::path path(outputPath);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    nlohmann::json payload = nlohmann::json::object();
    const std::time_t now = std::time(nullptr);
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));

    payload["project"] = "IS-2";
    payload["generated_at"] = std::string(stamp);
    payload["status"] = std::all_of(results.begin(), results.end(), [](const TestResultRecord& result) {
        return result.passed;
    })
        ? "PASS"
        : "FAIL";

    nlohmann::json tests = nlohmann::json::array();
    for (const auto& result : results) {
        nlohmann::json entry = nlohmann::json::object();
        entry["name"] = result.name;
        entry["status"] = result.status;
        entry["passed"] = result.passed;
        entry["category"] = result.category;
        tests.push_back(entry);
    }

    payload["tests"] = tests;
    payload["summary"] = nlohmann::json::object();
    payload["summary"]["total"] = static_cast<int>(tests.size());
    payload["summary"]["passed"] = std::count_if(results.begin(), results.end(), [](const TestResultRecord& result) {
        return result.passed;
    });
    payload["summary"]["failed"] = static_cast<int>(results.size()) - static_cast<int>(payload["summary"]["passed"]);

    std::ofstream out(path);
    if (!out.is_open()) {
        return;
    }
    out << payload.dump(2);
    out << '\n';
}

namespace {
std::string contextualAction(const EnvironmentObservation& observation,
                             const AffectiveState* affect = nullptr,
                             const std::vector<record>* memories = nullptr) {
    // Explicit semantic preferences are consulted before generic fallbacks.
    if (memories != nullptr) {
        for (auto it = memories->rbegin(); it != memories->rend(); ++it) {
            if (it->confidence < 0.4f) continue;
            if (observation.goal_state == 1 && observation.task_status == 1 &&
                observation.target_proximity <= 1 && it->raw_fact.object == "FETCH") {
                if (it->raw_fact.relation == "prefers_action") return "FETCH";
                if (it->raw_fact.relation == "dislikes_action") return "ACT_CONFUSED";
            }
            if (observation.goal_state == 2 && observation.signal_strength >= 0.80f &&
                it->raw_fact.object == "BARK") {
                if (it->raw_fact.relation == "prefers_action") return "BARK";
                if (it->raw_fact.relation == "dislikes_action") return "ACT_CONFUSED";
            }
            if (observation.goal_state == 2 && observation.task_status == 0 &&
                observation.signal_strength < 0.80f && it->raw_fact.object == "SLEEP_LAY") {
                if (it->raw_fact.relation == "prefers_action") return "SLEEP_LAY";
                if (it->raw_fact.relation == "dislikes_action") return "WANDER";
            }
        }
    }
    // goal_state: 0=social greeting, 1=explicit task/request, 2=unexpected event/rest.
    if (observation.goal_state == 0) {
        if (observation.user_status == 0) return "GREETING";
        if (affect != nullptr && affect->stress >= 0.75f) return "ACT_CONFUSED";
        return observation.target_proximity == 0 ? "NUZZLE" : "GREETING";
    }

    if (observation.goal_state == 1) {
        if (observation.task_status == 1 && observation.target_proximity <= 1) return "FETCH";
        if (observation.task_status == 0) return "ACT_CONFUSED";
        return "WANDER";
    }

    // An unexpected disturbance should not trigger an unrelated fetch.
    if (observation.signal_strength >= 0.80f) {
        if (affect != nullptr && affect->stress >= 0.70f) return "ACT_CONFUSED";
        return "BARK";
    }
    if (observation.task_status == 0) return "SLEEP_LAY";

    // A negative response to insistence is an explicit reason to avoid insisting again.
    if (affect != nullptr && affect->valence == AffectValence::NEGATIVE) return "WANDER";
    if (memories != nullptr) {
        for (auto it = memories->rbegin(); it != memories->rend(); ++it) {
            if (it->raw_fact.relation == "response_to_action" &&
                it->raw_fact.object == "negative" &&
                it->confidence <= 0.5f &&
                it->raw_fact.subject == "INSIST") {
                return "WANDER";
            }
        }
    }
    return "ACT_CONFUSED";
}

bool wasActionRejected(const std::string& action, const std::vector<record>& memories) {
    for (auto it = memories.rbegin(); it != memories.rend(); ++it) {
        if (it->raw_fact.relation == "response_to_action" && it->raw_fact.subject == action) {
            return it->raw_fact.object == "negative" && it->confidence <= 0.5f;
        }
    }
    return false;
}

std::string alternativeAfterRejection(
    const std::string& rejectedAction,
    const EnvironmentObservation& observation) {
    // Keep the preferred alternative when it is possible in this context,
    // otherwise choose the first valid action that differs from the rejected one.
    std::vector<std::string> candidates;
    if (rejectedAction == "GREETING") candidates.push_back("NUZZLE");
    else if (rejectedAction == "NUZZLE") candidates.push_back("GREETING");
    else if (rejectedAction == "BARK") candidates.push_back("ACT_CONFUSED");
    else if (rejectedAction == "ACT_CONFUSED") candidates.push_back("WANDER");
    else if (rejectedAction == "FETCH") candidates.push_back("ACT_CONFUSED");
    else if (rejectedAction == "INSIST" || rejectedAction == "SLEEP_LAY") candidates.push_back("WANDER");

    for (const auto& candidate : companionActionSet()) {
        if (candidate != rejectedAction &&
            std::find(candidates.begin(), candidates.end(), candidate) == candidates.end())
            candidates.push_back(candidate);
    }
    for (const auto& candidate : candidates) {
        if (candidate != rejectedAction && isActionValidForObservation(candidate, observation))
            return candidate;
    }
    return "WANDER";
}
std::string currentTimeStamp() {
    const auto now = std::chrono::system_clock::now();
    const auto timer = std::chrono::system_clock::to_time_t(now);
    std::tm gmt{};
    gmtime_r(&timer, &gmt);
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%SZ", &gmt);
    return std::string(stamp);
}
} // namespace

BaselineAgent::BaselineAgent(const EnvironmentConfig& configValue)
    : config(configValue), q_policy(0.25f, 0.8f, 0.15f, static_cast<unsigned int>(configValue.seed)) {
    q_policy.setActions(config.actions);
    reset();
}

void BaselineAgent::reset() {
    session_context = SessionContext{};
    session_context.current_action = "IDLE";
    session_context.last_timestamp = currentTimeStamp();
    current_state = makeState(
        0,
        distance::FAR,
        Level::MEDIUM,
        Level::LOW,
        Level::MEDIUM,
        Level::LOW,
        0,
        0.0f);
    interaction_history.clear();
    q_policy.setActions(config.actions);
}

void BaselineAgent::setExplorationRate(float epsilon) {
    q_policy.setEpsilon(std::clamp(epsilon, 0.0f, 1.0f));
}

void BaselineAgent::setLearningEnabled(bool enabled) {
    learning_enabled = enabled;
}

std::string BaselineAgent::selectAction(const EnvironmentObservation& observation) {
    session_context.current_intent = observation.goal_state;
    session_context.target_proximity = observation.target_proximity;
    session_context.user_status = observation.user_status;
    const state candidate = makeState(
        session_context.current_intent,
        proximityFromValue(observation.target_proximity),
        Level::MEDIUM, // Baseline does not observe the environment's hidden energy variable.
        Level::MEDIUM,
        Level::MEDIUM,
        Level::LOW,
        observation.user_status,
        0.0f);
    state observedState = candidate;
    observedState.task_status = observation.task_status;
    observedState.signal_strength_bucket =
        observation.signal_strength < 0.33f ? 0 : (observation.signal_strength < 0.67f ? 1 : 2);
    observedState.food_available = observation.food_available;
    observedState.play_available = observation.play_available;
    current_state = observedState;
    session_context.current_action = q_policy.chooseAction(
        observedState, contextualAction(observation), validActionsForObservation(observation));
    return session_context.current_action;
}

void BaselineAgent::applyOutcome(const EnvironmentObservation& observation,
                                const std::string& action,
                                const ActionOutcome& outcome) {
    applyOutcome(observation, action, outcome, observation);
}

void BaselineAgent::applyOutcome(const EnvironmentObservation& observation,
                                const std::string& action,
                                const ActionOutcome& outcome,
                                const EnvironmentObservation& nextObservation) {
    state next_state = makeState(
        nextObservation.goal_state,
        proximityFromValue(nextObservation.target_proximity),
        Level::MEDIUM, // Do not leak hidden environment energy into the baseline state.
        Level::MEDIUM,
        Level::MEDIUM,
        Level::LOW,
        nextObservation.user_status,
        0.0f);
    next_state.task_status = nextObservation.task_status;
    next_state.signal_strength_bucket =
        nextObservation.signal_strength < 0.33f ? 0 : (nextObservation.signal_strength < 0.67f ? 1 : 2);
    next_state.food_available = nextObservation.food_available;
    next_state.play_available = nextObservation.play_available;

    if (learning_enabled) {
        q_policy.update(current_state, action, outcome.reward, next_state, outcome.terminal, validActionsForObservation(nextObservation));
    }
    session_context.last_reward = outcome.reward;
    session_context.last_timestamp = currentTimeStamp();
    session_context.current_action = action;
    session_context.user_status = observation.user_status;
    session_context.target_proximity = observation.target_proximity;

    InteractionEvent event;
    event.timestamp = session_context.last_timestamp;
    event.action_taken = action;
    event.summary = outcome.outcome_tag;
    event.reward = outcome.reward;
    event.state_summary = "baseline_observation";
    event.affect_signal = std::clamp(observation.signal_strength, 0.0f, 1.0f);
    interaction_history.push_back(event);
    current_state = next_state;
}

void BaselineAgent::loadPersistentState(const std::string& path) {
    q_policy.loadQTable(path);
}

void BaselineAgent::savePersistentState(const std::string& path) {
    q_policy.saveQTable(path);
}

const state& BaselineAgent::getCurrentState() const {
    return current_state;
}

const SessionContext& BaselineAgent::getSessionContext() const {
    return session_context;
}

const std::vector<InteractionEvent>& BaselineAgent::getInteractionHistory() const {
    return interaction_history;
}

ProposedAgent::ProposedAgent(const EnvironmentConfig& configValue)
    : config(configValue), q_policy(0.25f, 0.8f, 0.15f, static_cast<unsigned int>(configValue.seed)), affective_state{} {
    q_policy.setActions(config.actions);
    reset();
}

void ProposedAgent::reset() {
    session_context = SessionContext{};
    session_context.current_action = "IDLE";
    session_context.energy_level = 2;
    session_context.last_timestamp = currentTimeStamp();
    // Memory and affect persist across UI sessions by design. Reset only
    // transient session/history state; the UI's explicit Clear Memory action
    // is the only operation that deletes persisted user-specific memory.
    interaction_history.clear();
    runtime_context = RuntimeContext{};
    current_state = makeState(
        0,
        distance::FAR,
        Level::MEDIUM,
        Level::LOW,
        Level::MEDIUM,
        Level::LOW,
        0,
        0.0f);
    q_policy.setActions(config.actions);
}

void ProposedAgent::setExplorationRate(float epsilon) {
    q_policy.setEpsilon(std::clamp(epsilon, 0.0f, 1.0f));
}

void ProposedAgent::setLearningEnabled(bool enabled) {
    learning_enabled = enabled;
}

std::string ProposedAgent::selectAction(const EnvironmentObservation& observation) {
    session_context.current_intent = observation.goal_state;
    session_context.target_proximity = observation.target_proximity;
    session_context.user_status = observation.user_status;
    runtime_context.session_context = session_context;
    runtime_context.affective_state = affective_state;
    runtime_context.memory_summary = buildMemorySummary(memory_records, "");
    current_state = makeRuntimeState(runtime_context);
    current_state.encoded_user_input = static_cast<float>(observation.goal_state);
    current_state.task_status = observation.task_status;
    current_state.signal_strength_bucket =
        observation.signal_strength < 0.33f ? 0 : (observation.signal_strength < 0.67f ? 1 : 2);
    current_state.food_available = observation.food_available;
    current_state.play_available = observation.play_available;

    session_context.current_intent = observation.goal_state;
    session_context.target_proximity = observation.target_proximity;
    session_context.user_status = observation.user_status;
    const std::string fallback = contextualAction(observation, &affective_state, &memory_records);
    std::string selected = q_policy.chooseAction(
        current_state, fallback, validActionsForObservation(observation));
    // Explicit, sufficiently confident preferences take precedence over a
    // learned action when the current scenario directly matches the preference.
    for (auto it = memory_records.rbegin(); it != memory_records.rend(); ++it) {
        if (it->confidence < 0.4f) continue;
        if (observation.goal_state == 1 && observation.task_status == 1 &&
            observation.target_proximity <= 1 && it->raw_fact.object == "FETCH") {
            if (it->raw_fact.relation == "prefers_action") selected = "FETCH";
            else if (it->raw_fact.relation == "dislikes_action") selected = "ACT_CONFUSED";
            if (it->raw_fact.relation == "prefers_action" || it->raw_fact.relation == "dislikes_action") break;
        }
        if (observation.goal_state == 2 && observation.signal_strength >= 0.80f &&
            it->raw_fact.object == "BARK") {
            if (it->raw_fact.relation == "prefers_action") selected = "BARK";
            else if (it->raw_fact.relation == "dislikes_action") selected = "ACT_CONFUSED";
            if (it->raw_fact.relation == "prefers_action" || it->raw_fact.relation == "dislikes_action") break;
        }
        if (observation.goal_state == 2 && observation.task_status == 0 &&
            observation.signal_strength < 0.80f && it->raw_fact.object == "SLEEP_LAY") {
            if (it->raw_fact.relation == "prefers_action") selected = "SLEEP_LAY";
            else if (it->raw_fact.relation == "dislikes_action") selected = "WANDER";
            if (it->raw_fact.relation == "prefers_action" || it->raw_fact.relation == "dislikes_action") break;
        }
    }
    if (wasActionRejected(selected, memory_records)) {
        selected = alternativeAfterRejection(selected, observation);
    }
    selected = validActionOrFallback(selected, fallback, observation);
    session_context.current_action = selected;
    return session_context.current_action;
}

void ProposedAgent::applyOutcome(const EnvironmentObservation& observation,
                                const std::string& action,
                                const ActionOutcome& outcome) {
    applyOutcome(observation, action, outcome, observation);
}

void ProposedAgent::applyOutcome(const EnvironmentObservation& observation,
                                const std::string& action,
                                const ActionOutcome& outcome,
                                const EnvironmentObservation& nextObservation) {
    session_context.current_intent = observation.goal_state;
    session_context.target_proximity = observation.target_proximity;
    session_context.user_status = observation.user_status;
    session_context.last_reward = outcome.reward;
    session_context.last_timestamp = currentTimeStamp();
    if (outcome.energy_after >= 0.0f) {
        const float boundedEnergy = std::clamp(outcome.energy_after, 0.0f, 1.0f);
        session_context.energy_level = std::clamp(
            static_cast<int>(std::lround(boundedEnergy * 2.0f)), 0, 2);
    }
    affective_state.updateFromFeedback(outcome.reward, outcome.reward > 0.0f, outcome.reward < 0.0f);
    if (outcome.energy_after >= 0.0f) {
        if (outcome.energy_after < 0.34f) affective_state.energy = AffectEnergy::LOW;
        else if (outcome.energy_after < 0.67f) affective_state.energy = AffectEnergy::MODERATE;
        else affective_state.energy = AffectEnergy::HIGH;
    }

    // Confidence estimates observed success, not input signal strength.
    // Repeated outcomes for the same action/context update one record instead
    // of creating an unbounded sequence of equally weighted duplicates.
    const std::string outcomeClass = outcome.reward > 0.05f ? "positive" :
                                     outcome.reward < -0.05f ? "negative" : "neutral";
    auto memoryIt = std::find_if(memory_records.begin(), memory_records.end(),
        [&](const record& existing) {
            return existing.raw_fact.subject == action &&
                   existing.raw_fact.relation == "response_to_action";
        });
    const float successfulSignal = outcome.reward > 0.05f ? 1.0f : 0.0f;
    if (memoryIt == memory_records.end()) {
        record memory_entry;
        memory_entry.raw_fact.subject = action;
        memory_entry.raw_fact.relation = "response_to_action";
        memory_entry.raw_fact.object = outcomeClass;
        memory_entry.confidence = successfulSignal;
        memory_entry.generalization_factor = 0.5f;
        memory_entry.current_context = record::USER_SPECIFIC;
        memory_entry.source = record::OBSERVATION;
        memory_entry.observation_count = 1;
        memory_entry.last_updated = session_context.last_timestamp;
        memory_entry.relevance = std::clamp(std::abs(outcome.reward), 0.0f, 1.0f);
        memory_entry.recency = 1.0f;
        memory_records.push_back(memory_entry);
    } else {
        const int priorCount = std::max(1, memoryIt->observation_count);
        memoryIt->confidence = std::clamp(
            (memoryIt->confidence * static_cast<float>(priorCount) + successfulSignal) /
            static_cast<float>(priorCount + 1), 0.0f, 1.0f);
        memoryIt->raw_fact.object = outcomeClass;
        memoryIt->observation_count = priorCount + 1;
        memoryIt->last_updated = session_context.last_timestamp;
        memoryIt->relevance = std::clamp(
            (memoryIt->relevance * static_cast<float>(priorCount) +
             std::abs(outcome.reward)) / static_cast<float>(priorCount + 1), 0.0f, 1.0f);
        memoryIt->recency = 1.0f;
    }

    // Update confidence in stored preferences from subsequent outcomes.
    // A preferred action should receive positive feedback; avoiding a disliked
    // action should also receive positive feedback.
    for (auto& preference : memory_records) {
        if (preference.raw_fact.subject != "user" ||
            (preference.raw_fact.relation != "prefers_action" &&
             preference.raw_fact.relation != "dislikes_action")) continue;
        const bool selectedPreferred = preference.raw_fact.relation == "prefers_action"
            ? action == preference.raw_fact.object
            : action != preference.raw_fact.object;
        const bool outcomeSuccess = selectedPreferred ? outcome.reward > 0.05f : outcome.reward < -0.05f;
        const float successSignal = outcomeSuccess ? 1.0f : 0.0f;
        const int priorCount = std::max(1, preference.observation_count);
        preference.confidence = std::clamp(
            (preference.confidence * static_cast<float>(priorCount) + successSignal) /
            static_cast<float>(priorCount + 1), 0.0f, 1.0f);
        preference.observation_count = priorCount + 1;
        preference.last_updated = session_context.last_timestamp;
        preference.recency = 1.0f;
    }

    runtime_context.session_context = session_context;
    runtime_context.affective_state = affective_state;
    runtime_context.memory_summary = buildMemorySummary(memory_records, action);
    RuntimeContext nextRuntime = runtime_context;
    nextRuntime.session_context.current_intent = nextObservation.goal_state;
    nextRuntime.session_context.target_proximity = nextObservation.target_proximity;
    nextRuntime.session_context.user_status = nextObservation.user_status;

    state next_state = makeRuntimeState(nextRuntime);
    next_state.affect_trend = summarizeAffectTrend(affective_state, levelFromValue(session_context.energy_level));
    next_state.task_status = nextObservation.task_status;
    next_state.signal_strength_bucket =
        nextObservation.signal_strength < 0.33f ? 0 : (nextObservation.signal_strength < 0.67f ? 1 : 2);
    next_state.food_available = nextObservation.food_available;
    next_state.play_available = nextObservation.play_available;
    if (learning_enabled) {
        q_policy.update(current_state, action, outcome.reward, next_state, outcome.terminal);
    }

    InteractionEvent event;
    event.timestamp = session_context.last_timestamp;
    event.action_taken = action;
    event.summary = outcome.outcome_tag.empty() ? action : outcome.outcome_tag;
    event.reward = outcome.reward;
    event.state_summary = "proposed_memory_aware_observation";
    event.affect_signal = std::clamp(affective_state.affinity + affective_state.arousal, 0.0f, 1.0f);
    interaction_history.push_back(event);

    session_context.current_action = action;
    current_state = next_state;
}

void ProposedAgent::loadPersistentState(const std::string& path) {
    q_policy.loadQTable(path);
    const std::string prefix = path + ".";
    if (std::filesystem::exists(prefix + "memory.json"))
        memory_records = loadMemoryRecords(prefix + "memory.json");
    if (std::filesystem::exists(prefix + "affect.json"))
        affective_state = loadAffectiveState(prefix + "affect.json");
}

void ProposedAgent::savePersistentState(const std::string& path) {
    q_policy.saveQTable(path);
    savePersistentContext(path);
}

void ProposedAgent::savePersistentContext(const std::string& path) {
    const std::string prefix = path + ".";
    bool memorySaved = true;
    if (memory_records.empty()) {
        std::error_code ec;
        std::filesystem::remove(prefix + "memory.json", ec);
        memorySaved = !ec;
    } else {
        memorySaved = saveMemoryRecords(prefix + "memory.json", memory_records);
    }
    const bool affectSaved = saveAffectiveState(prefix + "affect.json", affective_state);
    if (!memorySaved || !affectSaved)
        throw std::runtime_error("Unable to persist proposed-agent memory/affect sidecar files");
}

const state& ProposedAgent::getCurrentState() const {
    return current_state;
}

const SessionContext& ProposedAgent::getSessionContext() const {
    return session_context;
}

const std::vector<InteractionEvent>& ProposedAgent::getInteractionHistory() const {
    return interaction_history;
}

const std::vector<record>& ProposedAgent::getMemoryRecords() const {
    return memory_records;
}

const AffectiveState& ProposedAgent::getAffectiveState() const {
    return affective_state;
}

void ProposedAgent::clearMemory() {
    memory_records.clear();
}

void ProposedAgent::recordUserPreference(const std::string& action, bool likes) {
    const std::string relation = likes ? "prefers_action" : "dislikes_action";
    auto existing = std::find_if(memory_records.begin(), memory_records.end(),
        [&](const record& item) {
            return item.raw_fact.subject == "user" &&
                   item.raw_fact.relation == relation &&
                   item.raw_fact.object == action;
        });
    if (existing != memory_records.end()) {
        existing->confidence = std::clamp(existing->confidence + 0.15f, 0.0f, 1.0f);
        existing->observation_count += 1;
        existing->last_updated = currentTimeStamp();
        existing->recency = 1.0f;
        existing->source = record::USER_STATEMENT;
        return;
    }
    record preference;
    preference.raw_fact.subject = "user";
    preference.raw_fact.relation = relation;
    preference.raw_fact.object = action;
    preference.confidence = 0.75f; // Explicit statement; outcome evidence adjusts this later.
    preference.generalization_factor = 0.5f;
    preference.current_context = record::USER_SPECIFIC;
    preference.source = record::USER_STATEMENT;
    preference.observation_count = 1;
    preference.last_updated = currentTimeStamp();
    preference.relevance = 1.0f;
    preference.recency = 1.0f;
    memory_records.push_back(preference);
}


bool testProposedMemoryConfidenceTracksOutcome() {
    EnvironmentConfig config;
    ProposedAgent agent(config);
    agent.setLearningEnabled(false);
    agent.setExplorationRate(0.0f);
    EnvironmentObservation observation{0, 1, 0, 0, 0.55f};
    ActionOutcome positive;
    positive.reward = 1.0f;
    positive.terminal = true;
    positive.outcome_tag = "positive";
    agent.applyOutcome(observation, "GREETING", positive);
    ActionOutcome negative;
    negative.reward = -1.0f;
    negative.terminal = true;
    negative.outcome_tag = "negative";
    agent.applyOutcome(observation, "GREETING", negative);
    const auto& records = agent.getMemoryRecords();
    return records.size() == 1 &&
           records.front().raw_fact.object == "negative" &&
           nearlyEqual(records.front().confidence, 0.5, 1e-5) &&
           records.front().observation_count == 2;
}

bool testProposedMemoryPersistsAcrossResetAndDisk() {
    const auto dir = std::filesystem::temp_directory_path() / "is2_proposed_persistence_test";
    std::filesystem::create_directories(dir);
    const auto policyPath = (dir / "proposed.qtable").string();
    EnvironmentConfig config;
    ProposedAgent original(config);
    original.setLearningEnabled(false);
    ActionOutcome outcome;
    outcome.reward = 1.0f;
    outcome.terminal = true;
    outcome.outcome_tag = "positive";
    original.applyOutcome(EnvironmentObservation{0, 1, 0, 0, 0.55f}, "GREETING", outcome);
    original.reset();
    if (original.getMemoryRecords().empty()) return false;
    original.savePersistentState(policyPath);
    ProposedAgent restored(config);
    restored.loadPersistentState(policyPath);
    const bool loaded = restored.getMemoryRecords().size() == original.getMemoryRecords().size() &&
        restored.getAffectiveState().affinity == original.getAffectiveState().affinity;
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    return loaded;
}

bool testOutcomeUsesExplicitNextObservation() {
    EnvironmentConfig config;
    BaselineAgent baseline(config);
    ProposedAgent proposed(config);
    baseline.setLearningEnabled(false);
    proposed.setLearningEnabled(false);
    const EnvironmentObservation current{0, 1, 0, 0, 0.45f};
    const EnvironmentObservation next{1, 0, 1, 1, 0.90f, true, true};
    const std::string baselineAction = baseline.selectAction(current);
    const std::string proposedAction = proposed.selectAction(current);
    ActionOutcome outcome;
    outcome.reward = 0.5f;
    outcome.terminal = false;
    outcome.outcome_tag = "transition_test";
    baseline.applyOutcome(current, baselineAction, outcome, next);
    proposed.applyOutcome(current, proposedAction, outcome, next);
    const auto& b = baseline.getCurrentState();
    const auto& p = proposed.getCurrentState();
    return b.encoded_user_input == static_cast<float>(next.goal_state) &&
           p.encoded_user_input == static_cast<float>(next.goal_state) &&
           b.task_status == next.task_status && p.task_status == next.task_status &&
           b.signal_strength_bucket == 2 && p.signal_strength_bucket == 2 &&
           b.food_available && b.play_available && p.food_available && p.play_available;
}

bool testNegativeFeedbackUpdatesAffectDimensions() {
    AffectiveState affect;
    const float arousalBefore = affect.arousal;
    const float stressBefore = affect.stress;
    const float affinityBefore = affect.affinity;
    affect.updateFromFeedback(-1.0f, false, true);
    return affect.valence == AffectValence::NEGATIVE &&
           affect.arousal < arousalBefore &&
           affect.stress > stressBefore &&
           affect.affinity < affinityBefore &&
           affect.energy == AffectEnergy::LOW;
}

bool runBaselineValidation() {
    std::vector<TestResultRecord> results;
    results.push_back({"Basic Q update", "PASS", testBasicQUpdate(), "baseline"});
    results.push_back({"Unseen state handling", "PASS", testUnseenStateHandling(), "baseline"});
    results.push_back({"Intent-appropriate fallback actions", "PASS", testContextualFallbackMatchesIntent(), "policy"});
    results.push_back({"Task and signal state encoding", "PASS", testTaskAndSignalAreEncoded(), "state"});
    results.push_back({"Proposed agent adapts after negative feedback", "PASS", testProposedAgentAdaptsAfterNegativeFeedback(), "affect"});
    results.push_back({"Proposed agent avoids rejected action", "PASS", testProposedAgentAvoidsRejectedAction(), "memory"});
    results.push_back({"Memory retention after intervening interactions", "PASS", testMemoryRetentionAfterInterveningInteractions(), "memory"});
    results.push_back({"Semantic preference survives intervening interactions", "PASS", testSemanticPreferenceSurvivesInterveningInteractions(), "memory"});
    results.push_back({"Memory confidence follows outcome history", "PASS", testProposedMemoryConfidenceTracksOutcome(), "memory"});
    results.push_back({"Proposed memory and affect persist across reset and disk", "PASS", testProposedMemoryPersistsAcrossResetAndDisk(), "persistence"});
    results.push_back({"Negative feedback updates all affect dimensions", "PASS", testNegativeFeedbackUpdatesAffectDimensions(), "affect"});
    results.push_back({"Outcome uses explicit next observation", "PASS", testOutcomeUsesExplicitNextObservation(), "training"});
    results.push_back({"Persistence", "PASS", testPersistence(), "baseline"});
    results.push_back({"Terminal handling", "PASS", testTerminalHandling(), "baseline"});
    results.push_back({"Observable action masking and food/play availability", "PASS", testActionMaskUsesOnlyObservableAvailability(), "policy"});
    results.push_back({"Seeded exploration reproducibility", "PASS", testSeededExplorationIsReproducible(), "training"});
    results.push_back({"Baseline excludes memory and affect inputs", "PASS", testBaselineExcludesMemoryAndAffectInputs(), "training"});
    results.push_back({"Evaluation does not update policy", "PASS", testEvaluationDoesNotUpdatePolicy(), "training"});
    results.push_back({"Memory/affect encoding", "PASS", testMemoryAffectStateEncoding(), "state"});
    results.push_back({"Context summary state changes", "PASS", testContextSummaryChangesState(), "state"});
    results.push_back({"Live memory summary state changes", "PASS", testLiveMemorySummariesChangeState(), "memory"});
    results.push_back({"Persistence round trip and aggregation", "PASS", testPersistenceRoundTripAndAggregation(), "persistence"});
    results.push_back({"Runtime lifecycle and environment contract", "PASS", testRuntimeLifecycleAndEnvironmentContract(), "runtime"});

    bool allPassed = true;
    for (auto& result : results) {
        if (!result.passed) {
            result.status = "FAIL";
            allPassed = false;
        }
        std::cout << result.name << ": " << (result.passed ? "PASS" : "FAIL") << std::endl;
    }

    writeValidationReport(results, "Docs/test-results.json");
    return allPassed;
}
