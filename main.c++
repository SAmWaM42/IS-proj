#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "Components/State-Action/state-action.h"

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

    const std::size_t encoded =
        (((((intent * 4 + proximity) * 4 + userStatus) * 4 + energyIdx) * 4 + stressIdx) * 4 + curiosityIdx) * 4 + attachmentIdx;

    return encoded + static_cast<std::size_t>(std::round(memory));
}

state makeState(int intent,
                distance proximity,
                Level energy,
                Level stress,
                Level curiosity,
                Level attachment,
                int userStatus = 0,
                float memory = 0.0f) {
    state s;
    s.energy = energy;
    s.stress = stress;
    s.curiosity = curiosity;
    s.attachment = attachment;
    s.memory = memory;
    s.encoded_user_input = static_cast<float>(intent);
    s.environmentCondition["target_proximity"] = proximity;
    s.environmentCondition["user_status"] = static_cast<distance>(userStatus);
    return s;
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

bool runAllTests() {
    const std::vector<std::pair<std::string, bool>> tests = {
        {"Basic Q update", testBasicQUpdate()},
        {"Unseen state handling", testUnseenStateHandling()},
        {"Persistence", testPersistence()},
        {"Terminal handling", testTerminalHandling()}
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

} // namespace

std::size_t state::encode() const {
    return mixEncode(
        environmentCondition,
        energy,
        stress,
        curiosity,
        attachment,
        encoded_user_input,
        memory);
}

QTable::QTable(double learningRate, double discountFactor)
    : learningRate(learningRate), discountFactor(discountFactor) {}

double QTable::getQValue(std::size_t state, std::string action) const {
    const auto stateIt = table.find(state);
    if (stateIt == table.end()) {
        return 0.0;
    }

    const auto actionIt = stateIt->second.find(action);
    if (actionIt == stateIt->second.end()) {
        return 0.0;
    }

    return actionIt->second;
}

void QTable::update(std::size_t state, std::string action, double reward, std::size_t nextState) {
    const double oldValue = getQValue(state, action);
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
    table[state][action] = newValue;
}

std::string QTable::chooseAction(std::size_t state) {
    const auto stateIt = table.find(state);
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

    for (const auto& [state, actionMap] : table) {
        for (const auto& [action, value] : actionMap) {
            out << state << " " << action << " " << value << '\n';
        }
    }
}

void QTable::load(const std::string& filename) {
    std::ifstream in(filename);
    if (!in.is_open()) {
        return;
    }

    table.clear();
    std::size_t state = 0;
    std::string action;
    double value = 0.0;
    while (in >> state >> action >> value) {
        table[state][action] = value;
    }
}

int main() {
    if (!runAllTests()) {
        return 1;
    }

    std::cout << "Baseline learning loop validation passed." << std::endl;
    return 0;
}
