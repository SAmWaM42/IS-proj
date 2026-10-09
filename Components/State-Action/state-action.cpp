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

std::size_t mixEncode(const std::map<std::string, distance>& environmentCondition,
                      Level energy,
                      Level stress,
                      Level curiosity,
                      Level attachment,
                      const AffectiveState& affect,
                      float encoded_user_input,
                      float memory) {
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

    const std::size_t encoded =
        (((((((((intent * 4 + proximity) * 4 + userStatus) * 4 + energyIdx) * 4 + stressIdx) * 4 + curiosityIdx) * 4 + attachmentIdx)
            * 3 + valenceIdx) * 3 + affectEnergyIdx) * 3 + arousalIdx) * 3 + engagementIdx;

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
    state s;
    s.energy = energy;
    s.stress = stress;
    s.curiosity = curiosity;
    s.attachment = attachment;
    s.affect = AffectiveState{};
    s.memory = memory;
    s.encoded_user_input = static_cast<float>(intent);
    s.environmentCondition["target_proximity"] = proximity;
    s.environmentCondition["user_status"] = static_cast<distance>(userStatus);
    return s;
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
        memory);
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
        {"Memory/affect encoding", testMemoryAffectStateEncoding()}
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
