#pragma once

#include <string>

struct BackyardWorldState {
    bool ballAvailable = true;
    bool foodAvailable = true;
    bool disturbanceActive = false;
    bool dogHasBall = false;
    bool dogResting = false;
    unsigned int completedFetches = 0;
    unsigned int interactions = 0;
};

// Small deterministic world model, deliberately separate from SFML drawing.
// It models visible consequences of actions; it is not a physics engine.
class BackyardEnvironment {
public:
    const BackyardWorldState& state() const noexcept { return world; }
    void beginInteraction();
    void applyAction(const std::string& action);
    void applyStimulus(const std::string& stimulus);
    void resetTransientStimuli();

private:
    BackyardWorldState world;
};
