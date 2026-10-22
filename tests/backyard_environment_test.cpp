#include "Components/Simulation/backyard-environment.h"

#include <cassert>

int main() {
    BackyardEnvironment environment;

    assert(environment.state().ballAvailable);
    assert(environment.state().foodAvailable);
    assert(environment.state().completedFetches == 0);

    environment.beginInteraction();
    environment.applyStimulus("Ask the companion to fetch the ball");
    environment.applyAction("FETCH");
    assert(!environment.state().ballAvailable);
    assert(environment.state().dogHasBall);
    assert(environment.state().completedFetches == 1);
    assert(environment.state().interactions == 1);

    environment.beginInteraction();
    environment.applyStimulus("The companion should rest");
    environment.applyAction("SLEEP_LAY");
    assert(environment.state().dogResting);
    assert(!environment.state().ballAvailable);
    assert(environment.state().dogHasBall);

    environment.beginInteraction();
    environment.applyStimulus("A sudden loud noise occurs");
    assert(environment.state().disturbanceActive);
    environment.applyAction("BARK");
    assert(environment.state().disturbanceActive);

    environment.beginInteraction();
    environment.applyStimulus("Praise the companion");
    assert(!environment.state().disturbanceActive);

    environment.beginInteraction();
    environment.applyStimulus("Please feed the companion");
    environment.applyAction("EAT");
    assert(!environment.state().foodAvailable);

    environment.applyAction("RETURN_BALL");
    assert(environment.state().ballAvailable);
    assert(!environment.state().dogHasBall);
}
