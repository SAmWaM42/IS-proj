#include "Components/Simulation/backyard-environment.h"

#include <cassert>
#include <iostream>

namespace {
void fetchScenario() {
    BackyardEnvironment environment;
    environment.beginInteraction();
    environment.applyStimulus("Ask the companion to fetch the ball");
    environment.applyAction("FETCH");
    assert(!environment.state().ballAvailable);
    assert(environment.state().dogHasBall);
    assert(environment.state().completedFetches == 1);

    // A repeated request does not magically respawn the collected ball.
    environment.beginInteraction();
    environment.applyStimulus("Please fetch the ball again");
    environment.applyAction("FETCH");
    assert(!environment.state().ballAvailable);
    assert(environment.state().dogHasBall);
    assert(environment.state().completedFetches == 1);

    // An explicit throw represents a new ball offer.
    environment.beginInteraction();
    environment.applyStimulus("I throw the ball");
    environment.applyAction("FETCH");
    assert(!environment.state().ballAvailable);
    assert(environment.state().dogHasBall);
    assert(environment.state().completedFetches == 2);
    std::cout << "[PASS] fetch: collection, no implicit respawn, explicit re-offer\n";
}

void disturbanceScenario() {
    BackyardEnvironment environment;
    environment.beginInteraction();
    environment.applyStimulus("A sudden LOUD NOISE occurs");
    assert(environment.state().disturbanceActive);
    environment.applyAction("BARK");
    assert(environment.state().disturbanceActive);

    environment.beginInteraction();
    environment.applyStimulus("Praise the companion");
    assert(!environment.state().disturbanceActive);
    std::cout << "[PASS] noise: case-insensitive detection and transient reset\n";
}

void restScenario() {
    BackyardEnvironment environment;
    environment.beginInteraction();
    environment.applyStimulus("The companion should rest");
    environment.applyAction("SLEEP_LAY");
    assert(environment.state().dogResting);

    environment.beginInteraction();
    environment.applyStimulus("The companion should sleep");
    assert(!environment.state().dogResting);
    environment.applyAction("SLEEP_LAY");
    assert(environment.state().dogResting);
    std::cout << "[PASS] rest: rest stimulus and action-driven resting state\n";
}

void foodScenario() {
    BackyardEnvironment environment;
    environment.beginInteraction();
    environment.applyStimulus("Please feed the companion");
    environment.applyAction("EAT");
    assert(!environment.state().foodAvailable);
    std::cout << "[PASS] food: feeding consumes the offered food\n";
}
}

int main() {
    fetchScenario();
    disturbanceScenario();
    restScenario();
    foodScenario();
}
