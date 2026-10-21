#include "training.h"

#include "Components/State-Action/state-action.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
#include <set>
#include <vector>

namespace
{
    struct EpisodeMetrics
    {
        float total_reward = 0.0f;
        int steps = 0;
        bool completed = false;
        std::map<std::string, int> action_counts;
        std::set<std::size_t> visited_states;
    };

    EnvironmentObservation makeObservation(std::mt19937 &rng, int stepIndex)
    {
        // Use a repeatable interaction pattern rather than unrelated random
        // observations. Repeated contexts give the tabular policies enough
        // visits to learn, while small controlled variations prevent a single
        // exact observation from being the whole training set.
        std::uniform_int_distribution<int> nearDist(0, 1);
        std::uniform_int_distribution<int> socialDist(0, 1);
        EnvironmentObservation observation;
        switch (stepIndex % 11)
        {
        case 0: // User initiates a social interaction.
            observation = {0, 1, socialDist(rng), 0, 0.45f};
            break;
        case 1: // Explicit fetch request with a nearby object.
            observation = {1, nearDist(rng), 1, 1, 0.82f};
            break;
        case 2: // A loud unexpected disturbance.
            observation = {2, 0, 0, 0, 0.92f};
            break;
        case 3: // Quiet period with no task pending.
            observation = {2, 2, 0, 0, 0.18f};
            break;
        case 4: // Positive social feedback / renewed contact.
            observation = {0, 0, 1, 0, 0.72f};
            break;
        case 5: // Request where the target is too far away.
            observation = {1, 2, 1, 1, 0.56f};
            break;
        case 6: // The far-fetch request persists for another decision.
            observation = {1, 2, 1, 1, 0.56f};
            break;
        case 7: // Food is explicitly available.
            observation = {2, 0, 1, 0, 0.30f, true, false};
            break;
        case 8: // The user explicitly invites play.
            observation = {0, 1, 1, 0, 0.70f, false, true};
            break;
        case 9: // The user rejects an unwanted behaviour.
            observation = {0, 1, 2, 0, 0.45f};
            break;
        default: // A task is pending but the user has not responded.
            observation = {1, 1, 0, 1, 0.82f};
            break;
        }
        return observation;
    }

    struct EnvironmentStep
    {
        EnvironmentObservation next_observation;
        float reward = 0.0f;
        float energy_after = 0.8f;
        bool completed = false;
    };

    float rewardFor(const std::string &action,
                    const EnvironmentObservation &observation,
                    bool fetchSucceeded)
    {
        // External task/social reward is shared by both agents. Energy and effort
        // are environment variables; only the proposed policy observes energy.
        if (action == "EAT") return observation.food_available ? 0.80f : -0.50f;
        if (action == "PLAY") return observation.play_available ? 0.65f : -0.40f;
        if (action == "WHINE") return observation.user_status == 2 ? 0.35f : -0.15f;
        if (action == "HIDE_AVOID")
            return (observation.user_status == 2 || observation.signal_strength >= 0.90f) ? 0.45f : -0.10f;
        if (action == "APPROACH_FOLLOW")
            return observation.user_status != 2 ? 0.20f : -0.20f;
        if (action == "SNIFF")
            return observation.target_proximity <= 1 ? 0.30f : -0.05f;

        float reward = -0.10f;
        switch (observation.goal_state)
        {
        case 0:
            if (action == "GREETING")
                reward += observation.user_status == 0 ? 0.85f : 0.65f;
            else if (action == "NUZZLE")
                reward += observation.target_proximity == 0 && observation.user_status != 2 ? 0.75f : -0.20f;
            else if (action == "BARK")
                reward -= 0.30f;
            else if (action == "FETCH" || action == "INSIST")
                reward -= 0.45f;
            else if (action == "WANDER")
                reward -= 0.10f;
            break;
        case 1:
            if (action == "FETCH")
            {
                if (observation.task_status != 1) reward -= 0.55f;
                else if (fetchSucceeded) reward += 0.75f + observation.signal_strength * 0.10f;
                else reward -= 0.20f;
            }
            else if (action == "APPROACH_FOLLOW")
                reward += observation.task_status == 1 ? 0.20f : -0.20f;
            else if (action == "ACT_CONFUSED")
                reward += observation.task_status == 0 ? 0.15f : -0.20f;
            else if (action == "WANDER")
                reward += observation.target_proximity >= 2 ? 0.10f : -0.15f;
            else if (action == "INSIST")
                reward += observation.user_status == 0 ? 0.15f : -0.25f;
            else
                reward -= 0.35f;
            break;
        case 2:
        default:
            if (observation.signal_strength >= 0.80f)
            {
                if (action == "ACT_CONFUSED") reward += 0.55f;
                else if (action == "BARK") reward += 0.35f;
                else if (action == "INSIST" || action == "FETCH") reward -= 0.45f;
            }
            else if (observation.task_status == 0)
            {
                if (action == "SLEEP_LAY") reward += 0.65f;
                else if (action == "WANDER") reward += 0.15f;
                else if (action == "FETCH" || action == "INSIST") reward -= 0.35f;
            }
            else
            {
                if (action == "ACT_CONFUSED") reward += 0.20f;
                else if (action == "WANDER") reward += 0.10f;
            }
            break;
        }
        return reward;
    }

    EnvironmentStep applyEnvironmentAction(const std::string &action,
                                           const EnvironmentObservation &observation,
                                           float energy,
                                           std::mt19937 &rng)
    {
        EnvironmentStep result;
        result.next_observation = observation;
        energy = std::clamp(energy, 0.0f, 1.0f);

        const int proximity = std::clamp(observation.target_proximity, 0, 2);
        const float distanceCost[] = {0.035f, 0.085f, 0.16f};
        float effort = 0.012f;
        bool fetchSucceeded = false;

        if (action == "FETCH" && observation.task_status == 1 && observation.target_proximity < 3)
        {
            const float baseProbability[] = {0.96f, 0.78f, 0.52f};
            const float successProbability = std::clamp(
                baseProbability[proximity] * (0.55f + 0.45f * energy), 0.0f, 1.0f);
            std::bernoulli_distribution success(successProbability);
            fetchSucceeded = success(rng);
            effort = distanceCost[proximity] * (1.0f + 0.8f * (1.0f - energy));
            if (fetchSucceeded) result.next_observation.task_status = 0;
        }
        else if (action == "APPROACH_FOLLOW" && observation.target_proximity > 0 &&
                 observation.target_proximity < 3 && observation.user_status != 2)
        {
            effort = 0.045f * (1.0f + 0.8f * (1.0f - energy));
            result.next_observation.target_proximity = std::max(0, observation.target_proximity - 1);
        }
        else if (action == "SLEEP_LAY")
        {
            effort = 0.0f;
            energy = std::min(1.0f, energy + 0.28f);
        }
        else if (action == "EAT" && observation.food_available)
        {
            energy = std::min(1.0f, energy + 0.24f);
            result.next_observation.food_available = false;
        }
        else if (action == "PLAY" && observation.play_available)
        {
            effort = 0.055f * (1.0f + 0.8f * (1.0f - energy));
            result.next_observation.play_available = false;
        }

        if (action != "SLEEP_LAY" && !(action == "EAT" && observation.food_available))
            energy = std::max(0.0f, energy - effort * 1.5f);

        result.energy_after = std::clamp(energy, 0.0f, 1.0f);
        result.reward = std::clamp(
            rewardFor(action, observation, fetchSucceeded) - effort - 0.05f,
            -1.0f, 1.0f);
        result.completed = action == "FETCH" && fetchSucceeded &&
                           observation.task_status == 1 && observation.target_proximity < 3;
        return result;
    }

    EnvironmentObservation mergeTaskTransition(const EnvironmentObservation &current,
                                               const EnvironmentObservation &transitioned,
                                               const EnvironmentObservation &nextScenario)
    {
        // Preserve action-caused changes while a multi-step fetch request is still
        // active. A genuinely different scenario remains an external context change.
        if (current.goal_state == 1 && current.task_status == 1 &&
            nextScenario.goal_state == 1 && nextScenario.task_status == 1)
        {
            EnvironmentObservation merged = nextScenario;
            merged.target_proximity = transitioned.target_proximity;
            merged.task_status = transitioned.task_status;
            return merged;
        }
        return nextScenario;
    }

    bool testEnvironmentActionTransitions()
    {
        EnvironmentObservation farRequest{1, 2, 1, 1, 0.56f};
        std::mt19937 rng(17);
        const EnvironmentStep approached = applyEnvironmentAction("APPROACH_FOLLOW", farRequest, 0.8f, rng);
        const EnvironmentStep rested = applyEnvironmentAction(
            "SLEEP_LAY", EnvironmentObservation{2, 1, 0, 0, 0.18f}, 0.25f, rng);
        const EnvironmentStep nearFetch = applyEnvironmentAction(
            "FETCH", EnvironmentObservation{1, 0, 1, 1, 0.82f}, 0.8f, rng);
        const EnvironmentStep farFetch = applyEnvironmentAction(
            "FETCH", farRequest, 0.8f, rng);
        bool sawFarSuccess = false;
        bool sawFarFailure = false;
        for (unsigned int seed = 1; seed <= 200; ++seed) {
            std::mt19937 trialRng(seed);
            const EnvironmentStep trial = applyEnvironmentAction("FETCH", farRequest, 0.8f, trialRng);
            sawFarSuccess = sawFarSuccess || trial.completed;
            sawFarFailure = sawFarFailure || !trial.completed;
            if (trial.completed && trial.next_observation.task_status != 0) return false;
            if (!trial.completed && trial.next_observation.task_status != 1) return false;
        }
        return approached.next_observation.target_proximity == 1 &&
               approached.energy_after < 0.8f &&
               approached.next_observation.task_status == 1 &&
               rested.energy_after > 0.25f &&
               farFetch.energy_after < nearFetch.energy_after &&
               sawFarSuccess && sawFarFailure;
    }

    template <typename Agent>
    EpisodeMetrics runEpisode(Agent &agent,
                              const std::vector<EnvironmentObservation> &observations,
                              bool learning,
                              float epsilon,
                              std::ofstream &stepLog,
                              const std::string &agentName,
                              int episodeIndex)
    {
        agent.reset();
        agent.setLearningEnabled(learning);
        agent.setExplorationRate(epsilon);

        EpisodeMetrics metrics;
        float environmentEnergy = 0.8f;
        EnvironmentObservation currentObservation = observations.front();
        for (std::size_t step = 0; step < observations.size(); ++step)
        {
            if (step > 0) {
                currentObservation = mergeTaskTransition(
                    observations[step - 1], currentObservation, observations[step]);
            }
            const EnvironmentObservation observation = currentObservation;
            const std::string action = agent.selectAction(observation);
            metrics.visited_states.insert(agent.getCurrentState().encode());

            const float energyBefore = environmentEnergy;
            // Same seed per episode/step gives both agents paired environmental noise,
            // independent of how many stochastic actions either policy selected.
            std::mt19937 transitionRng(static_cast<unsigned int>(episodeIndex + 1) * 7919u +
                                       static_cast<unsigned int>(step) * 104729u);
            const EnvironmentStep transition =
                applyEnvironmentAction(action, observation, environmentEnergy, transitionRng);
            environmentEnergy = transition.energy_after;
            const bool terminal = step + 1 == observations.size();

            EnvironmentObservation nextObservation = transition.next_observation;
            if (!terminal) {
                nextObservation = mergeTaskTransition(
                    observation, transition.next_observation, observations[step + 1]);
            }

            ActionOutcome outcome;
            outcome.action_id = static_cast<int>(std::distance(
                companionActionSet().begin(),
                std::find(companionActionSet().begin(), companionActionSet().end(), action)));
            outcome.reward = transition.reward;
            outcome.terminal = terminal;
            outcome.outcome_tag = transition.completed ? "goal_completed" :
                                  (terminal ? "step_limit" : "interaction");
            outcome.energy_after = transition.energy_after;

            agent.applyOutcome(observation, action, outcome, nextObservation);

            metrics.total_reward += transition.reward;
            metrics.steps = static_cast<int>(step + 1);
            metrics.completed = metrics.completed || transition.completed;
            ++metrics.action_counts[action];

            nlohmann::json row;
            row["agent"] = agentName;
            row["episode"] = episodeIndex;
            row["step"] = step;
            row["goal_state"] = observation.goal_state;
            row["target_proximity"] = observation.target_proximity;
            row["user_status"] = observation.user_status;
            row["task_status"] = observation.task_status;
            row["signal_strength"] = observation.signal_strength;
            row["food_available"] = observation.food_available;
            row["play_available"] = observation.play_available;
            row["energy_before"] = energyBefore;
            row["energy_after"] = transition.energy_after;
            row["action"] = action;
            row["encoded_state"] = agent.getCurrentState().encode();
            row["reward"] = transition.reward;
            row["terminal"] = terminal;
            row["completed"] = transition.completed;
            row["fetch_succeeded"] = transition.completed;
            stepLog << row.dump() << '\n';

            currentObservation = nextObservation;
            if (terminal) break;
        }
        return metrics;
    }

    nlohmann::json metricsToJson(const EpisodeMetrics &metrics)
    {
        nlohmann::json result;
        result["total_reward"] = metrics.total_reward;
        result["steps"] = metrics.steps;
        result["completed"] = metrics.completed;
        result["action_counts"] = metrics.action_counts;
        result["unique_states_visited"] = metrics.visited_states.size();
        return result;
    }
} // namespace

bool runTrainingExperiment(const std::string &mode,
                           int episodes,
                           int steps,
                           unsigned int seed,
                           const std::string &outputDirectory)
{
    if (mode != "train" && mode != "validate" && mode != "compare")
    {
        std::cerr << "Unsupported experiment mode: " << mode << '\n';
        return false;
    }
    if (episodes < 1 || steps < 1)
    {
        std::cerr << "Episodes and steps must both be positive.\n";
        return false;
    }

    namespace fs = std::filesystem;
    const fs::path outDir(outputDirectory);
    fs::create_directories(outDir);
    const fs::path baselinePolicy = outDir / "baseline.qtable";
    const fs::path proposedPolicy = outDir / "proposed.qtable";

    if (mode != "train" && (!fs::exists(baselinePolicy) || !fs::exists(proposedPolicy)))
    {
        std::cerr << "No trained policies found. Run --train first; expected "
                  << baselinePolicy << " and " << proposedPolicy << ".\n";
        return false;
    }

    EnvironmentConfig config;
    config.seed = static_cast<int>(seed);
    BaselineAgent baseline(config);
    ProposedAgent proposed(config);

    if (mode != "train")
    {
        baseline.loadPersistentState(baselinePolicy.string());
        proposed.loadPersistentState(proposedPolicy.string());
        baseline.setLearningEnabled(false);
        proposed.setLearningEnabled(false);
        baseline.setExplorationRate(0.0f);
        proposed.setExplorationRate(0.0f);
    }

    std::ofstream baselineLog(outDir / (mode + "_baseline_steps.jsonl"));
    std::ofstream proposedLog(outDir / (mode + "_proposed_steps.jsonl"));
    if (!baselineLog || !proposedLog)
    {
        std::cerr << "Unable to open experiment log files in " << outDir << '\n';
        return false;
    }

    const unsigned int environmentSeed = mode == "train" ? seed : seed + 1000003u;
    std::mt19937 environmentRng(environmentSeed);
    nlohmann::json summary;
    summary["mode"] = mode;
    summary["seed"] = seed;
    summary["environment_seed"] = environmentSeed;
    summary["episodes"] = episodes;
    summary["max_steps_per_episode"] = steps;
    summary["reward_model"] = "synthetic_v2_context_sequence";
    summary["scenario_pattern"] = {"social", "fetch_near", "disturbance", "rest", "positive_social", "fetch_far", "fetch_follow_up", "food_offer", "play_invitation", "negative_feedback", "ignored_request"};
    summary["note"] = "Training uses repeated contextual scenarios for tabular coverage. Synthetic reward is not a measure of perceived believability.";

    nlohmann::json baselineEpisodes = nlohmann::json::array();
    nlohmann::json proposedEpisodes = nlohmann::json::array();
    double baselineRewardSum = 0.0;
    double proposedRewardSum = 0.0;
    double baselineStepSum = 0.0;
    double proposedStepSum = 0.0;
    int baselineCompletions = 0;
    int proposedCompletions = 0;

    for (int episode = 0; episode < episodes; ++episode)
    {
        std::vector<EnvironmentObservation> observations;
        observations.reserve(static_cast<std::size_t>(steps));
        for (int step = 0; step < steps; ++step)
        {
            observations.push_back(makeObservation(environmentRng, step));
        }

        const float epsilon = mode == "train"
                                  ? std::max(0.05f, 0.30f * (1.0f - static_cast<float>(episode) / static_cast<float>(episodes)))
                                  : 0.0f;
        const bool learning = mode == "train";

        const EpisodeMetrics baselineMetrics = runEpisode(
            baseline, observations, learning, epsilon, baselineLog, "baseline", episode);
        const EpisodeMetrics proposedMetrics = runEpisode(
            proposed, observations, learning, epsilon, proposedLog, "proposed", episode);

        baselineRewardSum += baselineMetrics.total_reward;
        proposedRewardSum += proposedMetrics.total_reward;
        baselineStepSum += baselineMetrics.steps;
        proposedStepSum += proposedMetrics.steps;
        baselineCompletions += baselineMetrics.completed ? 1 : 0;
        proposedCompletions += proposedMetrics.completed ? 1 : 0;

        nlohmann::json baselineRow = metricsToJson(baselineMetrics);
        baselineRow["episode"] = episode;
        baselineEpisodes.push_back(baselineRow);
        nlohmann::json proposedRow = metricsToJson(proposedMetrics);
        proposedRow["episode"] = episode;
        proposedEpisodes.push_back(proposedRow);
    }

    baselineLog.close();
    proposedLog.close();

    if (mode == "train")
    {
        baseline.savePersistentState(baselinePolicy.string());
        proposed.savePersistentState(proposedPolicy.string());
    }

    summary["baseline"]["mean_total_reward_per_episode"] = baselineRewardSum / episodes;
    summary["baseline"]["mean_reward_per_step"] = baselineStepSum > 0.0 ? baselineRewardSum / baselineStepSum : 0.0;
    summary["baseline"]["mean_steps_per_episode"] = baselineStepSum / episodes;
    summary["baseline"]["mean_reward"] = baselineRewardSum / episodes; // Backwards-compatible alias.
    summary["baseline"]["completion_rate"] = static_cast<double>(baselineCompletions) / episodes;
    summary["baseline"]["episodes"] = baselineEpisodes;
    summary["proposed"]["mean_total_reward_per_episode"] = proposedRewardSum / episodes;
    summary["proposed"]["mean_reward_per_step"] = proposedStepSum > 0.0 ? proposedRewardSum / proposedStepSum : 0.0;
    summary["proposed"]["mean_steps_per_episode"] = proposedStepSum / episodes;
    summary["proposed"]["mean_reward"] = proposedRewardSum / episodes; // Backwards-compatible alias.
    summary["proposed"]["completion_rate"] = static_cast<double>(proposedCompletions) / episodes;
    summary["proposed"]["episodes"] = proposedEpisodes;

    std::ofstream summaryFile(outDir / (mode + "_summary.json"));
    if (!summaryFile)
    {
        std::cerr << "Unable to write experiment summary in " << outDir << '\n';
        return false;
    }
    summaryFile << summary.dump(2) << '\n';

    std::cout << mode << " experiment complete (" << episodes << " episodes, seed "
              << seed << ").\n"
              << "Baseline mean reward per step: " << summary["baseline"]["mean_reward_per_step"] << '\n'
              << "Proposed mean reward per step: " << summary["proposed"]["mean_reward_per_step"] << '\n'
              << "Results: " << (outDir / (mode + "_summary.json")) << '\n';

    return true;
}


bool runTrainingSelfTests()
{
    const bool passed = testEnvironmentActionTransitions();
    std::cout << "Action-dependent environment transitions: " << (passed ? "PASS" : "FAIL") << '\n';
    return passed;
}
