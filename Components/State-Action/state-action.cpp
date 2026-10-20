#include "state-action.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
const std::vector<std::string> kActions = {
    "FETCH",
    "WANDER",
    "BARK",
    "ACT_CONFUSED",
    "SLEEP_LAY",
    "NUZZLE",
    "INSIST"
};

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
    QTable table(0.1, 0.9);
    const state unseen = makeState(3, distance::FAR, Level::HIGH, Level::MEDIUM, Level::LOW, Level::MEDIUM, 2, 0.0f);
    const std::string action = table.chooseAction(unseen.encode());
    return std::find(kActions.begin(), kActions.end(), action) != kActions.end();
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

    SimulationTick tick{observation, actionResult, "2026-10-11T00:00:00Z"};

    return stateChanged && normalized.memory_bias > 0.0f && normalized.affect_trend > 0.0f &&
           observation.target_proximity == 1 && tick.action_result.reward > 0.0f;
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
    return mixEncode(
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

std::string QTable::chooseAction(std::size_t stateKey) {
    const auto stateIt = table.find(stateKey);
    if (stateIt == table.end() || stateIt->second.empty()) {
        return kActions.front();
    }

    std::pair<std::string, double> bestEntry{"", -std::numeric_limits<double>::infinity()};
    for (const auto& entry : stateIt->second) {
        if (entry.second > bestEntry.second) {
            bestEntry = entry;
        }
    }

    return bestEntry.first;
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

float QLearning::getQValue(const state& stateRef, const std::string& action) const {
    return static_cast<float>(q_table.getQValue(stateRef.encode(), action));
}

void QLearning::setQValue(const state& stateRef, const std::string& action, float value) {
    q_table.table[stateRef.encode()][action] = value;
}

float QLearning::getMaxQValue(const state& stateRef) const {
    float best = -std::numeric_limits<float>::infinity();
    for (const auto& action : actions) {
        const float value = getQValue(stateRef, action);
        if (value > best) {
            best = value;
        }
    }
    return best;
}

std::string QLearning::getBestAction(const state& stateRef) const {
    std::string bestAction = actions.empty() ? "FETCH" : actions.front();
    float bestValue = -std::numeric_limits<float>::infinity();
    for (const auto& action : actions) {
        const float value = getQValue(stateRef, action);
        if (value > bestValue) {
            bestValue = value;
            bestAction = action;
        }
    }
    return bestAction;
}

std::string QLearning::getRandomAction(const state& stateRef) const {
    (void)stateRef;
    if (actions.empty()) {
        return "FETCH";
    }
    std::uniform_int_distribution<std::size_t> dist(0, actions.size() - 1);
    return actions[dist(rng)];
}

void QLearning::setActions(const std::vector<std::string>& newActions) {
    actions = newActions;
}

std::string QLearning::chooseAction(const state& stateRef) const {
    if (actions.empty()) {
        return "FETCH";
    }

    if (epsilon > 0.0f) {
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        if (dist(rng) < epsilon) {
            return getRandomAction(stateRef);
        }
    }

    return getBestAction(stateRef);
}

void QLearning::update(const state& current_state, const std::string& action, float reward, const state& next_state, bool terminal) {
    const double currentValue = q_table.getQValue(current_state.encode(), action);
    double bestNextValue = 0.0;
    if (!terminal) {
        for (const auto& candidate : actions) {
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

float QLearning::getEpsilon() const {
    return epsilon;
}

void QLearning::loadQTable(const std::string& path) {
    q_table.load(path);
}

void QLearning::saveQTable(const std::string& path) {
    q_table.save(path);
}

void QLearning::loadTrainingData(const std::string&) {
    // intentionally left as no-op for the minimal prototype
}

bool testSeededExplorationIsReproducible() {
    const state sample = makeState(1, distance::CLOSE, Level::MEDIUM, Level::LOW, Level::MEDIUM, Level::LOW, 1, 0.0f);
    QLearning first(0.25f, 0.8f, 1.0f, 12345u);
    QLearning second(0.25f, 0.8f, 1.0f, 12345u);
    const std::vector<std::string> actions = {"FETCH", "WANDER", "BARK", "INSIST"};
    first.setActions(actions);
    second.setActions(actions);
    for (int i = 0; i < 100; ++i) {
        if (first.chooseAction(sample) != second.chooseAction(sample)) {
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
float scoreActionAgainstObservation(const std::string& action, const EnvironmentObservation& observation) {
    float score = 0.0f;
    const std::string lowered = action;
    if (action == "FETCH") {
        score += observation.task_status == 1 ? 0.6f : 0.2f;
        score += observation.signal_strength * 0.5f;
    } else if (action == "GREETING") {
        score += observation.user_status == 1 ? 0.5f : 0.1f;
    } else if (action == "ACT_CONFUSED") {
        score += observation.signal_strength > 0.75f ? 0.2f : -0.2f;
    } else if (action == "SLEEP_LAY") {
        score += observation.task_status == 0 ? 0.5f : -0.1f;
    } else if (action == "BARK") {
        score += observation.signal_strength > 0.7f ? 0.1f : -0.2f;
    } else if (action == "INSIST") {
        score += observation.user_status == 1 ? 0.2f : -0.1f;
    }
    return std::clamp(score, -1.0f, 1.0f);
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
        levelFromValue(session_context.energy_level),
        levelFromValue(session_context.stress_level),
        Level::MEDIUM,
        Level::LOW,
        observation.user_status,
        0.0f);
    current_state = candidate;
    session_context.current_action = q_policy.chooseAction(candidate);
    return session_context.current_action;
}

void BaselineAgent::applyOutcome(const EnvironmentObservation& observation,
                                const std::string& action,
                                const ActionOutcome& outcome) {
    state next_state = makeState(
        session_context.current_intent,
        proximityFromValue(observation.target_proximity),
        levelFromValue(std::max(0, session_context.energy_level + (outcome.reward > 0.0f ? 1 : 0))),
        levelFromValue(std::max(0, session_context.stress_level + (outcome.reward < 0.0f ? 1 : 0))),
        Level::MEDIUM,
        Level::LOW,
        observation.user_status,
        0.0f);

    if (learning_enabled) {
        q_policy.update(current_state, action, outcome.reward, next_state, outcome.terminal);
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
    session_context.last_timestamp = currentTimeStamp();
    affective_state = AffectiveState{};
    memory_records.clear();
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

    session_context.current_intent = observation.goal_state;
    session_context.target_proximity = observation.target_proximity;
    session_context.user_status = observation.user_status;
    session_context.current_action = q_policy.chooseAction(current_state);
    return session_context.current_action;
}

void ProposedAgent::applyOutcome(const EnvironmentObservation& observation,
                                const std::string& action,
                                const ActionOutcome& outcome) {
    session_context.current_intent = observation.goal_state;
    session_context.target_proximity = observation.target_proximity;
    session_context.user_status = observation.user_status;
    session_context.last_reward = outcome.reward;
    session_context.last_timestamp = currentTimeStamp();
    affective_state.updateFromFeedback(outcome.reward, outcome.reward > 0.0f, outcome.reward < 0.0f);

    record memory_entry;
    memory_entry.raw_fact.subject = "user";
    memory_entry.raw_fact.relation = "interaction";
    memory_entry.raw_fact.object = outcome.outcome_tag.empty() ? action : outcome.outcome_tag;
    memory_entry.confidence = std::clamp(observation.signal_strength, 0.0f, 1.0f);
    memory_entry.generalization_factor = 0.5f;
    memory_entry.current_context = record::USER_SPECIFIC;
    memory_entry.source = record::OBSERVATION;
    memory_entry.observation_count = 1;
    memory_entry.last_updated = session_context.last_timestamp;
    memory_entry.relevance = std::clamp(std::abs(outcome.reward) + 0.25f, 0.0f, 1.0f);
    memory_entry.recency = std::clamp(observation.signal_strength, 0.0f, 1.0f);
    memory_records.push_back(memory_entry);

    runtime_context.session_context = session_context;
    runtime_context.affective_state = affective_state;
    runtime_context.memory_summary = buildMemorySummary(memory_records, action);

    state next_state = makeRuntimeState(runtime_context);
    next_state.affect_trend = summarizeAffectTrend(affective_state, levelFromValue(session_context.energy_level));
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
}

void ProposedAgent::savePersistentState(const std::string& path) {
    q_policy.saveQTable(path);
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

AgentComparisonRun runAgentComparison(const EnvironmentConfig& config, int steps) {
    AgentComparisonRun run{config, BaselineAgent(config), ProposedAgent(config), {}};

    for (int stepIndex = 0; stepIndex < steps; ++stepIndex) {
        EnvironmentObservation observation;
        observation.goal_state = stepIndex % 3;
        observation.target_proximity = (stepIndex + 1) % 3;
        observation.user_status = stepIndex % 2;
        observation.task_status = (stepIndex + 1) % 2;
        observation.signal_strength = 0.35f + 0.12f * static_cast<float>((stepIndex % 5));

        const std::string baselineAction = run.baseline_agent.selectAction(observation);
        const std::string proposedAction = run.proposed_agent.selectAction(observation);

        const float baselineReward = scoreActionAgainstObservation(baselineAction, observation);
        const float proposedReward = scoreActionAgainstObservation(proposedAction, observation);

        ActionOutcome baselineOutcome;
        baselineOutcome.action_id = stepIndex;
        baselineOutcome.reward = baselineReward;
        baselineOutcome.terminal = (stepIndex + 1 == steps);
        baselineOutcome.outcome_tag = "baseline_" + baselineAction;

        ActionOutcome proposedOutcome;
        proposedOutcome.action_id = stepIndex;
        proposedOutcome.reward = proposedReward;
        proposedOutcome.terminal = (stepIndex + 1 == steps);
        proposedOutcome.outcome_tag = "proposed_" + proposedAction;

        run.baseline_agent.applyOutcome(observation, baselineAction, baselineOutcome);
        run.proposed_agent.applyOutcome(observation, proposedAction, proposedOutcome);

        SimulationTick tick;
        tick.observation = observation;
        tick.action_result = baselineOutcome;
        tick.timestamp = currentTimeStamp();
        run.tick_log.push_back(tick);
    }

    return run;
}

bool runBaselineValidation() {
    std::vector<TestResultRecord> results;
    results.push_back({"Basic Q update", "PASS", testBasicQUpdate(), "baseline"});
    results.push_back({"Unseen state handling", "PASS", testUnseenStateHandling(), "baseline"});
    results.push_back({"Persistence", "PASS", testPersistence(), "baseline"});
    results.push_back({"Terminal handling", "PASS", testTerminalHandling(), "baseline"});
    results.push_back({"Seeded exploration reproducibility", "PASS", testSeededExplorationIsReproducible(), "training"});
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
