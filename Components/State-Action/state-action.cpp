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
    table.update(s.encode(), "ACT_CONFUSED", -1.0, terminal.encode());
    const double value = table.getQValue(s.encode(), "ACT_CONFUSED");
    return std::isfinite(value);
}

} // namespace

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

void QTable::update(std::size_t stateKey, std::string action, double reward, std::size_t nextState) {
    const double oldValue = getQValue(stateKey, action);
    double bestNextValue = 0.0;

    const auto nextIt = table.find(nextState);
    if (nextIt != table.end()) {
        for (const auto& entry : nextIt->second) {
            if (entry.second > bestNextValue) {
                bestNextValue = entry.second;
            }
        }
    }

    const double newValue = oldValue + learningRate * (reward + discountFactor * bestNextValue - oldValue);
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

QLearning::QLearning(float learningRate, float discountFactor, float epsilon)
    : q_table(learningRate, discountFactor), learning_rate(learningRate), discount_factor(discountFactor), epsilon(epsilon) {}

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
    static std::mt19937 rng(static_cast<unsigned int>(std::time(nullptr)));
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
        static std::mt19937 rng(static_cast<unsigned int>(std::time(nullptr)));
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        if (dist(rng) < epsilon) {
            return getRandomAction(stateRef);
        }
    }

    return getBestAction(stateRef);
}

void QLearning::update(const state& current_state, const std::string& action, float reward, const state& next_state) {
    const double currentValue = q_table.getQValue(current_state.encode(), action);
    double bestNextValue = 0.0;
    for (const auto& candidate : actions) {
        const double candidateValue = q_table.getQValue(next_state.encode(), candidate);
        if (candidateValue > bestNextValue) {
            bestNextValue = candidateValue;
        }
    }

    const double updatedValue = currentValue + learning_rate * (reward + discount_factor * bestNextValue - currentValue);
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

bool testMemoryAffectStateEncoding() {
    const state neutral = makeState(1, distance::CLOSE, Level::LOW, Level::LOW, Level::MEDIUM, Level::LOW, 0, 0.0f);
    state positive = neutral;
    positive.affect = AffectiveState{AffectValence::POSITIVE, AffectEnergy::HIGH, 0.8f, 0.9f, 0.2f, 0.7f};
    state negative = neutral;
    negative.affect = AffectiveState{AffectValence::NEGATIVE, AffectEnergy::LOW, 0.2f, 0.1f, 0.8f, 0.1f};

    return neutral.encode() != positive.encode() && neutral.encode() != negative.encode() && positive.encode() != negative.encode();
}

bool runBaselineValidation() {
    const std::vector<std::pair<std::string, bool>> tests = {
        {"Basic Q update", testBasicQUpdate()},
        {"Unseen state handling", testUnseenStateHandling()},
        {"Persistence", testPersistence()},
        {"Terminal handling", testTerminalHandling()},
        {"Memory/affect encoding", testMemoryAffectStateEncoding()},
        {"Context summary state changes", testContextSummaryChangesState()},
        {"Live memory summary state changes", testLiveMemorySummariesChangeState()},
        {"Persistence round trip and aggregation", testPersistenceRoundTripAndAggregation()}
    };

    bool allPassed = true;
    for (const auto& [name, passed] : tests) {
        std::cout << name << ": " << (passed ? "PASS" : "FAIL") << std::endl;
        if (!passed) {
            allPassed = false;
        }
    }

    return allPassed;
}
