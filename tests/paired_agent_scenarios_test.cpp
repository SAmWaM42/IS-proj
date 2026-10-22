#include "Components/Simulation/backyard-environment.h"
#include "Components/State-Action/state-action.h"
#include "Components/State-Action/action-space.h"

#include <cassert>
#include <iostream>

namespace {
EnvironmentObservation fetchObservation() {
    EnvironmentObservation observation;
    observation.goal_state = 1;
    observation.user_status = 1;
    observation.target_proximity = 1;
    observation.task_status = 1;
    observation.signal_strength = 0.90f;
    return observation;
}

EnvironmentObservation noiseObservation() {
    EnvironmentObservation observation;
    observation.goal_state = 2;
    observation.user_status = 0;
    observation.target_proximity = 0;
    observation.task_status = 0;
    observation.signal_strength = 0.95f;
    return observation;
}

EnvironmentObservation restObservation() {
    EnvironmentObservation observation;
    observation.goal_state = 2;
    observation.user_status = 0;
    observation.target_proximity = 0;
    observation.task_status = 0;
    observation.signal_strength = 0.20f;
    return observation;
}

void pairedFetchScenario() {
    EnvironmentConfig config;
    config.seed = 101;
    BaselineAgent baseline(config);
    ProposedAgent proposed(config);
    baseline.setExplorationRate(0.0f);
    proposed.setExplorationRate(0.0f);
    baseline.setLearningEnabled(false);
    proposed.setLearningEnabled(false);

    const EnvironmentObservation observation = fetchObservation();
    const std::string baselineAction = baseline.selectAction(observation);
    const std::string proposedAction = proposed.selectAction(observation);
    assert(isActionValidForObservation(baselineAction, observation));
    assert(isActionValidForObservation(proposedAction, observation));
    assert(baselineAction == "FETCH");
    assert(proposedAction == "FETCH");

    BackyardEnvironment baselineWorld;
    BackyardEnvironment proposedWorld;
    baselineWorld.applyStimulus("Please fetch the ball");
    proposedWorld.applyStimulus("Please fetch the ball");
    baselineWorld.beginInteraction();
    proposedWorld.beginInteraction();
    baselineWorld.applyAction(baselineAction);
    proposedWorld.applyAction(proposedAction);
    assert(!baselineWorld.state().ballAvailable && baselineWorld.state().dogHasBall);
    assert(!proposedWorld.state().ballAvailable && proposedWorld.state().dogHasBall);
    assert(baselineWorld.state().completedFetches == proposedWorld.state().completedFetches);
    std::cout << "[PASS] paired fetch: same observation, valid matching actions, matching world outcomes\n";
}

void pairedNoiseAndFeedbackScenario() {
    EnvironmentConfig config;
    config.seed = 102;
    BaselineAgent baseline(config);
    ProposedAgent proposed(config);
    baseline.setExplorationRate(0.0f);
    proposed.setExplorationRate(0.0f);
    baseline.setLearningEnabled(false);
    proposed.setLearningEnabled(false);

    const EnvironmentObservation observation = noiseObservation();
    const std::string baselineAction = baseline.selectAction(observation);
    const std::string proposedFirstAction = proposed.selectAction(observation);
    assert(baselineAction == "BARK");
    assert(proposedFirstAction == "BARK");

    BackyardEnvironment baselineWorld;
    BackyardEnvironment proposedWorld;
    baselineWorld.beginInteraction();
    proposedWorld.beginInteraction();
    baselineWorld.applyStimulus("A sudden loud noise occurs");
    proposedWorld.applyStimulus("A sudden loud noise occurs");
    baselineWorld.applyAction(baselineAction);
    proposedWorld.applyAction(proposedFirstAction);
    assert(baselineWorld.state().disturbanceActive);
    assert(proposedWorld.state().disturbanceActive);

    ActionOutcome negativeFeedback;
    negativeFeedback.reward = -1.0f;
    negativeFeedback.terminal = true;
    negativeFeedback.outcome_tag = "participant_negative";
    proposed.applyOutcome(observation, proposedFirstAction, negativeFeedback);
    const std::string proposedFollowUp = proposed.selectAction(observation);
    assert(proposedFollowUp == "ACT_CONFUSED");
    assert(baseline.selectAction(observation) == "BARK");
    assert(isActionValidForObservation(proposedFollowUp, observation));
    std::cout << "[PASS] paired noise: shared initial action; proposed agent adapts after negative feedback\n";
}

void pairedRestScenario() {
    EnvironmentConfig config;
    config.seed = 103;
    BaselineAgent baseline(config);
    ProposedAgent proposed(config);
    baseline.setExplorationRate(0.0f);
    proposed.setExplorationRate(0.0f);
    baseline.setLearningEnabled(false);
    proposed.setLearningEnabled(false);

    const EnvironmentObservation observation = restObservation();
    const std::string baselineAction = baseline.selectAction(observation);
    const std::string proposedAction = proposed.selectAction(observation);
    assert(baselineAction == "SLEEP_LAY");
    assert(proposedAction == "SLEEP_LAY");
    assert(isActionValidForObservation(baselineAction, observation));
    assert(isActionValidForObservation(proposedAction, observation));

    BackyardEnvironment baselineWorld;
    BackyardEnvironment proposedWorld;
    baselineWorld.beginInteraction();
    proposedWorld.beginInteraction();
    baselineWorld.applyStimulus("The companion should rest");
    proposedWorld.applyStimulus("The companion should rest");
    baselineWorld.applyAction(baselineAction);
    proposedWorld.applyAction(proposedAction);
    assert(baselineWorld.state().dogResting && proposedWorld.state().dogResting);
    std::cout << "[PASS] paired rest: shared observation produces expected resting world state\n";
}

void resetIsRepeatable() {
    BackyardEnvironment environment;
    environment.applyAction("FETCH");
    environment.applyAction("EAT");
    environment.applyAction("SLEEP_LAY");
    environment.beginInteraction();
    environment.reset();
    assert(environment.state().ballAvailable);
    assert(environment.state().foodAvailable);
    assert(!environment.state().dogHasBall);
    assert(!environment.state().dogResting);
    assert(!environment.state().disturbanceActive);
    assert(environment.state().completedFetches == 0);
    assert(environment.state().interactions == 0);
    std::cout << "[PASS] reset: world returns to identical initial conditions\n";
}
}

int main() {
    pairedFetchScenario();
    pairedNoiseAndFeedbackScenario();
    pairedRestScenario();
    resetIsRepeatable();
}
