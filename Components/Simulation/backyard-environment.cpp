#include "backyard-environment.h"

void BackyardEnvironment::beginInteraction() {
    ++world.interactions;
}

void BackyardEnvironment::applyStimulus(const std::string& stimulus) {
    resetTransientStimuli();
    if (stimulus.find("noise") != std::string::npos ||
        stimulus.find("Noise") != std::string::npos ||
        stimulus.find("disturbance") != std::string::npos) {
        world.disturbanceActive = true;
    }
    if (stimulus.find("Fetch") != std::string::npos ||
        stimulus.find("fetch") != std::string::npos ||
        stimulus.find("ball") != std::string::npos) {
        world.ballAvailable = true;
        world.dogHasBall = false;
    }
    if (stimulus.find("Rest") != std::string::npos ||
        stimulus.find("rest") != std::string::npos ||
        stimulus.find("sleep") != std::string::npos) {
        world.dogResting = false;
    }
    if (stimulus.find("food") != std::string::npos ||
        stimulus.find("feed") != std::string::npos) {
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
