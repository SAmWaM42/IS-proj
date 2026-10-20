#include "training.h"

#include "Components/State-Action/state-action.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
struct EpisodeMetrics {
    float total_reward = 0.0f;
    int steps = 0;
    bool completed = false;
    std::map<std::string, int> action_counts;
};

EnvironmentObservation makeObservation(std::mt19937& rng) {
    std::uniform_int_distribution<int> goalDist(0, 2);
    std::uniform_int_distribution<int> proximityDist(0, 3);
    std::uniform_int_distribution<int> userDist(0, 2);
    std::uniform_int_distribution<int> taskDist(0, 1);
    std::uniform_real_distribution<float> signalDist(0.0f, 1.0f);

    EnvironmentObservation observation;
    observation.goal_state = goalDist(rng);
    observation.target_proximity = proximityDist(rng);
    observation.user_status = userDist(rng);
    observation.task_status = taskDist(rng);
    observation.signal_strength = signalDist(rng);
    return observation;
}

float rewardFor(const std::string& action, const EnvironmentObservation& observation) {
    // Reward is conditioned on the interaction intent, not just generic environment values.
    float reward = -0.10f;
    switch (observation.goal_state) {
        case 0: // Social approach
            if (action == "GREETING") reward += observation.user_status == 0 ? 0.85f : 0.65f;
            else if (action == "NUZZLE") reward += observation.target_proximity == 0 && observation.user_status != 2 ? 0.75f : -0.20f;
            else if (action == "BARK") reward -= 0.30f;
            else if (action == "FETCH" || action == "INSIST") reward -= 0.45f;
            else if (action == "WANDER") reward -= 0.10f;
            break;
        case 1: // Explicit request / task
            if (action == "FETCH") {
                reward += observation.task_status == 1 ? 0.75f : -0.55f;
                reward += observation.target_proximity <= 1 ? 0.25f : -0.30f;
                reward += observation.signal_strength * 0.10f;
            } else if (action == "ACT_CONFUSED") reward += observation.task_status == 0 ? 0.15f : -0.20f;
            else if (action == "WANDER") reward += observation.target_proximity >= 2 ? 0.10f : -0.15f;
            else reward -= 0.35f;
            break;
        case 2: // Disturbance / rest
        default:
            if (observation.signal_strength >= 0.80f) {
                if (action == "ACT_CONFUSED") reward += 0.55f;
                else if (action == "BARK") reward += 0.35f;
                else if (action == "INSIST" || action == "FETCH") reward -= 0.45f;
            } else if (observation.task_status == 0) {
                if (action == "SLEEP_LAY") reward += 0.65f;
                else if (action == "WANDER") reward += 0.15f;
                else if (action == "FETCH" || action == "INSIST") reward -= 0.35f;
            } else {
                if (action == "ACT_CONFUSED") reward += 0.20f;
                else if (action == "WANDER") reward += 0.10f;
            }
            break;
    }
    return std::clamp(reward, -1.0f, 1.0f);
}

template <typename Agent>
EpisodeMetrics runEpisode(Agent& agent,
                          const std::vector<EnvironmentObservation>& observations,
                          bool learning,
                          float epsilon,
                          std::ofstream& stepLog,
                          const std::string& agentName,
                          int episodeIndex) {
    agent.reset();
    agent.setLearningEnabled(learning);
    agent.setExplorationRate(epsilon);

    EpisodeMetrics metrics;
    for (std::size_t step = 0; step < observations.size(); ++step) {
        const auto& observation = observations[step];
        const std::string action = agent.selectAction(observation);
        const float reward = rewardFor(action, observation);
        const bool completed = observation.goal_state == 1 &&
                               action == "FETCH" &&
                               observation.task_status == 1 &&
                               observation.target_proximity <= 1 &&
                               observation.signal_strength >= 0.72f;
        const bool terminal = completed || step + 1 == observations.size();

        ActionOutcome outcome;
        outcome.action_id = static_cast<int>(step);
        outcome.reward = reward;
        // Training observations are sampled independently, so this is a contextual-bandit update.
        // Do not bootstrap from an unrelated randomly sampled next observation.
        outcome.terminal = true;
        outcome.outcome_tag = completed ? "goal_completed" : (terminal ? "step_limit" : "interaction");

        agent.applyOutcome(observation, action, outcome);

        metrics.total_reward += reward;
        metrics.steps = static_cast<int>(step + 1);
        metrics.completed = metrics.completed || completed;
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
        row["action"] = action;
        row["reward"] = reward;
        row["terminal"] = terminal;
        row["completed"] = completed;
        stepLog << row.dump() << '\n';

        if (terminal) {
            break;
        }
    }
    return metrics;
}

nlohmann::json metricsToJson(const EpisodeMetrics& metrics) {
    nlohmann::json result;
    result["total_reward"] = metrics.total_reward;
    result["steps"] = metrics.steps;
    result["completed"] = metrics.completed;
    result["action_counts"] = metrics.action_counts;
    return result;
}
} // namespace

bool runTrainingExperiment(const std::string& mode,
                           int episodes,
                           int steps,
                           unsigned int seed,
                           const std::string& outputDirectory) {
    if (mode != "train" && mode != "validate" && mode != "compare") {
        std::cerr << "Unsupported experiment mode: " << mode << '\n';
        return false;
    }
    if (episodes < 1 || steps < 1) {
        std::cerr << "Episodes and steps must both be positive.\n";
        return false;
    }

    namespace fs = std::filesystem;
    const fs::path outDir(outputDirectory);
    fs::create_directories(outDir);
    const fs::path baselinePolicy = outDir / "baseline.qtable";
    const fs::path proposedPolicy = outDir / "proposed.qtable";

    if (mode != "train" && (!fs::exists(baselinePolicy) || !fs::exists(proposedPolicy))) {
        std::cerr << "No trained policies found. Run --train first; expected "
                  << baselinePolicy << " and " << proposedPolicy << ".\n";
        return false;
    }

    EnvironmentConfig config;
    config.max_steps = steps;
    config.seed = static_cast<int>(seed);
    BaselineAgent baseline(config);
    ProposedAgent proposed(config);

    if (mode != "train") {
        baseline.loadPersistentState(baselinePolicy.string());
        proposed.loadPersistentState(proposedPolicy.string());
        baseline.setLearningEnabled(false);
        proposed.setLearningEnabled(false);
        baseline.setExplorationRate(0.0f);
        proposed.setExplorationRate(0.0f);
    }

    std::ofstream baselineLog(outDir / (mode + "_baseline_steps.jsonl"));
    std::ofstream proposedLog(outDir / (mode + "_proposed_steps.jsonl"));
    if (!baselineLog || !proposedLog) {
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
    summary["reward_model"] = "synthetic_v1";
    summary["note"] = "Synthetic reward is not a measure of perceived believability.";

    nlohmann::json baselineEpisodes = nlohmann::json::array();
    nlohmann::json proposedEpisodes = nlohmann::json::array();
    double baselineRewardSum = 0.0;
    double proposedRewardSum = 0.0;
    double baselineStepSum = 0.0;
    double proposedStepSum = 0.0;
    int baselineCompletions = 0;
    int proposedCompletions = 0;

    for (int episode = 0; episode < episodes; ++episode) {
        std::vector<EnvironmentObservation> observations;
        observations.reserve(static_cast<std::size_t>(steps));
        for (int step = 0; step < steps; ++step) {
            observations.push_back(makeObservation(environmentRng));
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

    if (mode == "train") {
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
    if (!summaryFile) {
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
