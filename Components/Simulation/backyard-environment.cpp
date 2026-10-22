#include "backyard-environment.h"

#include <algorithm>
#include <cctype>

void BackyardEnvironment::beginInteraction() {
    ++world.interactions;
}

void BackyardEnvironment::reset() {
    world = BackyardWorldState{};
}

void BackyardEnvironment::applyStimulus(const std::string& stimulus) {
    std::string lowered = stimulus;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    resetTransientStimuli();
    if (lowered.find("noise") != std::string::npos ||
        lowered.find("disturbance") != std::string::npos) {
        world.disturbanceActive = true;
    }

    // Mentioning fetch/ball is not itself a new ball spawn. Only an explicit
    // throw/offer replenishes a ball after the dog has collected it.
    const bool ballOffered = lowered.find("throw") != std::string::npos ||
        lowered.find("offer") != std::string::npos ||
        lowered.find("put down") != std::string::npos;
    if (ballOffered) {
        world.ballAvailable = true;
        world.dogHasBall = false;
    }

    if (lowered.find("rest") != std::string::npos ||
        lowered.find("sleep") != std::string::npos) {
        world.dogResting = false;
    }
    if (lowered.find("food") != std::string::npos ||
        lowered.find("feed") != std::string::npos) {
        world.foodAvailable = true;
    }
}

void BackyardEnvironment::applyAction(const std::string& action) {
    world.dogResting = false;
    if (action == "FETCH" && world.ballAvailable) {
        world.dogHasBall = true;
        world.ballAvailable = false;
        ++world.completedFetches;
    } else if (action == "SLEEP_LAY") {
        world.dogResting = true;
    } else if (action == "EAT" && world.foodAvailable) {
        world.foodAvailable = false;
    } else if (action == "BARK" || action == "WHINE") {
        // The visible disturbance remains active until the next interaction.
    } else if (action == "DROP" || action == "RETURN_BALL") {
        world.dogHasBall = false;
        world.ballAvailable = true;
    }
}

void BackyardEnvironment::resetTransientStimuli() {
    world.disturbanceActive = false;
}
