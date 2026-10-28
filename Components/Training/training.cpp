#include "training.h"

#include "Components/State-Action/state-action.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
#include <set>
#include <vector>
#include <utility>
#include <type_traits>

namespace
{
    // Sample varied but bounded elapsed time between decisions. Paired agents
    // receive the same interval sequence within each episode.
    constexpr float kMinSecondsPerTrainingStep = 0.5f;
    constexpr float kMaxSecondsPerTrainingStep = 5.0f;

    // Keep one previous version of each generated artifact, tagged with compare_.
    // The comparison-mode outputs already begin with compare_, so their backup
    // uses compare_previous_ to avoid an ambiguous compare_compare_ prefix.
    void rotateArtifactForComparison(const std::filesystem::path& currentPath)
    {
        if (!std::filesystem::is_regular_file(currentPath))
            return;

        const std::string filename = currentPath.filename().string();
        const std::string backupName = filename.rfind("compare_", 0) == 0
            ? "compare_previous_" + filename.substr(8)
            : "compare_" + filename;
        const auto backupPath = currentPath.parent_path() / backupName;
        std::error_code ec;
        std::filesystem::remove(backupPath, ec);
        ec.clear();
        std::filesystem::rename(currentPath, backupPath, ec);
        if (ec)
            std::cerr << "Warning: unable to preserve previous artifact " << currentPath
                      << " as " << backupPath << ": " << ec.message() << '\\n';
    }

    struct EpisodeMetrics
    {
        float total_reward = 0.0f;
        int steps = 0;
        bool completed = false;
        int stimulus_checks = 0;
        int stimulus_aligned_actions = 0;
        std::map<std::string, int> stimulus_checks_by_type;
        std::map<std::string, int> stimulus_aligned_by_type;
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
        switch (stepIndex % 14)
        {
        case 0: // User initiates a social interaction.
            observation = {0, 1, socialDist(rng), 0, 0.45f};
            break;
        case 1: // Explicit fetch request with a nearby object.
            observation = {1, nearDist(rng), 1, 1, 0.82f};
            break;
        case 2: // The nearby fetch request persists for a second decision.
            observation = {1, 1, 1, 1, 0.82f};
            break;
        case 3: // A loud unexpected disturbance.
            observation = {2, 0, 0, 0, 0.92f};
            break;
        case 4: // Quiet period with no task pending.
            observation = {2, 2, 0, 0, 0.18f};
            break;
        case 5: // Positive social feedback / renewed contact.
            observation = {0, 0, 1, 0, 0.72f};
            break;
        case 6: // Request where the target is too far away.
            observation = {1, 2, 1, 1, 0.56f};
            break;
        case 7: // The far-fetch request persists for another decision.
            observation = {1, 2, 1, 1, 0.56f};
            break;
        case 8: // A third decision permits retrying after a failed fetch.
            observation = {1, 2, 1, 1, 0.56f};
            break;
        case 9: // Food is explicitly available.
            observation = {2, 0, 1, 0, 0.30f, true, false};
            break;
        case 10: // The user explicitly invites play.
            observation = {0, 1, 1, 0, 0.70f, false, true};
            break;
        case 11: // The user rejects an unwanted behaviour.
            observation = {0, 1, 2, 0, 0.45f};
            break;
        case 12: // A task is pending but the user has not responded.
            observation = {1, 1, 0, 1, 0.82f};
            break;
        default: // Fetch is requested during feeding time: the ball and food coexist.
            observation = {1, 1, 1, 1, 0.56f, true, false};
            break;
        }
        return observation;
    }

    struct EnvironmentStep
    {
        EnvironmentObservation next_observation;
        float reward = 0.0f;
        float energy_after = 0.8f;
        float stress_after = 0.2f;
        float user_satisfaction_after = 0.5f;
        bool completed = false;
    };

    struct ActionProfile
    {
        float fetch;
        float food;
        float play;
        float social;
        float disturbance;
        float rest;
        float rejection;
        float energyRelief;
        float stressRelief;
        float curiosityRelief;
    };

    const ActionProfile &actionProfileFor(const std::string &action)
    {
        // Profiles express broad affordances and drive effects. They are not a
        // scenario-to-correct-action lookup table.
        static const std::map<std::string, ActionProfile> profiles = {
            {"FETCH",           { 1.00f, -0.40f, -0.10f, -0.40f, -0.90f, -0.80f, -0.70f, -0.40f, -0.20f, -0.20f}},
            {"APPROACH_FOLLOW", { 0.65f, -0.20f,  0.50f,  0.55f, -0.30f, -0.20f, -0.80f, -0.15f, -0.10f,  0.10f}},
            {"EAT",             {-0.15f,  1.00f, -0.20f, -0.10f, -0.70f,  0.20f, -0.30f,  0.55f,  0.05f,  0.00f}},
            {"SLEEP_LAY",       {-0.25f, -0.20f, -0.35f, -0.25f, -0.90f,  1.00f,  0.25f,  0.60f,  3.00f, -0.15f}},
            {"PLAY",            { 0.05f, -0.25f,  1.00f,  0.60f, -0.65f, -0.50f, -0.40f, -0.20f, -0.45f,  0.40f}},
            {"SNIFF",           {-0.85f, -0.55f, -0.25f, -0.35f, -0.80f,  0.15f, -0.55f, -0.10f, -0.10f,  0.35f}},
            {"INSIST",          { 0.35f, -0.40f, -0.40f, -0.30f, -0.60f, -0.70f, -1.00f, -0.10f, -0.30f,  0.00f}},
            {"HIDE_AVOID",      {-0.45f, -0.30f, -0.60f, -0.30f,  0.85f,  0.10f,  0.95f,  0.05f,  0.70f, -0.10f}},
            {"WHINE",           {-0.25f, -0.20f, -0.25f, -0.10f,  0.35f, -0.20f,  0.50f, -0.10f,  0.20f, -0.10f}},
            {"BARK",            {-0.45f, -0.20f, -0.20f, -0.25f,  0.60f, -0.40f, -0.10f, -0.10f,  0.10f,  0.00f}},
            {"ACT_CONFUSED",    {-0.10f, -0.10f, -0.10f,  0.10f,  0.70f,  0.10f,  0.40f,  0.00f,  0.40f,  0.10f}},
            {"GREETING",        {-0.20f, -0.10f,  0.40f,  1.00f, -0.30f, -0.30f, -0.50f, -0.10f, -0.10f,  0.10f}},
            {"NUZZLE",          {-0.25f, -0.10f,  0.45f,  0.90f, -0.40f, -0.30f, -0.60f, -0.10f, -0.10f,  0.10f}},
            {"WANDER",          {-0.30f, -0.10f,  0.15f,  0.10f, -0.20f,  0.50f, -0.30f,  0.10f,  0.10f,  0.35f}}
        };
        static const ActionProfile fallback{-0.10f, -0.10f, -0.10f, 0.0f, -0.10f, 0.0f, -0.10f, 0.0f, 0.0f, 0.0f};
        const auto it = profiles.find(action);
        return it == profiles.end() ? fallback : it->second;
    }

    float actionContextFit(const std::string &action,
                           const EnvironmentObservation &observation,
                           float energy,
                           float stress)
    {
        const ActionProfile &profile = actionProfileFor(action);
        float weightedFit = 0.0f;
        float totalWeight = 0.0f;
        const auto addCue = [&](float weight, float fit) {
            weightedFit += weight * fit;
            totalWeight += weight;
        };

        if (observation.goal_state == 1 && observation.task_status == 1)
            addCue(1.00f, profile.fetch);
        if (observation.food_available)
            addCue(0.80f, profile.food);
        if (observation.play_available)
            addCue(0.70f, profile.play);
        if (observation.goal_state == 0 && observation.user_status != 2)
            addCue(0.55f, profile.social);
        if (observation.goal_state == 2 && observation.signal_strength >= 0.80f)
            addCue(0.95f, profile.disturbance);
        if (observation.goal_state == 2 && observation.task_status == 0 &&
            observation.signal_strength < 0.80f)
            addCue(0.45f, profile.rest);
        if (observation.user_status == 2)
            addCue(0.90f, profile.rejection);

        const float contextualFit = totalWeight > 0.0f
            ? weightedFit / totalWeight
            : 0.25f * profile.curiosityRelief;
        const float energyDrive = std::clamp(1.0f - energy, 0.0f, 1.0f);
        const float stressDrive = std::clamp(stress, 0.0f, 1.0f);
        const float driveFit = 0.30f * profile.energyRelief * energyDrive +
                               0.30f * profile.stressRelief * stressDrive +
                               0.10f * profile.curiosityRelief;
        return std::clamp(contextualFit + driveFit, -1.0f, 1.0f);
    }

    float rewardFor(const std::string &action,
                    const EnvironmentObservation &observation,
                    bool fetchSucceeded,
                    float energyBefore = 0.8f,
                    float stressBefore = 0.2f,
                    float userSatisfaction = 0.5f)
    {
        const float energy = std::clamp(energyBefore, 0.0f, 1.0f);
        const float stress = std::clamp(stressBefore, 0.0f, 1.0f);
        const float satisfaction = std::clamp(userSatisfaction, 0.0f, 1.0f);
        const float fit = actionContextFit(action, observation, energy, stress);

        float driveRelief = 0.0f;
        if (action == "EAT" && observation.food_available)
            driveRelief = 0.18f + 0.42f * (1.0f - energy);
        else if (action == "SLEEP_LAY")
            driveRelief = 0.28f * (1.0f - energy) + 0.45f * stress;

        // Explicit survival pressure: below 35% energy, recovery is increasingly
        // valuable and non-recovery actions carry a growing opportunity cost.
        if (energy < 0.35f) {
            const float urgency = (0.35f - energy) / 0.35f;
            const bool validRecovery = action == "SLEEP_LAY" ||
                                       (action == "EAT" && observation.food_available);
            driveRelief += validRecovery ? 0.30f * urgency : -0.22f * urgency;
        }
        else if (action == "PLAY" && observation.play_available)
            driveRelief = 0.12f * energy - 0.10f * stress;
        else if (action == "APPROACH_FOLLOW" && observation.target_proximity >= 2 &&
                 observation.task_status == 1)
            driveRelief = 0.08f;

        // Compliance is rewarded only on successful completion. Deviation itself
        // is not punished; the separate user-response term models disappointment.
        const float complianceBonus =
            observation.goal_state == 1 && observation.task_status == 1 &&
            action == "FETCH" && fetchSucceeded ? 0.62f : 0.0f;

        float userResponse = 0.0f;
        if (observation.goal_state == 1 && observation.task_status == 1 && !fetchSucceeded)
        {
            const bool justifiedDeviation =
                (action == "EAT" && observation.food_available && energy < 0.45f) ||
                (action == "SLEEP_LAY" && (energy < 0.25f || stress > 0.78f)) ||
                (action == "APPROACH_FOLLOW" && observation.target_proximity >= 2);
            if (action != "FETCH")
                userResponse = justifiedDeviation
                    ? -0.035f * (0.5f + satisfaction)
                    : -0.16f * (0.5f + satisfaction);
        }
        else if (observation.user_status == 2)
        {
            const bool responseAcknowledgesRejection =
                action == "HIDE_AVOID" || action == "WHINE" || action == "ACT_CONFUSED";
            userResponse = responseAcknowledgesRejection
                ? 0.06f * (0.5f + satisfaction)
                : -0.12f * (0.5f + satisfaction);
        }
        else if (fit >= 0.40f)
        {
            userResponse = 0.05f * satisfaction;
        }

        const float incongruencePenalty = fit < 0.0f ? 0.55f * fit : 0.0f;
        return std::clamp(fit + driveRelief + complianceBonus + userResponse +
                          incongruencePenalty - 0.05f, -1.0f, 1.0f);
    }

    EnvironmentStep applyEnvironmentAction(const std::string &action,
                                           const EnvironmentObservation &observation,
                                           float energy,
                                           float stress,
                                           float userSatisfaction,
                                           std::mt19937 &rng,
                                           float elapsedSeconds = 0.0f)
    {
        EnvironmentStep result;
        result.next_observation = observation;
        energy = std::clamp(energy, 0.0f, 1.0f);
        stress = std::clamp(stress, 0.0f, 1.0f);
        userSatisfaction = std::clamp(userSatisfaction, 0.0f, 1.0f);
        const float energyBefore = energy;
        const float stressBefore = stress;
        const float satisfactionBefore = userSatisfaction;

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

        // Shared physical-energy dynamics: passive drain occurs during the
        // elapsed interval after the action. Resting reduces, but does not
        // eliminate, baseline metabolic drain. The UI uses the same rates.
        const float passiveDrainPerSecond = action == "SLEEP_LAY" ? 0.0008f : 0.0025f;
        energy = std::max(0.0f, energy - passiveDrainPerSecond * std::max(0.0f, elapsedSeconds));
        result.next_observation.energy = energy;

        const float fit = actionContextFit(action, observation, energyBefore, stressBefore);
        if (observation.goal_state == 2 && observation.signal_strength >= 0.80f)
            stress += 0.16f;
        if (observation.user_status == 2) stress += 0.10f;
        if (action == "SLEEP_LAY") stress -= 0.28f;
        if (action == "PLAY" && observation.play_available && stress < 0.75f) stress -= 0.08f;
        if (action == "FETCH" && fetchSucceeded) stress -= 0.04f;
        stress = std::clamp(stress, 0.0f, 1.0f);

        const bool taskPending = observation.goal_state == 1 && observation.task_status == 1;
        const bool justifiedDeviation =
            (action == "EAT" && observation.food_available && energyBefore < 0.45f) ||
            (action == "SLEEP_LAY" && (energyBefore < 0.25f || stressBefore > 0.78f)) ||
            (action == "APPROACH_FOLLOW" && observation.target_proximity >= 2);
        if (taskPending && fetchSucceeded)
            userSatisfaction += 0.10f;
        else if (taskPending && action != "FETCH")
            userSatisfaction -= justifiedDeviation ? 0.025f : 0.08f;
        else if (fit >= 0.40f)
            userSatisfaction += 0.025f;
        else if (fit < 0.0f)
            userSatisfaction -= 0.025f;
        userSatisfaction = std::clamp(userSatisfaction, 0.0f, 1.0f);

        result.energy_after = std::clamp(energy, 0.0f, 1.0f);
        result.stress_after = stress;
        result.user_satisfaction_after = userSatisfaction;
        result.reward = std::clamp(
            rewardFor(action, observation, fetchSucceeded, energyBefore, stressBefore,
                      satisfactionBefore) - effort,
            -1.0f, 1.0f);
        result.completed = action == "FETCH" && fetchSucceeded &&
                           observation.task_status == 1 && observation.target_proximity < 3;
        return result;
    }

    // Compatibility wrapper for deterministic transition tests.
    EnvironmentStep applyEnvironmentAction(const std::string &action,
                                           const EnvironmentObservation &observation,
                                           float energy,
                                           std::mt19937 &rng)
    {
        return applyEnvironmentAction(action, observation, energy, 0.2f, 0.5f, rng);
    }

    EnvironmentObservation mergeTaskTransition(const EnvironmentObservation &current,
                                               const EnvironmentObservation &transitioned,
                                               const EnvironmentObservation &nextScenario)
    {
        // Preserve action-caused changes across contiguous decisions for the same fetch
        // request. In particular, do not resurrect a task after a successful FETCH.
        // A genuinely different scenario remains an external context change.
        if (current.goal_state == 1 && current.task_status == 1 &&
            transitioned.task_status == 0 &&
            nextScenario.goal_state == 1 && nextScenario.task_status == 1)
        {
            // A successful fetch satisfies the request. Do not replay the
            // pre-generated "request still pending" observation as if it were
            // still active; move to a plausible post-completion social response.
            EnvironmentObservation completedFollowUp = nextScenario;
            completedFollowUp.goal_state = 0;
            completedFollowUp.target_proximity = 0;
            completedFollowUp.user_status = 0;
            completedFollowUp.task_status = 0;
            completedFollowUp.signal_strength = 0.72f;
            return completedFollowUp;
        }

        if (current.goal_state == 1 &&
            nextScenario.goal_state == 1 && nextScenario.task_status == 1)
        {
            EnvironmentObservation merged = nextScenario;
            merged.target_proximity = transitioned.target_proximity;
            merged.task_status = transitioned.task_status;
            return merged;
        }
        return nextScenario;
    }

    std::string stimulusCategory(const EnvironmentObservation &observation)
    {
        if (observation.goal_state == 1 && observation.task_status == 1)
        {
            if (observation.food_available) return "fetch_with_food";
            return observation.target_proximity >= 2 ? "fetch_far" : "fetch_near";
        }
        if (observation.goal_state == 0 && observation.user_status == 2)
            return "negative_feedback";
        if (observation.food_available) return "food_offer";
        if (observation.play_available) return "play_invitation";
        if (observation.goal_state == 2 && observation.signal_strength >= 0.80f)
            return "disturbance";
        if (observation.goal_state == 2 && observation.task_status == 0)
            return "quiet_rest";
        if (observation.goal_state == 0) return "social_contact";
        return "other";
    }

    bool stimulusResponseAligned(const std::string &action,
                                 const EnvironmentObservation &observation,
                                 float energy,
                                 float stress)
    {
        // Use the graded context-fit model, not a one-correct-action oracle.
        // This is an engineering diagnostic only, not a believability measure.
        return actionContextFit(action, observation, energy, stress) >= 0.0f;
    }

    bool testTaskPriorityReward()
    {
        const EnvironmentObservation request{1, 0, 1, 1, 0.56f};
        const EnvironmentObservation feedingTimeRequest{1, 1, 1, 1, 0.56f, true, false};
        const EnvironmentObservation idle{2, 1, 0, 0, 0.20f};
        const float fetchReward = rewardFor("FETCH", request, true, 0.75f, 0.20f);
        const float eatWhenHungry = rewardFor("EAT", feedingTimeRequest, false, 0.15f, 0.20f);
        const float sleepWhenOverwhelmed = rewardFor("SLEEP_LAY", feedingTimeRequest, false, 0.20f, 0.90f);
        const float sniffWhenCompetingNeeds = rewardFor("SNIFF", feedingTimeRequest, false, 0.15f, 0.20f);
        const float sniffWhenIdle = rewardFor("SNIFF", idle, false, 0.75f, 0.20f);
        std::mt19937 scenarioRng(42);
        const EnvironmentObservation generated = makeObservation(scenarioRng, 13);
        return fetchReward > sniffWhenCompetingNeeds &&
               eatWhenHungry > sniffWhenCompetingNeeds &&
               sleepWhenOverwhelmed > sniffWhenCompetingNeeds &&
               sleepWhenOverwhelmed > eatWhenHungry &&
               sniffWhenIdle > sniffWhenCompetingNeeds &&
               generated.goal_state == 1 && generated.task_status == 1 &&
               generated.food_available &&
               isActionValidForObservation("FETCH", generated) &&
               isActionValidForObservation("EAT", generated) &&
               isActionValidForObservation("SLEEP_LAY", generated);
    }

    bool testSuccessfulFetchAdvancesContext()
    {
        const EnvironmentObservation activeRequest{1, 1, 1, 1, 0.82f};
        const EnvironmentObservation successfulFetch{1, 0, 1, 0, 0.82f};
        const EnvironmentObservation pendingFollowUp{1, 1, 1, 1, 0.82f};
        const EnvironmentObservation next = mergeTaskTransition(
            activeRequest, successfulFetch, pendingFollowUp);
        return next.goal_state == 0 && next.task_status == 0 &&
               next.user_status == 0 && next.target_proximity == 0 &&
               next.signal_strength == 0.72f;
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

    bool testCriticalEnergyPrioritizesRecovery()
    {
        EnvironmentObservation critical{2, 1, 0, 0, 0.20f, false, true, 0.15f};
        const auto withoutFood = validActionsForObservation(critical);
        if (withoutFood.size() != 1 || withoutFood.front() != "SLEEP_LAY") return false;
        critical.food_available = true;
        const auto withFood = validActionsForObservation(critical);
        return withFood.size() == 2 &&
               std::find(withFood.begin(), withFood.end(), "EAT") != withFood.end() &&
               std::find(withFood.begin(), withFood.end(), "SLEEP_LAY") != withFood.end();
    }

    bool testEnergyDepletionTransition()
    {
        const EnvironmentObservation observation{2, 1, 0, 0, 0.20f, false, false, 0.01f};
        std::mt19937 rng(912u);
        const EnvironmentStep depleted = applyEnvironmentAction(
            "WANDER", observation, 0.01f, 0.2f, 0.5f, rng, 1.0f);
        return depleted.energy_after <= 0.0f;
    }

    bool testPassiveEnergyDrain()
    {
        const EnvironmentObservation observation{2, 1, 0, 0, 0.20f};
        std::mt19937 firstRng(711u), secondRng(711u);
        const EnvironmentStep immediate = applyEnvironmentAction(
            "WANDER", observation, 0.8f, 0.2f, 0.5f, firstRng, 0.0f);
        const EnvironmentStep elapsed = applyEnvironmentAction(
            "WANDER", observation, 0.8f, 0.2f, 0.5f, secondRng, 5.0f);
        return elapsed.energy_after < immediate.energy_after &&
               std::abs((immediate.energy_after - elapsed.energy_after) - 0.0125f) < 1e-5f &&
               std::abs(elapsed.next_observation.energy - elapsed.energy_after) < 1e-6f;
    }

    bool testTrainingAppliesElapsedAffectDecay()
    {
        const EnvironmentObservation observation{0, 1, 1, 0, 0.45f};
        EnvironmentConfig config;
        config.seed = 501;
        ProposedAgent timedAgent(config);
        ProposedAgent immediateAgent(config);
        timedAgent.setLearningEnabled(false);
        immediateAgent.setLearningEnabled(false);
        timedAgent.setExplorationRate(0.0f);
        immediateAgent.setExplorationRate(0.0f);

        const std::string timedAction = timedAgent.selectAction(observation);
        const std::string immediateAction = immediateAgent.selectAction(observation);
        if (timedAction != immediateAction) return false;

        ActionOutcome outcome;
        outcome.reward = -1.0f;
        outcome.terminal = false;
        outcome.outcome_tag = "temporal_decay_test";
        outcome.energy_after = 0.8f;
        timedAgent.applyOutcome(observation, timedAction, outcome, observation, 1.0f);
        immediateAgent.applyOutcome(observation, immediateAction, outcome, observation, 0.0f);

        const AffectiveState& timed = timedAgent.getAffectiveState();
        const AffectiveState& immediate = immediateAgent.getAffectiveState();
        const state& next = timedAgent.getCurrentState();
        return timed.stress < immediate.stress &&
               timed.arousal < immediate.arousal &&
               timed.engagement > immediate.engagement &&
               timed.affinity > immediate.affinity &&
               std::abs(next.affect.stress - timed.stress) < 1e-5f &&
               std::abs(next.affect.arousal - timed.arousal) < 1e-5f;
    }

    bool testPersistentFetchRetryAndCompletionCredit()
    {
        const EnvironmentObservation farRequest{1, 2, 1, 1, 0.56f};
        bool sawFailure = false;
        bool sawSuccess = false;
        bool failureKeepsTaskActive = false;
        bool successDoesNotResurrectTask = false;
        bool completionOutweighsSniff = false;

        for (unsigned int seed = 1; seed <= 300; ++seed)
        {
            std::mt19937 trialRng(seed);
            const EnvironmentStep attempt = applyEnvironmentAction("FETCH", farRequest, 0.8f, trialRng);
            if (!attempt.completed && !sawFailure)
            {
                sawFailure = true;
                const EnvironmentObservation retry = mergeTaskTransition(
                    farRequest, attempt.next_observation, farRequest);
                failureKeepsTaskActive = retry.task_status == 1 &&
                    retry.target_proximity == farRequest.target_proximity &&
                    isActionValidForObservation("FETCH", retry);
            }
            if (attempt.completed && !sawSuccess)
            {
                sawSuccess = true;
                const EnvironmentObservation afterCompletion = mergeTaskTransition(
                    farRequest, attempt.next_observation, farRequest);
                successDoesNotResurrectTask = afterCompletion.task_status == 0;
            }
        }

        const EnvironmentObservation nearRequest{1, 0, 1, 1, 0.82f};
        bool sawNearSuccess = false;
        for (unsigned int seed = 1; seed <= 100; ++seed)
        {
            std::mt19937 trialRng(seed);
            const EnvironmentStep fetch = applyEnvironmentAction("FETCH", nearRequest, 0.8f, trialRng);
            if (fetch.completed)
            {
                std::mt19937 sniffRng(seed);
                const EnvironmentStep sniff = applyEnvironmentAction("SNIFF", nearRequest, 0.8f, sniffRng);
                completionOutweighsSniff = fetch.reward > sniff.reward;
                sawNearSuccess = true;
                break;
            }
        }

        return sawFailure && sawSuccess && failureKeepsTaskActive &&
               successDoesNotResurrectTask && sawNearSuccess && completionOutweighsSniff;
    }

    template <typename Agent>
    EpisodeMetrics runEpisode(Agent &agent,
                              const std::vector<EnvironmentObservation> &observations,
                              const std::vector<float> &elapsedIntervals,
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
        float environmentStress = 0.2f;
        float userSatisfaction = 0.5f;
        EnvironmentObservation currentObservation = observations.front();
        for (std::size_t step = 0; step < observations.size(); ++step)
        {
            if (step > 0) {
                currentObservation = mergeTaskTransition(
                    observations[step - 1], currentObservation, observations[step]);
            }
            currentObservation.energy = environmentEnergy;
            const EnvironmentObservation observation = currentObservation;
            const std::string action = agent.selectAction(observation);
            const std::size_t stateKeyBeforeAction = agent.getCurrentState().encode();
            metrics.visited_states.insert(stateKeyBeforeAction);

            const float energyBefore = environmentEnergy;
            const float stressBefore = environmentStress;
            // Same seed per episode/step gives both agents paired environmental noise,
            // independent of how many stochastic actions either policy selected.
            const float elapsedSeconds = step + 1 == observations.size() ? 0.0f : elapsedIntervals[step];
            std::mt19937 transitionRng(static_cast<unsigned int>(episodeIndex + 1) * 7919u +
                                       static_cast<unsigned int>(step) * 104729u);
            const EnvironmentStep transition =
                applyEnvironmentAction(action, observation, environmentEnergy,
                                       environmentStress, userSatisfaction, transitionRng, elapsedSeconds);
            environmentEnergy = transition.energy_after;
            environmentStress = transition.stress_after;
            userSatisfaction = transition.user_satisfaction_after;
            const bool energyDepleted = transition.energy_after <= 0.0f;
            const bool terminal = step + 1 == observations.size() || energyDepleted;
            const float transitionReward = energyDepleted ? -1.0f : transition.reward;

            EnvironmentObservation nextObservation = transition.next_observation;
            if (!terminal) {
                nextObservation = mergeTaskTransition(
                    observation, transition.next_observation, observations[step + 1]);
            }
            // Scenario merging may replace observation fields, but physical energy
            // is environment-owned and must survive every merge.
            nextObservation.energy = transition.energy_after;

            ActionOutcome outcome;
            outcome.action_id = static_cast<int>(std::distance(
                companionActionSet().begin(),
                std::find(companionActionSet().begin(), companionActionSet().end(), action)));
            outcome.reward = transitionReward;
            outcome.terminal = terminal;
            outcome.outcome_tag = energyDepleted ? "energy_depleted" :
                                  (transition.completed ? "goal_completed" :
                                   (terminal ? "step_limit" : "interaction"));
            outcome.energy_after = transition.energy_after;

            agent.applyOutcome(observation, action, outcome, nextObservation, elapsedSeconds);

            metrics.total_reward += transitionReward;
            metrics.steps = static_cast<int>(step + 1);
            metrics.completed = metrics.completed || transition.completed;
            ++metrics.action_counts[action];
            // Do not count the intentionally taskless post-completion context
            // as evidence of a response to an active fetch request.
            if (observation.goal_state != 1 || observation.task_status == 1)
            {
                ++metrics.stimulus_checks;
                const std::string category = stimulusCategory(observation);
                ++metrics.stimulus_checks_by_type[category];
                if (stimulusResponseAligned(action, observation, energyBefore, stressBefore))
                {
                    ++metrics.stimulus_aligned_actions;
                    ++metrics.stimulus_aligned_by_type[category];
                }
            }

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
            row["simulated_elapsed_seconds"] = elapsedSeconds;
            if constexpr (std::is_same_v<Agent, ProposedAgent>) {
                row["affect_arousal_after_transition"] = agent.getAffectiveState().arousal;
                row["affect_stress_after_transition"] = agent.getAffectiveState().stress;
                row["affect_engagement_after_transition"] = agent.getAffectiveState().engagement;
                row["affect_affinity_after_transition"] = agent.getAffectiveState().affinity;
            }
            row["environment_stress_after"] = transition.stress_after;
            row["user_satisfaction_after"] = transition.user_satisfaction_after;
            row["action"] = action;
            row["encoded_state"] = stateKeyBeforeAction;
            row["next_encoded_state"] = agent.getCurrentState().encode();
            row["reward"] = transitionReward;
            row["terminal"] = terminal;
            row["terminal_reason"] = energyDepleted ? "energy_depleted" :
                                     (step + 1 == observations.size() ? "step_limit" : "none");
            row["energy_depleted"] = energyDepleted;
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
        result["stimulus_checks"] = metrics.stimulus_checks;
        result["stimulus_aligned_actions"] = metrics.stimulus_aligned_actions;
        result["stimulus_response_alignment_rate"] =
            metrics.stimulus_checks > 0
                ? static_cast<double>(metrics.stimulus_aligned_actions) / metrics.stimulus_checks
                : 0.0;
        result["stimulus_checks_by_type"] = metrics.stimulus_checks_by_type;
        result["stimulus_aligned_by_type"] = metrics.stimulus_aligned_by_type;
        return result;
    }
} // namespace

bool runTrainingExperiment(const std::string &mode,
                           int episodes,
                           int steps,
                           unsigned int seed,
                           const std::string &outputDirectory,
                           bool resumeTraining)
{
    const bool trainingMode = mode == "train" || mode == "resume-training";
    if (mode != "train" && mode != "resume-training" && mode != "validate" && mode != "compare")
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

    if ((mode == "resume-training" || mode == "validate" || mode == "compare") &&
        (!fs::exists(baselinePolicy) || !fs::exists(proposedPolicy)))
    {
        std::cerr << "No trained policies found. Run --train first; expected "
                  << baselinePolicy << " and " << proposedPolicy << ".\n";
        return false;
    }

    EnvironmentConfig config;
    config.seed = static_cast<int>(seed);
    BaselineAgent baseline(config);
    ProposedAgent proposed(config);

    if (mode == "resume-training" || mode == "validate" || mode == "compare")
    {
        baseline.loadPersistentState(baselinePolicy.string());
        proposed.loadPersistentState(proposedPolicy.string());
    }
    if (mode == "validate" || mode == "compare")
    {
        baseline.setLearningEnabled(false);
        proposed.setLearningEnabled(false);
        baseline.setExplorationRate(0.0f);
        proposed.setExplorationRate(0.0f);
    }

    rotateArtifactForComparison(outDir / (mode + "_baseline_steps.jsonl"));
    rotateArtifactForComparison(outDir / (mode + "_proposed_steps.jsonl"));
    rotateArtifactForComparison(outDir / (mode + "_summary.json"));
    std::ofstream baselineLog(outDir / (mode + "_baseline_steps.jsonl"));
    std::ofstream proposedLog(outDir / (mode + "_proposed_steps.jsonl"));
    if (!baselineLog || !proposedLog)
    {
        std::cerr << "Unable to open experiment log files in " << outDir << '\n';
        return false;
    }

    const unsigned int environmentSeed = trainingMode ? seed : seed + 1000003u;
    std::mt19937 environmentRng(environmentSeed);
    nlohmann::json summary;
    summary["mode"] = mode;
    summary["training_initialization"] = mode == "resume-training" ? "loaded_existing_qtables" :
                                         mode == "train" ? "fresh_qtables" : "loaded_evaluation_checkpoint";
    summary["resume_training_requested"] = resumeTraining || mode == "resume-training";
    summary["seed"] = seed;
    summary["environment_seed"] = environmentSeed;
    summary["episodes"] = episodes;
    summary["max_steps_per_episode"] = steps;
    summary["reward_model"] = "synthetic_v4_graded_context_fit";
    summary["affect_decay_during_training"] = true;
    summary["simulated_seconds_between_decisions_min"] = kMinSecondsPerTrainingStep;
    summary["simulated_seconds_between_decisions_max"] = kMaxSecondsPerTrainingStep;
    summary["temporal_model"] = "Each non-terminal decision uses a seeded elapsed-time interval sampled uniformly from 0.5 to 5 simulated seconds. Paired agents share intervals. Physical energy receives action effort costs and passive time-based drain (0.0025 per second normally; 0.0008 while resting). The shared energy value is observable to both agents and included in both state representations. Proposed-agent affect decays over the interval before the next state is built for Q-learning.";
    summary["scenario_pattern"] = {"social", "fetch_near", "fetch_near_follow_up", "disturbance", "rest", "positive_social", "fetch_far", "fetch_far_follow_up", "fetch_far_retry", "food_offer", "play_invitation", "negative_feedback", "ignored_request", "fetch_at_feeding_time"};
    summary["note"] = "Training uses repeated contextual scenarios, including fetch during feeding time. Rewards combine graded context fit, drive relief, successful task completion, effort, and satisfaction-weighted disappointment; non-compliance alone is not treated as automatically wrong. Stimulus alignment is a synthetic engineering diagnostic, not perceived believability.";

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
        std::vector<float> elapsedIntervals;
        observations.reserve(static_cast<std::size_t>(steps));
        elapsedIntervals.reserve(static_cast<std::size_t>(steps));
        std::uniform_real_distribution<float> elapsedDist(
            kMinSecondsPerTrainingStep, kMaxSecondsPerTrainingStep);
        for (int step = 0; step < steps; ++step)
        {
            observations.push_back(makeObservation(environmentRng, step));
            elapsedIntervals.push_back(elapsedDist(environmentRng));
        }

        const float epsilon = trainingMode
                                  ? std::max(0.05f, 0.30f * (1.0f - static_cast<float>(episode) / static_cast<float>(episodes)))
                                  : 0.0f;
        const bool learning = trainingMode;

        const EpisodeMetrics baselineMetrics = runEpisode(
            baseline, observations, elapsedIntervals, learning, epsilon, baselineLog, "baseline", episode);
        const EpisodeMetrics proposedMetrics = runEpisode(
            proposed, observations, elapsedIntervals, learning, epsilon, proposedLog, "proposed", episode);

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

    if (trainingMode)
    {
        rotateArtifactForComparison(baselinePolicy);
        rotateArtifactForComparison(proposedPolicy);
        rotateArtifactForComparison(outDir / "proposed.qtable.affect.json");
        rotateArtifactForComparison(outDir / "proposed.qtable.memory.json");
        baseline.savePersistentState(baselinePolicy.string());
        proposed.savePersistentState(proposedPolicy.string());
    }

    const auto buildStimulusAudit = [](const nlohmann::json &episodes) {
        nlohmann::json audit = nlohmann::json::object();
        int checks = 0;
        int aligned = 0;
        for (const auto &episode : episodes)
        {
            checks += episode["stimulus_checks"].get<int>();
            aligned += episode["stimulus_aligned_actions"].get<int>();
            for (auto it = episode["stimulus_checks_by_type"].begin();
                 it != episode["stimulus_checks_by_type"].end(); ++it)
            {
                const std::string category = it.key();
                if (!audit.contains(category))
                    audit[category] = {{"checks", 0}, {"aligned", 0}};
                audit[category]["checks"] = audit[category]["checks"].get<int>() + it.value().get<int>();
            }
            for (auto it = episode["stimulus_aligned_by_type"].begin();
                 it != episode["stimulus_aligned_by_type"].end(); ++it)
            {
                const std::string category = it.key();
                if (!audit.contains(category))
                    audit[category] = {{"checks", 0}, {"aligned", 0}};
                audit[category]["aligned"] = audit[category]["aligned"].get<int>() + it.value().get<int>();
            }
        }
        for (auto it = audit.begin(); it != audit.end(); ++it)
        {
            const int categoryChecks = it.value()["checks"].get<int>();
            const int categoryAligned = it.value()["aligned"].get<int>();
            it.value()["alignment_rate"] = categoryChecks > 0
                ? static_cast<double>(categoryAligned) / categoryChecks : 0.0;
        }
        return std::pair<nlohmann::json, std::pair<int, int>>{
            audit, {checks, aligned}};
    };

    const auto baselineAudit = buildStimulusAudit(baselineEpisodes);
    const auto proposedAudit = buildStimulusAudit(proposedEpisodes);
    summary["baseline"]["stimulus_checks"] = baselineAudit.second.first;
    summary["baseline"]["stimulus_aligned_actions"] = baselineAudit.second.second;
    summary["baseline"]["stimulus_response_alignment_rate"] =
        baselineAudit.second.first > 0
            ? static_cast<double>(baselineAudit.second.second) / baselineAudit.second.first : 0.0;
    summary["baseline"]["stimulus_response_audit_by_type"] = baselineAudit.first;
    summary["proposed"]["stimulus_checks"] = proposedAudit.second.first;
    summary["proposed"]["stimulus_aligned_actions"] = proposedAudit.second.second;
    summary["proposed"]["stimulus_response_alignment_rate"] =
        proposedAudit.second.first > 0
            ? static_cast<double>(proposedAudit.second.second) / proposedAudit.second.first : 0.0;
    summary["proposed"]["stimulus_response_audit_by_type"] = proposedAudit.first;
    summary["stimulus_alignment_note"] =
        "Heuristic engineering diagnostic for broad stimulus/action compatibility only; not a validated animal-behaviour model or measure of perceived believability.";

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


bool runIdlePolicyDiagnostics(const std::string& policyDirectory,
                              const std::string& outputDirectory,
                              unsigned int seed)
{
    namespace fs = std::filesystem;
    const fs::path policyDir(policyDirectory);
    const fs::path baselinePath = policyDir / "baseline.qtable";
    const fs::path proposedPath = policyDir / "proposed.qtable";
    if (!fs::exists(baselinePath) || !fs::exists(proposedPath)) {
        std::cerr << "Idle diagnostics require trained policies at "
                  << baselinePath << " and " << proposedPath
                  << ". Run --train first.\n";
        return false;
    }

    fs::create_directories(outputDirectory);
    const fs::path logPath = fs::path(outputDirectory) / "idle-policy-diagnostics.jsonl";
    const fs::path summaryPath = fs::path(outputDirectory) / "idle-policy-diagnostics-summary.json";
    rotateArtifactForComparison(logPath);
    rotateArtifactForComparison(summaryPath);
    std::ofstream log(logPath);
    if (!log) {
        std::cerr << "Unable to write idle diagnostics to " << logPath << '\n';
        return false;
    }

    EnvironmentConfig config;
    config.seed = static_cast<int>(seed);
    BaselineAgent baseline(config);
    ProposedAgent proposed(config);
    baseline.loadPersistentState(baselinePath.string());
    proposed.loadPersistentState(proposedPath.string());
    baseline.setLearningEnabled(false);
    proposed.setLearningEnabled(false);
    baseline.setExplorationRate(0.0f);
    proposed.setExplorationRate(0.0f);

    // Mirrors the UI's ambient observation after reset: no task, no user
    // stimulus, no food, the ball nearby enough to be a medium-distance target.
    EnvironmentObservation ambient;
    ambient.goal_state = 2;
    ambient.target_proximity = 1;
    ambient.user_status = 0;
    ambient.task_status = 0;
    ambient.signal_strength = 0.0f;
    ambient.food_available = false;
    ambient.play_available = true;

    const std::vector<float> checkpoints{0.0f, 1.0f, 5.0f, 10.0f, 30.0f};
    nlohmann::json summary;
    summary["mode"] = "fixed_context_temporal_sweep";
    summary["policy_directory"] = policyDir.string();
    summary["seed"] = seed;
    summary["ambient_observation"] = {
        {"goal_state", ambient.goal_state},
        {"target_proximity", ambient.target_proximity},
        {"user_status", ambient.user_status},
        {"task_status", ambient.task_status},
        {"signal_strength", ambient.signal_strength},
        {"food_available", ambient.food_available},
        {"play_available", ambient.play_available}
    };
    summary["checkpoints_seconds"] = checkpoints;
    summary["notes"] = nlohmann::json::array({
        "Loads the saved Q-tables and proposed-agent sidecar state, disables exploration and learning, and holds the UI-like ambient observation constant.",
        "The fixed-context sweep isolates changes caused by elapsed-time affect decay; it does not claim to reproduce animation/behaviour completion timing.",
        "The UI starts another autonomous decision only after the 5-second idle timer expires while no behaviour is active. Behaviour duration depends on the selected action and scene geometry, so the snapshot sweep is diagnostic evidence rather than an exact full-UI replay.",
        "Q-values are logged for every valid proposed-agent action, alongside state encoding, contextual fallback, policy choice, final post-override action, affect, and memory count.",
        "Synthetic policy diagnostics do not measure perceived believability."
    });

    nlohmann::json snapshots = nlohmann::json::array();
    std::map<std::string, int> baselineActionCounts;
    std::map<std::string, int> proposedActionCounts;
    std::size_t priorBaselineState = 0;
    std::size_t priorProposedState = 0;
    std::string priorBaselineAction;
    std::string priorProposedAction;
    int baselineStateChanges = 0;
    int proposedStateChanges = 0;
    int baselineActionChanges = 0;
    int proposedActionChanges = 0;
    float previousSeconds = 0.0f;

    for (const float seconds : checkpoints) {
        const float delta = seconds - previousSeconds;
        if (delta > 0.0f) proposed.advanceTime(delta);
        previousSeconds = seconds;

        const std::string baselineAction = baseline.selectAction(ambient);
        const std::string proposedAction = proposed.selectAction(ambient);
        const state& baselineState = baseline.getCurrentState();
        const state& proposedState = proposed.getCurrentState();
        const ActionDecisionTrace& trace = proposed.getLastDecisionTrace();
        ++baselineActionCounts[baselineAction];
        ++proposedActionCounts[proposedAction];

        if (!snapshots.empty()) {
            if (baselineState.encode() != priorBaselineState) ++baselineStateChanges;
            if (proposedState.encode() != priorProposedState) ++proposedStateChanges;
            if (baselineAction != priorBaselineAction) ++baselineActionChanges;
            if (proposedAction != priorProposedAction) ++proposedActionChanges;
        }
        priorBaselineState = baselineState.encode();
        priorProposedState = proposedState.encode();
        priorBaselineAction = baselineAction;
        priorProposedAction = proposedAction;

        nlohmann::json qValues = nlohmann::json::array();
        for (const auto& item : trace.valid_action_q_values) {
            qValues.push_back({{"action", item.first}, {"q_value", item.second}});
        }

        nlohmann::json row;
        row["elapsed_seconds"] = seconds;
        row["baseline"] = {
            {"action", baselineAction},
            {"encoded_state", baselineState.encode()}
        };
        row["proposed"] = {
            {"action", proposedAction},
            {"encoded_state", proposedState.encode()},
            {"contextual_fallback", trace.contextual_fallback},
            {"policy_action_before_overrides", trace.policy_action},
            {"q_table_state_present", trace.q_table_state_present},
            {"q_table_has_valid_action_values", trace.q_table_has_valid_action_values},
            {"final_action_after_overrides", trace.final_action},
            {"general_preference_override", trace.general_preference_override},
            {"rejection_override_applied", trace.rejection_override_applied},
            {"memory_record_count", trace.memory_record_count},
            {"affect", {
                {"valence", static_cast<int>(trace.affect.valence)},
                {"energy", static_cast<int>(trace.affect.energy)},
                {"arousal", trace.affect.arousal},
                {"stress", trace.affect.stress},
                {"engagement", trace.affect.engagement},
                {"affinity", trace.affect.affinity}
            }},
            {"valid_action_q_values", qValues}
        };
        snapshots.push_back(row);
        log << row.dump() << '\n';
    }
    log.close();

    summary["snapshot_count"] = snapshots.size();
    summary["snapshots"] = snapshots;
    summary["baseline_action_counts"] = baselineActionCounts;
    summary["proposed_action_counts"] = proposedActionCounts;
    summary["baseline_state_changes_between_checkpoints"] = baselineStateChanges;
    summary["proposed_state_changes_between_checkpoints"] = proposedStateChanges;
    summary["baseline_action_changes_between_checkpoints"] = baselineActionChanges;
    summary["proposed_action_changes_between_checkpoints"] = proposedActionChanges;
    summary["diagnostic_interpretation"] = {
        {"state_changes", "If affect values change but encoded state does not, the state discretization may be hiding temporal change at these checkpoints."},
        {"action_changes", "If encoded state changes but selected action does not, inspect Q-value ranking, action masking, and deterministic tie/fallback behaviour."},
        {"overrides", "If policy_action_before_overrides differs from final_action_after_overrides, a memory/preference/rejection override rather than the Q-table determined the displayed action."},
        {"single_action", "A repeated action across this small sweep is a signal to inspect, not proof of a global policy collapse; use the per-action Q-values and wider context tests."}
    };

    std::ofstream summaryFile(summaryPath);
    if (!summaryFile) {
        std::cerr << "Unable to write idle diagnostic summary to " << summaryPath << '\n';
        return false;
    }
    summaryFile << summary.dump(2) << '\n';
    std::cout << "Idle policy diagnostics complete.\n"
              << "Snapshots: " << logPath << '\n'
              << "Summary: " << summaryPath << '\n'
              << "Baseline actions changed between checkpoints: " << baselineActionChanges << '\n'
              << "Proposed actions changed between checkpoints: " << proposedActionChanges << '\n';
    return true;
}



bool runTemporalStimulusDiagnostics(const std::string& policyDirectory,
                                    const std::string& outputDirectory,
                                    unsigned int seed)
{
    namespace fs = std::filesystem;
    const fs::path policyDir(policyDirectory);
    const fs::path baselinePath = policyDir / "baseline.qtable";
    const fs::path proposedPath = policyDir / "proposed.qtable";
    if (!fs::exists(baselinePath) || !fs::exists(proposedPath)) {
        std::cerr << "Temporal stimulus diagnostics require trained policies at "
                  << baselinePath << " and " << proposedPath
                  << ". Run --train first.\n";
        return false;
    }

    struct StimulusPhase {
        std::string name;
        EnvironmentObservation observation;
        int decisions;
    };

    // Each phase is held constant for six decisions, five simulated seconds apart.
    // The final phase returns to an unstimulated context to test whether behaviour
    // returns to its ambient distribution after the agent has experienced stimuli.
    EnvironmentObservation ambient;
    ambient.goal_state = 2;
    ambient.target_proximity = 1;
    ambient.user_status = 0;
    ambient.task_status = 0;
    ambient.signal_strength = 0.0f;
    ambient.food_available = false;
    ambient.play_available = true;

    const std::vector<StimulusPhase> phases = {
        {"ambient_initial", ambient, 6},
        {"social_contact", EnvironmentObservation{0, 1, 0, 0, 0.45f}, 6},
        {"disturbance", EnvironmentObservation{2, 0, 0, 0, 0.95f}, 6},
        {"quiet_no_stimulus", EnvironmentObservation{2, 2, 0, 0, 0.20f}, 6},
        {"play_invitation", EnvironmentObservation{0, 1, 1, 0, 0.70f, false, true}, 6},
        {"ambient_return", ambient, 6}
    };

    fs::create_directories(outputDirectory);
    const fs::path logPath = fs::path(outputDirectory) / "temporal-stimulus-diagnostics.jsonl";
    const fs::path summaryPath = fs::path(outputDirectory) / "temporal-stimulus-diagnostics-summary.json";
    rotateArtifactForComparison(logPath);
    rotateArtifactForComparison(summaryPath);
    std::ofstream log(logPath);
    if (!log) {
        std::cerr << "Unable to write temporal stimulus diagnostics to " << logPath << '\n';
        return false;
    }

    EnvironmentConfig config;
    config.seed = static_cast<int>(seed);
    BaselineAgent baseline(config);
    ProposedAgent proposed(config);
    baseline.loadPersistentState(baselinePath.string());
    proposed.loadPersistentState(proposedPath.string());
    baseline.setLearningEnabled(false);
    proposed.setLearningEnabled(false);
    baseline.setExplorationRate(0.0f);
    proposed.setExplorationRate(0.0f);

    nlohmann::json summary;
    summary["mode"] = "paired_temporal_stimulus_policy_diagnostic";
    summary["policy_directory"] = policyDir.string();
    summary["seed"] = seed;
    summary["learning_enabled"] = false;
    summary["exploration_enabled"] = false;
    summary["outcome_feedback_applied"] = false;
    summary["decision_interval_seconds"] = 5.0f;
    summary["phases"] = nlohmann::json::array();
    summary["notes"] = nlohmann::json::array({
        "Both policies receive the same fixed observation in each phase and the same phase order.",
        "Policies are evaluated without learning, exploration, reward updates, or environment transitions. Only the proposed agent advances its time-dependent affect between decisions.",
        "Action distributions describe policy response to this scripted sequence; they are not evidence of perceived believability.",
        "Q-table coverage and contextual fallback are logged to distinguish learned choices from fallback behaviour."
    });

    nlohmann::json snapshots = nlohmann::json::array();
    nlohmann::json phaseSummaries = nlohmann::json::array();
    std::map<std::string, std::map<std::string, int>> baselineCountsByPhase;
    std::map<std::string, std::map<std::string, int>> proposedCountsByPhase;
    std::map<std::string, int> baselineChangesByPhase;
    std::map<std::string, int> proposedChangesByPhase;
    std::string previousBaselineAction;
    std::string previousProposedAction;
    float elapsedSeconds = 0.0f;
    int globalStep = 0;

    for (const auto& phase : phases) {
        std::string phasePreviousBaseline;
        std::string phasePreviousProposed;
        int phaseBaselineChanges = 0;
        int phaseProposedChanges = 0;
        nlohmann::json phaseObservation = {
            {"goal_state", phase.observation.goal_state},
            {"target_proximity", phase.observation.target_proximity},
            {"user_status", phase.observation.user_status},
            {"task_status", phase.observation.task_status},
            {"signal_strength", phase.observation.signal_strength},
            {"food_available", phase.observation.food_available},
            {"play_available", phase.observation.play_available}
        };

        for (int decision = 0; decision < phase.decisions; ++decision) {
            if (globalStep > 0) {
                baseline.setLearningEnabled(false);
                proposed.advanceTime(5.0f);
                elapsedSeconds += 5.0f;
            }

            EnvironmentObservation observation = phase.observation;
            const std::string baselineAction = baseline.selectAction(observation);
            const std::string proposedAction = proposed.selectAction(observation);
            const auto& proposedTrace = proposed.getLastDecisionTrace();

            ++baselineCountsByPhase[phase.name][baselineAction];
            ++proposedCountsByPhase[phase.name][proposedAction];
            if (!phasePreviousBaseline.empty() && baselineAction != phasePreviousBaseline)
                ++phaseBaselineChanges;
            if (!phasePreviousProposed.empty() && proposedAction != phasePreviousProposed)
                ++phaseProposedChanges;

            nlohmann::json row;
            row["step"] = globalStep;
            row["elapsed_seconds"] = elapsedSeconds;
            row["phase"] = phase.name;
            row["phase_decision"] = decision;
            row["observation"] = phaseObservation;
            row["baseline"] = {
                {"action", baselineAction},
                {"encoded_state", baseline.getCurrentState().encode()},
                {"action_changed_from_previous_decision",
                 !previousBaselineAction.empty() && baselineAction != previousBaselineAction}
            };
            row["proposed"] = {
                {"action", proposedAction},
                {"encoded_state", proposed.getCurrentState().encode()},
                {"action_changed_from_previous_decision",
                 !previousProposedAction.empty() && proposedAction != previousProposedAction},
                {"contextual_fallback", proposedTrace.contextual_fallback},
                {"policy_action_before_overrides", proposedTrace.policy_action},
                {"final_action_after_overrides", proposedTrace.final_action},
                {"q_table_state_present", proposedTrace.q_table_state_present},
                {"q_table_has_valid_action_values", proposedTrace.q_table_has_valid_action_values},
                {"valid_action_q_values", nlohmann::json::array()},
                {"affect", {
                    {"valence", static_cast<int>(proposedTrace.affect.valence)},
                    {"energy", static_cast<int>(proposedTrace.affect.energy)},
                    {"arousal", proposedTrace.affect.arousal},
                    {"stress", proposedTrace.affect.stress},
                    {"engagement", proposedTrace.affect.engagement},
                    {"affinity", proposedTrace.affect.affinity}
                }},
                {"memory_record_count", proposedTrace.memory_record_count},
                {"general_preference_override", proposedTrace.general_preference_override},
                {"rejection_override_applied", proposedTrace.rejection_override_applied}
            };
            for (const auto& item : proposedTrace.valid_action_q_values) {
                row["proposed"]["valid_action_q_values"].push_back(
                    {{"action", item.first}, {"q_value", item.second}});
            }
            snapshots.push_back(row);
            log << row.dump() << '\n';

            phasePreviousBaseline = baselineAction;
            phasePreviousProposed = proposedAction;
            previousBaselineAction = baselineAction;
            previousProposedAction = proposedAction;
            ++globalStep;
        }

        baselineChangesByPhase[phase.name] = phaseBaselineChanges;
        proposedChangesByPhase[phase.name] = phaseProposedChanges;
        phaseSummaries.push_back({
            {"phase", phase.name},
            {"decisions", phase.decisions},
            {"observation", phaseObservation},
            {"baseline_action_counts", baselineCountsByPhase[phase.name]},
            {"proposed_action_counts", proposedCountsByPhase[phase.name]},
            {"baseline_action_changes_within_phase", phaseBaselineChanges},
            {"proposed_action_changes_within_phase", phaseProposedChanges}
        });
    }

    summary["total_decisions"] = globalStep;
    summary["total_simulated_seconds_between_decisions"] = elapsedSeconds;
    summary["phase_summaries"] = phaseSummaries;
    summary["snapshots"] = snapshots;
    summary["baseline_action_counts_by_phase"] = baselineCountsByPhase;
    summary["proposed_action_counts_by_phase"] = proposedCountsByPhase;
    summary["baseline_action_changes_within_phase"] = baselineChangesByPhase;
    summary["proposed_action_changes_within_phase"] = proposedChangesByPhase;

    std::ofstream summaryFile(summaryPath);
    if (!summaryFile) {
        std::cerr << "Unable to write temporal stimulus summary to " << summaryPath << '\n';
        return false;
    }
    summaryFile << summary.dump(2) << '\n';
    std::cout << "Temporal stimulus diagnostics complete.\n"
              << "Snapshots: " << logPath << '\n'
              << "Summary: " << summaryPath << '\n';
    return true;
}

bool runTrainingSelfTests()
{
    const bool energyDrainPassed = testPassiveEnergyDrain();
    std::cout << "Shared passive energy drain across elapsed simulation time: "
              << (energyDrainPassed ? "PASS" : "FAIL") << '\n';
    const bool recoveryPriorityPassed = testCriticalEnergyPrioritizesRecovery();
    std::cout << "Critical energy restricts both policies to recovery actions: "
              << (recoveryPriorityPassed ? "PASS" : "FAIL") << '\n';
    const bool depletionTransitionPassed = testEnergyDepletionTransition();
    std::cout << "Energy can reach the terminal depleted state: "
              << (depletionTransitionPassed ? "PASS" : "FAIL") << '\n';
    const bool temporalDecayPassed = testTrainingAppliesElapsedAffectDecay();
    std::cout << "Training transition applies elapsed-time affect decay before next-state construction: "
              << (temporalDecayPassed ? "PASS" : "FAIL") << '\n';
    const bool transitionsPassed = testEnvironmentActionTransitions();
    const bool retryPassed = testPersistentFetchRetryAndCompletionCredit();
    const bool taskPriorityPassed = testTaskPriorityReward();
    const bool completionContextPassed = testSuccessfulFetchAdvancesContext();
    std::cout << "Action-dependent environment transitions: " << (transitionsPassed ? "PASS" : "FAIL") << '\n';
    std::cout << "Persistent fetch retry and completion credit: " << (retryPassed ? "PASS" : "FAIL") << '\n';
    std::cout << "Task priority over unproductive sniffing: " << (taskPriorityPassed ? "PASS" : "FAIL") << '\n';
    std::cout << "Successful fetch advances to post-completion context: " << (completionContextPassed ? "PASS" : "FAIL") << '\n';
    return energyDrainPassed && recoveryPriorityPassed && depletionTransitionPassed &&
           temporalDecayPassed && transitionsPassed && retryPassed &&
           taskPriorityPassed && completionContextPassed;
}
