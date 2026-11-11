#include "training.h"

#include "Components/State-Action/state-action.h"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <cstdint>
#include <system_error>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
#include <set>
#include <sstream>
#include <limits>
#include <vector>
#include <utility>
#include <type_traits>

namespace
{
    // Sample varied but bounded elapsed time between decisions. Paired agents
    // receive the same interval sequence within each episode.
    constexpr int kDetailedStepLogEpisodes = std::numeric_limits<int>::max();
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
                      << " as " << backupPath << ": " << ec.message() << '\n';
    }

    struct EpisodeMetrics
    {
        float total_reward = 0.0f;
        int steps = 0;
        bool completed = false;
        int synthetic_positive_responses = 0;
        int synthetic_negative_responses = 0;
        int synthetic_no_responses = 0;
        int synthetic_requests_observed = 0;
        int synthetic_requests_acknowledged = 0;
        int synthetic_requests_pending_at_end = 0;
        int stimulus_checks = 0;
        int stimulus_aligned_actions = 0;
        std::map<std::string, int> stimulus_checks_by_type;
        std::map<std::string, int> stimulus_aligned_by_type;
        std::map<std::string, int> action_counts;
        std::set<std::size_t> visited_states;
        std::map<std::string, int> state_action_visits;
        std::set<std::string> valid_state_action_pairs_available;
        int q_table_supported_decisions = 0;
        int fallback_decisions = 0;

        int insist_predicate_eligible_decisions = 0;
        int insist_available_decisions = 0;
        int insist_eligibility_mask_mismatches = 0;
        int insist_policy_selections = 0;
        int insist_final_selections = 0;
        int insist_execution_attempts = 0;
        int insist_selected_when_unavailable = 0;
        int insist_attempts_with_meaningful_response = 0;
        int insist_attempts_without_meaningful_response = 0;

        int repetition_decisions = 0;
        int repetition_penalty_trigger_count = 0;
        int repetition_response_reset_count = 0;
        int repetition_category_change_count = 0;
        int repetition_max_consecutive_count = 0;
        double repetition_penalty_total = 0.0;
        double repetition_penalty_max = 0.0;
        std::vector<double> q_value_spreads;
        std::vector<double> repeated_action_q_gaps;
        std::vector<double> repetition_penalty_samples;
    };

    struct EnvironmentStep
    {
        EnvironmentObservation next_observation;
        float reward = 0.0f;
        float energy_after = 0.8f;
        float stress_after = 0.2f;
        float user_satisfaction_after = 0.5f;
        bool completed = false;
    };

    struct SyntheticUserProfile { float responsiveness=0.75f; int patience=3; float social_openness=0.70f; };
    struct SyntheticUserResponse {
        int status=0; int no_response_count=0; bool request_pending=false;
        bool request_acknowledged=false; std::string category="silence";
    };
    float computeTrainingEpsilon(int episode, int totalEpisodes)
    {
        if (totalEpisodes <= 0)
            return 0.0f;
        constexpr float kPi = 3.14159265358979323846f;
        const float e = static_cast<float>(std::clamp(episode, 0, totalEpisodes - 1));
        const float epsilonMax = 0.50f;
        const float epsilonMin = 0.05f;
        return epsilonMin + 0.5f * (epsilonMax - epsilonMin) *
               (1.0f + std::cos((e * kPi) / static_cast<float>(totalEpisodes)));
    }

    float computeIntrinsicBonus(int visitCount, float eta = 0.02f)
    {
        return eta / std::sqrt(static_cast<float>(visitCount) + 1.0f);
    }

    SyntheticUserProfile makeSyntheticUserProfile(unsigned int seed) {
        std::mt19937 rng(seed ^ 0x9e3779b9u);
        std::uniform_real_distribution<float> response(0.50f,0.95f), openness(0.40f,0.95f);
        std::uniform_int_distribution<int> patience(2,5);
        return {response(rng),patience(rng),openness(rng)};
    }
    SyntheticUserResponse simulateSyntheticUserResponse(
        const EnvironmentObservation& observation,const std::string& action,
        const EnvironmentStep& transition,const SyntheticUserProfile& profile,float draw) {
        SyntheticUserResponse r;
        const bool pending=observation.goal_state==1 && observation.task_status==1;
        r.request_pending=pending;
        if(pending) {
            r.no_response_count=std::min(3,observation.no_response_count+1);
            if(transition.completed && action=="FETCH") {
                r.status=1; r.no_response_count=0; r.request_pending=false;
                r.request_acknowledged=true; r.category="fetch_completed";
            } else if(action=="INSIST") {
                const float threshold=std::clamp(profile.responsiveness*(0.72f-0.06f*float(profile.patience-2)),0.20f,0.70f);
                if(draw<threshold) { r.status=1; r.no_response_count=0; r.request_acknowledged=true; r.category="request_acknowledged"; }
                else if(draw>0.94f) { r.status=2; r.no_response_count=0; r.category="request_rejected"; }
                else r.category="request_silence";
            } else if(action=="APPROACH_FOLLOW" &&
                      transition.next_observation.target_proximity<observation.target_proximity &&
                      draw<profile.responsiveness*0.42f) {
                r.status=1; r.no_response_count=0; r.category="approach_acknowledged";
            } else if(draw>0.975f) {
                r.status=2; r.no_response_count=0; r.category="request_rejected";
            } else r.category="request_silence";
            return r;
        }
        if((action=="GREETING"||action=="NUZZLE"||action=="PLAY") && observation.goal_state==0) {
            const float threshold=std::clamp(profile.responsiveness*profile.social_openness,0.15f,0.88f);
            if(draw<threshold) { r.status=1; r.category="social_acknowledgement"; }
            else if(draw>0.96f) { r.status=2; r.category="social_rejection"; }
            else r.category="social_silence";
        } else if(action=="EAT" && observation.food_available) {
            if(draw<profile.responsiveness*0.80f) { r.status=1; r.category="food_acknowledgement"; }
        } else if(action=="SLEEP_LAY" && (observation.goal_state==2||observation.energy<0.35f)) {
            if(draw<profile.responsiveness*0.62f) { r.status=1; r.category="rest_acknowledgement"; }
        } else if(observation.goal_state==2 && observation.signal_strength>=0.80f &&
                  (action=="BARK"||action=="HIDE_AVOID"||action=="ACT_CONFUSED"||action=="WHINE")) {
            if(draw<profile.responsiveness*0.55f) { r.status=1; r.category="disturbance_response"; }
        }
        if(r.status==0 && r.category=="silence") r.category="no_contextual_response";
        return r;
    }
    std::vector<float> makeSyntheticUserDraws(unsigned int seed,std::size_t count) {
        std::mt19937 rng(seed ^ 0x85ebca6bu);
        std::uniform_real_distribution<float> dist(0.0f,1.0f);
        std::vector<float> values; values.reserve(count);
        for(std::size_t i=0;i<count;++i) values.push_back(dist(rng));
        return values;
    }
    bool testSyntheticUserDeterminismAndLifecycle() {
        const auto a=makeSyntheticUserProfile(9123u), b=makeSyntheticUserProfile(9123u), c=makeSyntheticUserProfile(9124u);
        if(a.responsiveness!=b.responsiveness||a.patience!=b.patience||a.social_openness!=b.social_openness) return false;
        if(a.responsiveness==c.responsiveness&&a.patience==c.patience&&a.social_openness==c.social_openness) return false;
        EnvironmentObservation request{1,0,0,1,0.82f}; request.no_response_count=1;
        EnvironmentStep failed; failed.next_observation=request;
        const auto silence=simulateSyntheticUserResponse(request,"FETCH",failed,a,0.5f);
        if(silence.status!=0||silence.no_response_count!=2||!silence.request_pending) return false;
        EnvironmentStep success=failed; success.completed=true; success.next_observation.task_status=0;
        const auto ack=simulateSyntheticUserResponse(request,"FETCH",success,a,0.5f);
        if(ack.status!=1||ack.no_response_count!=0||ack.request_pending||!ack.request_acknowledged) return false;
        const auto draws1=makeSyntheticUserDraws(777u,12), draws2=makeSyntheticUserDraws(777u,12);
        if(draws1!=draws2) return false;
        const auto r1=simulateSyntheticUserResponse(request,"INSIST",failed,a,draws1[0]);
        const auto r2=simulateSyntheticUserResponse(request,"INSIST",failed,b,draws2[0]);
        return r1.status==r2.status&&r1.no_response_count==r2.no_response_count&&r1.category==r2.category;
    }

    // Scenario blocks preserve meaningful short sequences (fetch retries and
    // ignored requests) while their order and inclusion vary between episodes.
    std::vector<EnvironmentObservation> makeScenarioSchedule(std::mt19937 &rng,
                                                              int requestedSteps)
    {
        std::uniform_int_distribution<int> socialDist(0, 1);
        std::uniform_int_distribution<int> nearDist(0, 1);
        std::uniform_real_distribution<float> signalDist(0.38f, 0.96f);
        std::bernoulli_distribution includeScenario(0.88);

        EnvironmentObservation ignoredFirst{1, 1, 0, 1, 0.82f};
        EnvironmentObservation ignoredSecond = ignoredFirst;
        EnvironmentObservation ignoredThird = ignoredFirst;
        ignoredFirst.no_response_count = 0;
        ignoredSecond.no_response_count = 1;
        ignoredThird.no_response_count = 2;
        std::vector<std::vector<EnvironmentObservation>> blocks = {
            {{0, 1, socialDist(rng), 0, 0.45f}},
            {{1, nearDist(rng), 1, 1, 0.82f},
             {1, 1, 1, 1, 0.82f}},
            {{2, 0, 0, 0, signalDist(rng)}},
            {{2, 2, 0, 0, 0.18f}},
            {{0, 0, 1, 0, 0.72f}},
            {{1, 2, 1, 1, 0.56f},
             {1, 2, 1, 1, 0.56f},
             {1, 2, 1, 1, 0.56f}},
            {{2, 0, 1, 0, 0.30f, true, false}},
            {{0, 1, 1, 0, 0.70f, false, true}},
            {{0, 1, 2, 0, 0.45f}},
            // Three consecutive no-response observations let the agent
            // encounter repeated behaviour while INSIST is action-valid.
            {ignoredFirst, ignoredSecond, ignoredThird},
            {{1, 1, 1, 1, 0.56f, true, false}}
        };

        std::vector<EnvironmentObservation> schedule;
        schedule.reserve(static_cast<std::size_t>(std::max(1, requestedSteps)));
        while (schedule.size() < static_cast<std::size_t>(std::max(1, requestedSteps)))
        {
            std::shuffle(blocks.begin(), blocks.end(), rng);
            for (std::size_t index = 0; index < blocks.size(); ++index)
            {
                // Keep the ignored-request block in every cycle; other contexts
                // may be omitted to vary scenario composition.
                const bool isIgnoredRequestBlock = index < blocks.size() &&
                    blocks[index].size() == 3 &&
                    blocks[index][0].goal_state == 1 &&
                    blocks[index][0].user_status == 0;
                if (!isIgnoredRequestBlock && !includeScenario(rng))
                    continue;
                // Never truncate a multi-step block at the episode boundary:
                // in particular, retain all three no-response counts.
                if (schedule.size() + blocks[index].size() >
                    static_cast<std::size_t>(std::max(1, requestedSteps)))
                    continue;
                schedule.insert(schedule.end(), blocks[index].begin(), blocks[index].end());
            }
        }
        return schedule;
    }

    struct RepetitionTracker
    {
        std::string category;
        int consecutive = 0;
    };

    std::string repetitionCategory(const std::string &action)
    {
        if (action == "FETCH" || action == "APPROACH_FOLLOW") return "task_attempt";
        if (action == "INSIST") return "insistence";
        if (action == "GREETING" || action == "NUZZLE" || action == "BARK" ||
            action == "WHINE") return "social_bid";
        if (action == "PLAY") return "play";
        if (action == "EAT") return "feeding";
        if (action == "SLEEP_LAY") return "rest";
        return "exploration_or_avoidance";
    }

    float repetitionPenalty(const std::string &action,
                            bool meaningfulResponse,
                            RepetitionTracker &tracker,
                            int &consecutiveCount)
    {
        if (meaningfulResponse)
        {
            tracker = {};
            consecutiveCount = 0;
            return 0.0f;
        }
        const std::string category = repetitionCategory(action);
        if (tracker.category == category)
            ++tracker.consecutive;
        else
        {
            tracker.category = category;
            tracker.consecutive = 1;
        }
        consecutiveCount = tracker.consecutive;
        // First occurrence is unpenalised. Repetition then incurs a bounded
        // diminishing-return cost shared by both agent conditions.
        return std::clamp(0.04f * static_cast<float>(std::max(0, tracker.consecutive - 1)),
                          0.0f, 0.20f);
    }

    bool testScenarioGenerationAndIgnoredRequest()
    {
        std::mt19937 firstRng(410u), repeatRng(410u), otherRng(411u);
        const auto first = makeScenarioSchedule(firstRng, 40);
        const auto repeated = makeScenarioSchedule(repeatRng, 40);
        const auto other = makeScenarioSchedule(otherRng, 40);
        if (first.size() != 40 || repeated.size() != 40 || other.size() != 40)
            return false;
        const auto key = [](const EnvironmentObservation &o) {
            return std::to_string(o.goal_state) + "|" + std::to_string(o.target_proximity) +
                "|" + std::to_string(o.user_status) + "|" + std::to_string(o.task_status) +
                "|" + std::to_string(o.food_available) + "|" + std::to_string(o.play_available) +
                "|" + std::to_string(o.no_response_count);
        };
        bool sameSeedMatches = true, differentSeedDiffers = false, ignoredRunFound = false;
        for (std::size_t i = 0; i < first.size(); ++i) {
            sameSeedMatches = sameSeedMatches && key(first[i]) == key(repeated[i]);
            differentSeedDiffers = differentSeedDiffers || key(first[i]) != key(other[i]);
            if (i + 2 < first.size() &&
                first[i].goal_state == 1 && first[i].task_status == 1 && first[i].user_status == 0 &&
                first[i].no_response_count == 0 &&
                first[i+1].goal_state == 1 && first[i+1].task_status == 1 && first[i+1].user_status == 0 &&
                first[i+1].no_response_count == 1 &&
                first[i+2].goal_state == 1 && first[i+2].task_status == 1 && first[i+2].user_status == 0 &&
                first[i+2].no_response_count == 2)
                ignoredRunFound = true;
        }
        EnvironmentObservation ignored{1, 1, 0, 1, 0.82f};
        const bool insistInitiallyUnavailable = !isActionValidForObservation("INSIST", ignored);
        ignored.no_response_count = 2;
        return sameSeedMatches && differentSeedDiffers && ignoredRunFound &&
               insistInitiallyUnavailable && isActionValidForObservation("INSIST", ignored);
    }

    bool testSharedRepetitionPenalty()
    {
        RepetitionTracker tracker;
        int count = 0;
        const float first = repetitionPenalty("FETCH", false, tracker, count);
        const float second = repetitionPenalty("FETCH", false, tracker, count);
        const float third = repetitionPenalty("APPROACH_FOLLOW", false, tracker, count);
        const float reset = repetitionPenalty("BARK", true, tracker, count);
        const float afterReset = repetitionPenalty("BARK", false, tracker, count);
        return first == 0.0f && std::abs(second - 0.04f) < 1e-6f &&
               std::abs(third - 0.08f) < 1e-6f && reset == 0.0f &&
               afterReset == 0.0f && count == 1;
    }

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
        // Keep comparisons below reward saturation. Use a moderate energy level
        // so the stress-relief preference for sleep can be distinguished from food
        // relief, while sniffing remains a poor response to an active request.
        const float eatWhenHungry = rewardFor("EAT", feedingTimeRequest, false, 0.55f, 0.20f);
        const float sleepWhenOverwhelmed = rewardFor("SLEEP_LAY", feedingTimeRequest, false, 0.55f, 0.82f);
        const float sniffWhenCompetingNeeds = rewardFor("SNIFF", feedingTimeRequest, false, 0.55f, 0.20f);
        const float sniffWhenIdle = rewardFor("SNIFF", idle, false, 0.75f, 0.20f);
        // Keep this test focused on the mixed-affordance context; scenario generation has its own test.
        const EnvironmentObservation generated = feedingTimeRequest;
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
                              std::ofstream &diagnosticLog,
                              const std::string &mode,
                              const std::string &agentName,
                              int episodeIndex,
                              bool recordDetailedSteps,
                              unsigned int transitionSeed,
                              float initialEnergy,
                              float initialStress,
                              float initialSatisfaction,
                              const std::string& userModel,
                              const SyntheticUserProfile& syntheticProfile,
                              const std::vector<float>& syntheticUserDraws)
    {
        agent.reset();
        agent.setLearningEnabled(learning);
        agent.setExplorationRate(epsilon);

        EpisodeMetrics metrics;
        float environmentEnergy = std::clamp(initialEnergy, 0.25f, 1.0f);
        float environmentStress = std::clamp(initialStress, 0.0f, 0.75f);
        float userSatisfaction = std::clamp(initialSatisfaction, 0.2f, 0.8f);
        RepetitionTracker repetitionTracker;
        int repetitionCount = 0;
        EnvironmentObservation currentObservation = observations.front();
        if(userModel=="synthetic") { currentObservation.user_status=0; currentObservation.no_response_count=0; }
        for (std::size_t step = 0; step < observations.size(); ++step)
        {
            if (step > 0) {
                const bool wasPending=currentObservation.goal_state==1&&currentObservation.task_status==1;
                const int previousStatus=currentObservation.user_status;
                const int previousNoResponse=currentObservation.no_response_count;
                currentObservation = mergeTaskTransition(
                    observations[step - 1], currentObservation, observations[step]);
                if(userModel=="synthetic") {
                    const bool continuing=wasPending&&currentObservation.goal_state==1&&currentObservation.task_status==1;
                    currentObservation.user_status=continuing?previousStatus:0;
                    currentObservation.no_response_count=continuing?previousNoResponse:0;
                }
            }
            currentObservation.energy = environmentEnergy;
            const EnvironmentObservation observation = currentObservation;
            const std::string action = agent.selectAction(observation);
            const std::size_t stateKeyBeforeAction = agent.getCurrentState().encode();
            const auto validActions = validActionsForObservation(observation);
            const std::string stateActionKey = std::to_string(stateKeyBeforeAction) + "|" + action;
            const int priorVisitCount = metrics.state_action_visits[stateActionKey];
            const float intrinsicBonus = learning ? computeIntrinsicBonus(priorVisitCount) : 0.0f;
            metrics.visited_states.insert(stateKeyBeforeAction);
            ++metrics.state_action_visits[stateActionKey];
            for (const auto &validAction : validActions)
                metrics.valid_state_action_pairs_available.insert(
                    std::to_string(stateKeyBeforeAction) + "|" + validAction);
            bool hasValidStoredAction = false;
            bool qTableStatePresent = false;
            std::string contextualFallback;
            std::string policyAction = action;
            std::string finalAction = action;
            std::vector<std::pair<std::string, double>> decisionQValues;
            std::vector<std::string> storedValidActionNames;
            if constexpr (std::is_same_v<Agent, ProposedAgent>) {
                const auto& trace = agent.getLastDecisionTrace();
                hasValidStoredAction = trace.q_table_has_valid_action_values;
                qTableStatePresent = trace.q_table_state_present;
                contextualFallback = trace.contextual_fallback;
                policyAction = trace.policy_action;
                finalAction = trace.final_action;
                decisionQValues = trace.valid_action_q_values;
                storedValidActionNames = trace.stored_valid_action_names;
            } else {
                hasValidStoredAction = agent.lastDecisionHadValidQAction();
                qTableStatePresent = agent.lastDecisionHadQTableState();
                contextualFallback = agent.getLastContextualFallback();
                decisionQValues = agent.getValidActionQValues(observation);
                storedValidActionNames = agent.getStoredValidActionNames(observation);
            }
            if (hasValidStoredAction) ++metrics.q_table_supported_decisions;
            else ++metrics.fallback_decisions;

            const bool insistPredicateEligible = isActionValidForObservation("INSIST", observation);
            const bool insistAvailable = std::find(validActions.begin(), validActions.end(), "INSIST") != validActions.end();
            metrics.insist_predicate_eligible_decisions += insistPredicateEligible ? 1 : 0;
            metrics.insist_available_decisions += insistAvailable ? 1 : 0;
            metrics.insist_eligibility_mask_mismatches += insistPredicateEligible != insistAvailable ? 1 : 0;
            metrics.insist_policy_selections += policyAction == "INSIST" ? 1 : 0;
            metrics.insist_final_selections += finalAction == "INSIST" ? 1 : 0;
            metrics.insist_execution_attempts += action == "INSIST" ? 1 : 0;
            metrics.insist_selected_when_unavailable += action == "INSIST" && !insistAvailable ? 1 : 0;

            std::vector<std::pair<std::string, double>> storedDecisionQValues;
            for (const auto& item : decisionQValues) {
                if (std::find(storedValidActionNames.begin(), storedValidActionNames.end(), item.first) != storedValidActionNames.end())
                    storedDecisionQValues.push_back(item);
            }
            double qValueSpread = 0.0;
            bool hasQValueSpread = false;
            if (storedDecisionQValues.size() >= 2) {
                const auto bounds = std::minmax_element(
                    storedDecisionQValues.begin(), storedDecisionQValues.end(),
                    [](const auto& lhs, const auto& rhs) { return lhs.second < rhs.second; });
                qValueSpread = bounds.second->second - bounds.first->second;
                metrics.q_value_spreads.push_back(qValueSpread);
                hasQValueSpread = true;
            }

            const float energyBefore = environmentEnergy;
            const float stressBefore = environmentStress;
            // Same seed per episode/step gives both agents paired environmental noise,
            // independent of how many stochastic actions either policy selected.
            const float elapsedSeconds = step + 1 == observations.size() ? 0.0f : elapsedIntervals[step];
            std::mt19937 transitionRng(transitionSeed ^
                (static_cast<unsigned int>(episodeIndex + 1) * 7919u) ^
                (static_cast<unsigned int>(step) * 104729u));
            const EnvironmentStep transition =
                applyEnvironmentAction(action, observation, environmentEnergy,
                                       environmentStress, userSatisfaction, transitionRng, elapsedSeconds);
            environmentEnergy = transition.energy_after;
            environmentStress = transition.stress_after;
            userSatisfaction = transition.user_satisfaction_after;
            const bool energyDepleted = transition.energy_after <= 0.0f;
            const bool terminal = step + 1 == observations.size() || energyDepleted;
            SyntheticUserResponse syntheticResponse;
            if(userModel=="synthetic") {
                const float draw=step<syntheticUserDraws.size()?syntheticUserDraws[step]:0.5f;
                syntheticResponse=simulateSyntheticUserResponse(observation,action,transition,syntheticProfile,draw);
                if(syntheticResponse.status==1) ++metrics.synthetic_positive_responses;
                else if(syntheticResponse.status==2) ++metrics.synthetic_negative_responses;
                else ++metrics.synthetic_no_responses;
                if(observation.goal_state==1&&observation.task_status==1) ++metrics.synthetic_requests_observed;
                if(syntheticResponse.request_acknowledged) ++metrics.synthetic_requests_acknowledged;
            }
            const bool meaningfulResponse = userModel=="synthetic"
                ? (syntheticResponse.status!=0||transition.completed)
                : (transition.completed||observation.user_status!=0);
            const std::string previousRepetitionCategory = repetitionTracker.category;
            const int previousRepetitionCount = repetitionTracker.consecutive;
            const std::string currentRepetitionCategory = repetitionCategory(action);
            const bool repetitionResponseReset = meaningfulResponse && previousRepetitionCount > 0;
            const bool repetitionCategoryChanged = !meaningfulResponse && previousRepetitionCount >= 2 &&
                !previousRepetitionCategory.empty() && previousRepetitionCategory != currentRepetitionCategory;
            const float boredomPenalty = repetitionPenalty(
                action, meaningfulResponse, repetitionTracker, repetitionCount);
            const float transitionReward = energyDepleted ? -1.0f :
                std::clamp(transition.reward - boredomPenalty + intrinsicBonus, -1.0f, 1.0f);

            EnvironmentObservation nextObservation = transition.next_observation;
            if (!terminal) {
                nextObservation = mergeTaskTransition(
                    observation, transition.next_observation, observations[step + 1]);
            }
            if(userModel=="synthetic") {
                nextObservation.user_status=syntheticResponse.status;
                nextObservation.no_response_count=syntheticResponse.no_response_count;
                if(transition.completed) { nextObservation.task_status=0; nextObservation.goal_state=0; }
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

            if (action == "INSIST") {
                if (meaningfulResponse) ++metrics.insist_attempts_with_meaningful_response;
                else ++metrics.insist_attempts_without_meaningful_response;
            }
            if (repetitionCount >= 2) ++metrics.repetition_decisions;
            if (boredomPenalty > 0.0f) {
                ++metrics.repetition_penalty_trigger_count;
                metrics.repetition_penalty_total += boredomPenalty;
                metrics.repetition_penalty_max = std::max(metrics.repetition_penalty_max, static_cast<double>(boredomPenalty));
                metrics.repetition_penalty_samples.push_back(boredomPenalty);
            }
            if (repetitionResponseReset) ++metrics.repetition_response_reset_count;
            if (repetitionCategoryChanged) ++metrics.repetition_category_change_count;
            metrics.repetition_max_consecutive_count = std::max(metrics.repetition_max_consecutive_count, repetitionCount);

            double repeatedActionQGap = 0.0;
            bool hasRepeatedActionQGap = false;
            if (repetitionCount >= 2 &&
                std::find(storedValidActionNames.begin(), storedValidActionNames.end(), action) != storedValidActionNames.end()) {
                const auto repeatedValue = std::find_if(storedDecisionQValues.begin(), storedDecisionQValues.end(),
                    [&](const auto& item) { return item.first == action; });
                double bestAlternative = -std::numeric_limits<double>::infinity();
                for (const auto& item : storedDecisionQValues) {
                    if (item.first != action) bestAlternative = std::max(bestAlternative, item.second);
                }
                if (repeatedValue != storedDecisionQValues.end() && std::isfinite(bestAlternative)) {
                    repeatedActionQGap = bestAlternative - repeatedValue->second;
                    metrics.repeated_action_q_gaps.push_back(repeatedActionQGap);
                    hasRepeatedActionQGap = true;
                }
            }

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
            row["no_response_count"] = observation.no_response_count;
            row["energy_before"] = energyBefore;
            row["energy_after"] = transition.energy_after;
            row["simulated_elapsed_seconds"] = elapsedSeconds;
            if constexpr (std::is_same_v<Agent, ProposedAgent>) {
                row["affect_arousal_after_transition"] = agent.getAffectiveState().arousal;
                row["affect_stress_after_transition"] = agent.getAffectiveState().stress;
                row["affect_engagement_after_transition"] = agent.getAffectiveState().engagement;
                row["affect_affinity_after_transition"] = agent.getAffectiveState().affinity;
                row["affect_energy_after_transition"] = static_cast<int>(agent.getAffectiveState().energy);
                row["affect_valence_after_transition"] = static_cast<int>(agent.getAffectiveState().valence);
                const auto& decisionTrace = agent.getLastDecisionTrace();
                row["memory_record_count_at_decision"] = decisionTrace.memory_record_count;
                row["memory_signal_at_decision"] = decisionTrace.memory_signal;
                row["affect_arousal_at_decision"] = decisionTrace.affect.arousal;
                row["affect_stress_at_decision"] = decisionTrace.affect.stress;
                row["affect_engagement_at_decision"] = decisionTrace.affect.engagement;
                row["affect_affinity_at_decision"] = decisionTrace.affect.affinity;
                row["memory_record_count_after_transition"] = agent.getMemoryRecords().size();
                row["memory_signal_after_transition"] = summarizeMemorySignal(agent.getMemoryRecords());
                row["preferred_fetch_object"] = agent.getPreferredFetchObject();
                row["general_preference_override"] = decisionTrace.general_preference_override;
                row["rejection_override_applied"] = decisionTrace.rejection_override_applied;
            } else {
                row["general_preference_override"] = "";
                row["rejection_override_applied"] = false;
                row["affect_arousal_after_transition"] = nullptr;
                row["affect_stress_after_transition"] = nullptr;
                row["affect_engagement_after_transition"] = nullptr;
                row["affect_affinity_after_transition"] = nullptr;
                row["affect_energy_after_transition"] = nullptr;
                row["affect_valence_after_transition"] = nullptr;
                row["memory_record_count_at_decision"] = nullptr;
                row["memory_signal_at_decision"] = nullptr;
                row["affect_arousal_at_decision"] = nullptr;
                row["affect_stress_at_decision"] = nullptr;
                row["affect_engagement_at_decision"] = nullptr;
                row["affect_affinity_at_decision"] = nullptr;
                row["memory_record_count_after_transition"] = nullptr;
                row["memory_signal_after_transition"] = nullptr;
                row["preferred_fetch_object"] = nullptr;
            }
            row["policy_action_before_overrides"] = policyAction;
            row["final_action_after_overrides"] = finalAction;
            nlohmann::json validActionQValues = nlohmann::json::array();
            for (const auto& item : decisionQValues) {
                validActionQValues.push_back({{"action", item.first}, {"q_value", item.second}});
            }
            row["valid_actions"] = validActions;
            row["valid_action_q_values"] = validActionQValues;
            row["stored_valid_action_names"] = storedValidActionNames;
            row["environment_stress_after"] = transition.stress_after;
            row["user_satisfaction_after"] = transition.user_satisfaction_after;
            row["repetition_category"] = repetitionCategory(action);
            row["repetition_consecutive_count"] = repetitionCount;
            row["boredom_penalty"] = boredomPenalty;
            row["meaningful_environment_or_user_response"] = meaningfulResponse;
            row["user_model"] = userModel;
            row["synthetic_user_response_status"] = userModel=="synthetic"?nlohmann::json(syntheticResponse.status):nlohmann::json(nullptr);
            row["synthetic_user_response_category"] = userModel=="synthetic"?nlohmann::json(syntheticResponse.category):nlohmann::json(nullptr);
            row["synthetic_user_no_response_count_after"] = userModel=="synthetic"?nlohmann::json(syntheticResponse.no_response_count):nlohmann::json(nullptr);
            row["synthetic_user_request_pending_after"] = userModel=="synthetic"?nlohmann::json(syntheticResponse.request_pending):nlohmann::json(nullptr);
            row["action"] = action;
            row["encoded_state"] = stateKeyBeforeAction;
            row["q_table_state_present"] = qTableStatePresent;
            row["q_table_has_valid_action_values"] = hasValidStoredAction;
            row["contextual_fallback"] = contextualFallback;
            row["decision_used_fallback"] = !hasValidStoredAction;
            row["next_encoded_state"] = agent.getCurrentState().encode();
            row["reward"] = transitionReward;
            row["terminal"] = terminal;
            row["terminal_reason"] = energyDepleted ? "energy_depleted" :
                                     (step + 1 == observations.size() ? "step_limit" : "none");
            row["energy_depleted"] = energyDepleted;
            row["completed"] = transition.completed;
            row["fetch_succeeded"] = transition.completed;
            row["insist_predicate_eligible"] = insistPredicateEligible;
            row["insist_available"] = insistAvailable;
            row["insist_policy_selected"] = policyAction == "INSIST";
            row["insist_final_selected"] = finalAction == "INSIST";
            row["insist_execution_attempted"] = action == "INSIST";
            row["repetition_response_reset"] = repetitionResponseReset;
            row["repetition_category_changed"] = repetitionCategoryChanged;
            row["q_value_spread"] = hasQValueSpread ? nlohmann::json(qValueSpread) : nlohmann::json(nullptr);
            row["repeated_action_vs_best_alternative_q_gap"] = hasRepeatedActionQGap
                ? nlohmann::json(repeatedActionQGap) : nlohmann::json(nullptr);
            // Keep full episode-level metrics in the summary, but retain verbose
            // per-step diagnostics only for the final few episodes to control artifact size.
            if (recordDetailedSteps)
                stepLog << row.dump() << '\n';

            const bool insistEvent = insistPredicateEligible || insistAvailable ||
                policyAction == "INSIST" || finalAction == "INSIST" || action == "INSIST";
            const bool repetitionEvent = boredomPenalty > 0.0f || repetitionResponseReset ||
                repetitionCategoryChanged;
            if (insistEvent || repetitionEvent) {
                nlohmann::json event;
                event["schema_version"] = 1;
                event["mode"] = mode;
                event["agent"] = agentName;
                event["episode"] = episodeIndex;
                event["step"] = step;
                event["event_types"] = nlohmann::json::array();
                if (insistPredicateEligible) event["event_types"].push_back("insist_eligible");
                if (insistAvailable) event["event_types"].push_back("insist_available");
                if (policyAction == "INSIST") event["event_types"].push_back("insist_policy_selected");
                if (finalAction == "INSIST") event["event_types"].push_back("insist_final_selected");
                if (action == "INSIST") event["event_types"].push_back("insist_execution_attempt");
                if (boredomPenalty > 0.0f) event["event_types"].push_back("repetition_penalty_applied");
                if (repetitionResponseReset) event["event_types"].push_back("repetition_context_reset_by_response");
                if (repetitionCategoryChanged) event["event_types"].push_back("repetition_category_changed");
                event["encoded_state"] = stateKeyBeforeAction;
                event["goal_state"] = observation.goal_state;
                event["task_status"] = observation.task_status;
                event["user_status"] = observation.user_status;
                event["no_response_count"] = observation.no_response_count;
                event["valid_actions"] = validActions;
                event["insist_predicate_eligible"] = insistPredicateEligible;
                event["insist_available"] = insistAvailable;
                event["insist_policy_action"] = policyAction;
                event["insist_final_action"] = finalAction;
                event["selected_action"] = action;
                event["insist_execution_attempted"] = action == "INSIST";
                event["insist_selected_when_unavailable"] = action == "INSIST" && !insistAvailable;
                event["insist_meaningful_response_after_attempt"] = action == "INSIST" && meaningfulResponse;
                event["q_table_supported_decision"] = hasValidStoredAction;
                event["q_table_state_present"] = qTableStatePresent;
                event["valid_action_q_values"] = validActionQValues;
                event["stored_valid_action_names"] = storedValidActionNames;
                event["stored_valid_action_q_values"] = nlohmann::json::array();
                for (const auto& item : storedDecisionQValues)
                    event["stored_valid_action_q_values"].push_back({{"action", item.first}, {"q_value", item.second}});
                event["q_value_spread"] = hasQValueSpread ? nlohmann::json(qValueSpread) : nlohmann::json(nullptr);
                event["repeated_action_vs_best_alternative_q_gap"] = hasRepeatedActionQGap
                    ? nlohmann::json(repeatedActionQGap) : nlohmann::json(nullptr);
                event["repetition_category"] = currentRepetitionCategory;
                event["repetition_consecutive_count"] = repetitionCount;
                event["repetition_penalty"] = boredomPenalty;
                event["meaningful_response"] = meaningfulResponse;
                event["reward_before_repetition_penalty"] = transition.reward;
                event["reward_after_repetition_penalty"] = transitionReward;
                event["terminal"] = terminal;
                diagnosticLog << event.dump() << '\n';
            }

            currentObservation = nextObservation;
            if (terminal) break;
        }
        if(userModel=="synthetic"&&currentObservation.goal_state==1&&currentObservation.task_status==1)
            metrics.synthetic_requests_pending_at_end=1;
        return metrics;
    }

    nlohmann::json distributionSummary(std::vector<double> values)
    {
        nlohmann::json result = {{"sample_count", values.size()}};
        if (values.empty()) return result;
        std::sort(values.begin(), values.end());
        const auto quantile = [&](double probability) {
            const double position = probability * static_cast<double>(values.size() - 1);
            const std::size_t lower = static_cast<std::size_t>(std::floor(position));
            const std::size_t upper = static_cast<std::size_t>(std::ceil(position));
            const double fraction = position - static_cast<double>(lower);
            return values[lower] + (values[upper] - values[lower]) * fraction;
        };
        double sum = 0.0;
        for (double value : values) sum += value;
        const double mean = sum / static_cast<double>(values.size());
        double squaredDeviation = 0.0;
        for (double value : values) squaredDeviation += (value - mean) * (value - mean);
        result["mean"] = mean;
        result["standard_deviation_population"] = std::sqrt(squaredDeviation / static_cast<double>(values.size()));
        result["min"] = values.front();
        result["p25"] = quantile(0.25);
        result["median"] = quantile(0.50);
        result["p75"] = quantile(0.75);
        result["p90"] = quantile(0.90);
        result["max"] = values.back();
        return result;
    }

    void accumulateTrainingDiagnostics(EpisodeMetrics& total, const EpisodeMetrics& episode)
    {
        total.steps += episode.steps;
        total.synthetic_positive_responses += episode.synthetic_positive_responses;
        total.synthetic_negative_responses += episode.synthetic_negative_responses;
        total.synthetic_no_responses += episode.synthetic_no_responses;
        total.synthetic_requests_observed += episode.synthetic_requests_observed;
        total.synthetic_requests_acknowledged += episode.synthetic_requests_acknowledged;
        total.synthetic_requests_pending_at_end += episode.synthetic_requests_pending_at_end;
        total.insist_predicate_eligible_decisions += episode.insist_predicate_eligible_decisions;
        total.insist_available_decisions += episode.insist_available_decisions;
        total.insist_eligibility_mask_mismatches += episode.insist_eligibility_mask_mismatches;
        total.insist_policy_selections += episode.insist_policy_selections;
        total.insist_final_selections += episode.insist_final_selections;
        total.insist_execution_attempts += episode.insist_execution_attempts;
        total.insist_selected_when_unavailable += episode.insist_selected_when_unavailable;
        total.insist_attempts_with_meaningful_response += episode.insist_attempts_with_meaningful_response;
        total.insist_attempts_without_meaningful_response += episode.insist_attempts_without_meaningful_response;
        total.repetition_decisions += episode.repetition_decisions;
        total.repetition_penalty_trigger_count += episode.repetition_penalty_trigger_count;
        total.repetition_response_reset_count += episode.repetition_response_reset_count;
        total.repetition_category_change_count += episode.repetition_category_change_count;
        total.repetition_max_consecutive_count = std::max(total.repetition_max_consecutive_count,
                                                          episode.repetition_max_consecutive_count);
        total.repetition_penalty_total += episode.repetition_penalty_total;
        total.repetition_penalty_max = std::max(total.repetition_penalty_max, episode.repetition_penalty_max);
        total.q_value_spreads.insert(total.q_value_spreads.end(), episode.q_value_spreads.begin(), episode.q_value_spreads.end());
        total.repeated_action_q_gaps.insert(total.repeated_action_q_gaps.end(), episode.repeated_action_q_gaps.begin(), episode.repeated_action_q_gaps.end());
        total.repetition_penalty_samples.insert(total.repetition_penalty_samples.end(), episode.repetition_penalty_samples.begin(), episode.repetition_penalty_samples.end());
    }

    nlohmann::json trainingDiagnosticsToJson(const EpisodeMetrics& metrics)
    {
        return {
            {"decisions_observed", metrics.steps},
            {"insist", {
                {"predicate_eligible_decisions", metrics.insist_predicate_eligible_decisions},
                {"valid_action_available_decisions", metrics.insist_available_decisions},
                {"eligibility_vs_valid_action_mask_mismatches", metrics.insist_eligibility_mask_mismatches},
                {"policy_selections_before_overrides", metrics.insist_policy_selections},
                {"final_selections_after_overrides", metrics.insist_final_selections},
                {"environment_execution_attempts", metrics.insist_execution_attempts},
                {"selected_when_unavailable", metrics.insist_selected_when_unavailable},
                {"attempts_followed_by_meaningful_response", metrics.insist_attempts_with_meaningful_response},
                {"attempts_without_meaningful_response", metrics.insist_attempts_without_meaningful_response}
            }},
            {"repetition", {
                {"repeated_category_decisions", metrics.repetition_decisions},
                {"penalty_trigger_count", metrics.repetition_penalty_trigger_count},
                {"cumulative_penalty", metrics.repetition_penalty_total},
                {"mean_penalty_per_decision", metrics.steps > 0 ? metrics.repetition_penalty_total / metrics.steps : 0.0},
                {"mean_penalty_per_trigger", metrics.repetition_penalty_trigger_count > 0 ? metrics.repetition_penalty_total / metrics.repetition_penalty_trigger_count : 0.0},
                {"maximum_single_step_penalty", metrics.repetition_penalty_max},
                {"maximum_consecutive_count", metrics.repetition_max_consecutive_count},
                {"context_resets_after_meaningful_response", metrics.repetition_response_reset_count},
                {"category_change_events", metrics.repetition_category_change_count},
                {"penalty_distribution", distributionSummary(metrics.repetition_penalty_samples)}
            }},
            {"q_value_analysis", {
                {"within_state_valid_action_value_spread", distributionSummary(metrics.q_value_spreads)},
                {"repeated_action_minus_best_alternative_gap_sign_convention", "best_alternative_q_minus_repeated_action_q; positive means the best alternative has higher learned value"},
                {"repeated_action_vs_best_alternative_gap", distributionSummary(metrics.repeated_action_q_gaps)}
            }}
        };
    }

    nlohmann::json metricsToJson(const EpisodeMetrics &metrics)
    {
        nlohmann::json result;
        result["total_reward"] = metrics.total_reward;
        result["steps"] = metrics.steps;
        result["completed"] = metrics.completed;
        result["synthetic_user"] = {
            {"positive_responses",metrics.synthetic_positive_responses},
            {"negative_responses",metrics.synthetic_negative_responses},
            {"no_responses",metrics.synthetic_no_responses},
            {"requests_observed",metrics.synthetic_requests_observed},
            {"requests_acknowledged",metrics.synthetic_requests_acknowledged},
            {"requests_pending_at_end",metrics.synthetic_requests_pending_at_end}
        };
        result["action_counts"] = metrics.action_counts;
        result["unique_states_visited"] = metrics.visited_states.size();
        result["unique_state_action_pairs_visited"] = metrics.state_action_visits.size();
        result["valid_state_action_pairs_available_on_encountered_states"] =
            metrics.valid_state_action_pairs_available.size();
        result["valid_state_action_pair_coverage_on_encountered_states"] =
            metrics.valid_state_action_pairs_available.empty() ? 0.0 :
            static_cast<double>(metrics.state_action_visits.size()) /
                metrics.valid_state_action_pairs_available.size();
        result["state_action_visits"] = metrics.state_action_visits;
        result["q_table_supported_decisions"] = metrics.q_table_supported_decisions;
        result["fallback_decisions"] = metrics.fallback_decisions;
        result["q_table_supported_decision_rate"] = metrics.steps > 0
            ? static_cast<double>(metrics.q_table_supported_decisions) / metrics.steps : 0.0;
        result["fallback_decision_rate"] = metrics.steps > 0
            ? static_cast<double>(metrics.fallback_decisions) / metrics.steps : 0.0;
        result["stimulus_checks"] = metrics.stimulus_checks;
        result["stimulus_aligned_actions"] = metrics.stimulus_aligned_actions;
        result["stimulus_response_alignment_rate"] =
            metrics.stimulus_checks > 0
                ? static_cast<double>(metrics.stimulus_aligned_actions) / metrics.stimulus_checks
                : 0.0;
        result["stimulus_checks_by_type"] = metrics.stimulus_checks_by_type;
        result["stimulus_aligned_by_type"] = metrics.stimulus_aligned_by_type;
        result["training_diagnostics"] = trainingDiagnosticsToJson(metrics);
        return result;
    }
} // namespace


namespace {
#ifndef IS2_GIT_REVISION
#define IS2_GIT_REVISION "unknown"
#endif
#ifndef IS2_GIT_DIRTY
#define IS2_GIT_DIRTY "unknown"
#endif
#ifndef IS2_COMPILER_ID
#define IS2_COMPILER_ID "unknown"
#endif
#ifndef IS2_COMPILER_VERSION
#define IS2_COMPILER_VERSION "unknown"
#endif
#ifndef IS2_BUILD_TYPE
#define IS2_BUILD_TYPE "unknown"
#endif

std::string utcTimestampNow()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t raw = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &raw);
#else
    gmtime_r(&raw, &utc);
#endif
    std::ostringstream output;
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return output.str();
}

std::string fnv1a64FileHash(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) return "unavailable";
    std::uint64_t hash = 14695981039346656037ull;
    char buffer[8192];
    while (input) {
        input.read(buffer, sizeof(buffer));
        const std::streamsize count = input.gcount();
        for (std::streamsize index = 0; index < count; ++index) {
            hash ^= static_cast<unsigned char>(buffer[index]);
            hash *= 1099511628211ull;
        }
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

nlohmann::json buildProvenanceJson()
{
    nlohmann::json build = {
        {"git_revision", IS2_GIT_REVISION},
        {"git_worktree_dirty_at_configure", IS2_GIT_DIRTY},
        {"compiler_id", IS2_COMPILER_ID},
        {"compiler_version", IS2_COMPILER_VERSION},
        {"build_type", IS2_BUILD_TYPE},
        {"cxx_standard", 17},
        {"compiled_at", std::string(__DATE__) + " " + __TIME__}
    };
    std::error_code error;
    const auto executablePath = std::filesystem::canonical("/proc/self/exe", error);
    if (!error) {
        build["executable_path"] = executablePath.string();
        build["executable_fnv1a64"] = fnv1a64FileHash(executablePath);
        const auto size = std::filesystem::file_size(executablePath, error);
        if (!error) build["executable_size_bytes"] = size;
        error.clear();
        const auto modified = std::filesystem::last_write_time(executablePath, error);
        if (!error) build["executable_mtime_clock_ticks"] = modified.time_since_epoch().count();
    } else {
        build["executable_path"] = "unavailable";
        build["executable_fnv1a64"] = "unavailable";
    }
    build["executable_hash_note"] = "FNV-1a-64 is a non-cryptographic identity checksum, not a security hash.";
    return build;
}
} // namespace

bool runTrainingExperiment(const std::string &mode,
                           int episodes,
                           int steps,
                           unsigned int seed,
                           const std::string &outputDirectory,
                           const std::string& userModel,
                           bool resumeTraining,
                           const std::vector<std::string>& commandArguments)
{
    const std::string runStartedAtUtc = utcTimestampNow();
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
    rotateArtifactForComparison(outDir / (mode + "_diagnostic_events.jsonl"));
    rotateArtifactForComparison(outDir / (mode + "_run_manifest.json"));
    std::ofstream baselineLog(outDir / (mode + "_baseline_steps.jsonl"));
    std::ofstream proposedLog(outDir / (mode + "_proposed_steps.jsonl"));
    std::ofstream diagnosticLog(outDir / (mode + "_diagnostic_events.jsonl"));
    if (!baselineLog || !proposedLog || !diagnosticLog)
    {
        std::cerr << "Unable to open experiment log files in " << outDir << '\n';
        return false;
    }

    const unsigned int environmentSeed = trainingMode ? seed : seed + 1000003u;
    std::mt19937 environmentRng(environmentSeed);
    nlohmann::json summary;
    summary["mode"] = mode;
    summary["command_arguments"] = commandArguments;
    summary["training_initialization"] = mode == "resume-training" ? "loaded_existing_qtables" :
                                         mode == "train" ? "fresh_qtables" : "loaded_evaluation_checkpoint";
    summary["resume_training_requested"] = resumeTraining || mode == "resume-training";
    summary["seed"] = seed;
    summary["environment_seed"] = environmentSeed;
    summary["episodes"] = episodes;
    summary["max_steps_per_episode"] = steps;
    summary["reward_model"] = "synthetic_v4_graded_context_fit";
    summary["user_model"] = userModel;
    summary["synthetic_user_model"] = userModel=="synthetic"?nlohmann::json({
        {"profile_ranges",{{"responsiveness",{0.50,0.95}},{"patience_turns",{2,5}},{"social_openness",{0.40,0.95}}}},
        {"response_status_convention","0=silence, 1=positive, 2=negative"},
        {"response_randomness","episode/step seeded draws shared between paired agents"},
        {"policy_independence","Uses public context and action/outcome only; never reads agent identity, Q-values, memory, or affect."}
    }):nlohmann::json(nullptr);
    summary["affect_decay_during_training"] = true;
    summary["simulated_seconds_between_decisions_min"] = kMinSecondsPerTrainingStep;
    summary["simulated_seconds_between_decisions_max"] = kMaxSecondsPerTrainingStep;
    summary["temporal_model"] = "Each non-terminal decision uses a seeded elapsed-time interval sampled uniformly from 0.5 to 5 simulated seconds. Paired agents share intervals. Physical energy receives action effort costs and passive time-based drain (0.0025 per second normally; 0.0008 while resting). The shared energy value is observable to both agents and included in both state representations. Proposed-agent affect decays over the interval before the next state is built for Q-learning.";
    summary["scenario_generation"] = {
        {"method", "seeded shuffled scenario blocks with probabilistic scenario inclusion"},
        {"user_response_mode",userModel},
        {"requested_steps", steps},
        {"episode_length_range", {std::max(1, steps * 3 / 4), std::max(1, steps * 5 / 4)}},
        {"preserved_sequences", {"fetch retries", "three-step ignored request / no-response sequence"}},
        {"ignored_request_contract", "task_status=1 and user_status=0; INSIST becomes action-valid only after no_response_count reaches 2"},
        {"random_seed_recorded_per_episode", true}
    };
    summary["environment_repetition_constraint"] = {
        {"enabled", true},
        {"scope", "shared training environment reward for both agents"},
        {"category_window", "consecutive action categories within an episode"},
        {"first_occurrence_penalty", 0.0},
        {"increment_per_repeat", 0.04},
        {"maximum_penalty", 0.20},
        {"recovery", "meaningful user/environment response or change of action category"}
    };
    summary["note"] = "Training uses reproducibly shuffled and variably composed scenario blocks. Both agents receive the same schedule and seeded transition noise. Repeated action categories without a meaningful response incur a shared bounded environmental reward penalty. These synthetic rewards and coverage diagnostics are engineering measures, not perceived believability.";

    nlohmann::json baselineEpisodes = nlohmann::json::array();
    nlohmann::json proposedEpisodes = nlohmann::json::array();
    double baselineRewardSum = 0.0;
    double proposedRewardSum = 0.0;
    double baselineStepSum = 0.0;
    double proposedStepSum = 0.0;
    int baselineCompletions = 0;
    int proposedCompletions = 0;
    std::set<std::size_t> baselineGlobalStates, proposedGlobalStates;
    std::set<std::string> baselineGlobalVisitedPairs, proposedGlobalVisitedPairs;
    std::set<std::string> baselineGlobalAvailablePairs, proposedGlobalAvailablePairs;
    EpisodeMetrics baselineDiagnosticAggregate;
    EpisodeMetrics proposedDiagnosticAggregate;
    std::uniform_int_distribution<int> episodeLengthDist(
        std::max(1, steps * 3 / 4), std::max(1, steps * 5 / 4));

    for (int episode = 0; episode < episodes; ++episode)
    {
        const int episodeSteps = episodeLengthDist(environmentRng);
        std::uniform_real_distribution<float> initialEnergyDist(0.45f, 0.95f);
        std::uniform_real_distribution<float> initialStressDist(0.10f, 0.55f);
        std::uniform_real_distribution<float> initialSatisfactionDist(0.35f, 0.75f);
        const float episodeInitialEnergy = initialEnergyDist(environmentRng);
        const float episodeInitialStress = initialStressDist(environmentRng);
        const float episodeInitialSatisfaction = initialSatisfactionDist(environmentRng);
        const unsigned int episodeScenarioSeed =
            seed ^ (static_cast<unsigned int>(episode + 1) * 2654435761u);
        std::mt19937 scenarioRng(episodeScenarioSeed);
        std::vector<EnvironmentObservation> observations =
            makeScenarioSchedule(scenarioRng, episodeSteps);
        if(userModel=="synthetic") for(auto& o:observations) { o.user_status=0; o.no_response_count=0; }
        const SyntheticUserProfile episodeSyntheticProfile=makeSyntheticUserProfile(episodeScenarioSeed);
        const std::vector<float> syntheticUserDraws=makeSyntheticUserDraws(episodeScenarioSeed,observations.size());
        nlohmann::json scenarioComposition = nlohmann::json::array();
        for (const auto &item : observations) {
            scenarioComposition.push_back({
                {"goal_state", item.goal_state},
                {"target_proximity", item.target_proximity},
                {"user_status", item.user_status},
                {"task_status", item.task_status},
                {"signal_strength", item.signal_strength},
                {"food_available", item.food_available},
                {"play_available", item.play_available},
                {"no_response_count", item.no_response_count}
            });
        }
        std::vector<float> elapsedIntervals;
        elapsedIntervals.reserve(observations.size());
        std::uniform_real_distribution<float> elapsedDist(
            kMinSecondsPerTrainingStep, kMaxSecondsPerTrainingStep);
        for (std::size_t step = 0; step < observations.size(); ++step)
            elapsedIntervals.push_back(elapsedDist(environmentRng));

        const float epsilon = trainingMode
                                  ? computeTrainingEpsilon(episode, episodes)
                                  : 0.0f;
        const bool learning = trainingMode;

        const bool recordDetailedSteps = episode >= std::max(0, episodes - kDetailedStepLogEpisodes);
        const EpisodeMetrics baselineMetrics = runEpisode(
            baseline, observations, elapsedIntervals, learning, epsilon, baselineLog, diagnosticLog, mode, "baseline", episode,
            recordDetailedSteps, environmentSeed, episodeInitialEnergy, episodeInitialStress, episodeInitialSatisfaction,
            userModel, episodeSyntheticProfile, syntheticUserDraws);
        const EpisodeMetrics proposedMetrics = runEpisode(
            proposed, observations, elapsedIntervals, learning, epsilon, proposedLog, diagnosticLog, mode, "proposed", episode,
            recordDetailedSteps, environmentSeed, episodeInitialEnergy, episodeInitialStress, episodeInitialSatisfaction,
            userModel, episodeSyntheticProfile, syntheticUserDraws);
        accumulateTrainingDiagnostics(baselineDiagnosticAggregate, baselineMetrics);
        accumulateTrainingDiagnostics(proposedDiagnosticAggregate, proposedMetrics);

        baselineGlobalStates.insert(baselineMetrics.visited_states.begin(), baselineMetrics.visited_states.end());
        proposedGlobalStates.insert(proposedMetrics.visited_states.begin(), proposedMetrics.visited_states.end());
        for (const auto &entry : baselineMetrics.state_action_visits) baselineGlobalVisitedPairs.insert(entry.first);
        for (const auto &entry : proposedMetrics.state_action_visits) proposedGlobalVisitedPairs.insert(entry.first);
        baselineGlobalAvailablePairs.insert(baselineMetrics.valid_state_action_pairs_available.begin(),
                                            baselineMetrics.valid_state_action_pairs_available.end());
        proposedGlobalAvailablePairs.insert(proposedMetrics.valid_state_action_pairs_available.begin(),
                                            proposedMetrics.valid_state_action_pairs_available.end());

        baselineRewardSum += baselineMetrics.total_reward;
        proposedRewardSum += proposedMetrics.total_reward;
        baselineStepSum += baselineMetrics.steps;
        proposedStepSum += proposedMetrics.steps;
        baselineCompletions += baselineMetrics.completed ? 1 : 0;
        proposedCompletions += proposedMetrics.completed ? 1 : 0;

        nlohmann::json baselineRow = metricsToJson(baselineMetrics);
        baselineRow["episode"] = episode;
        baselineRow["scenario_seed"] = episodeScenarioSeed;
        baselineRow["user_model"] = userModel;
        baselineRow["synthetic_user_profile"] = userModel=="synthetic"?nlohmann::json({
            {"responsiveness",episodeSyntheticProfile.responsiveness},{"patience",episodeSyntheticProfile.patience},
            {"social_openness",episodeSyntheticProfile.social_openness}}):nlohmann::json(nullptr);
        baselineRow["scenario_steps"] = observations.size();
        baselineRow["initial_environment"] = {
            {"energy", episodeInitialEnergy},
            {"stress", episodeInitialStress},
            {"user_satisfaction", episodeInitialSatisfaction}
        };
        baselineRow["scenario_composition"] = scenarioComposition;
        baselineEpisodes.push_back(baselineRow);
        nlohmann::json proposedRow = metricsToJson(proposedMetrics);
        proposedRow["episode"] = episode;
        proposedRow["scenario_seed"] = episodeScenarioSeed;
        proposedRow["user_model"] = userModel;
        proposedRow["synthetic_user_profile"] = userModel=="synthetic"?nlohmann::json({
            {"responsiveness",episodeSyntheticProfile.responsiveness},{"patience",episodeSyntheticProfile.patience},
            {"social_openness",episodeSyntheticProfile.social_openness}}):nlohmann::json(nullptr);
        proposedRow["scenario_steps"] = observations.size();
        proposedRow["initial_environment"] = {
            {"energy", episodeInitialEnergy},
            {"stress", episodeInitialStress},
            {"user_satisfaction", episodeInitialSatisfaction}
        };
        proposedRow["scenario_composition"] = scenarioComposition;
        proposedEpisodes.push_back(proposedRow);
    }

    baselineLog.close();
    proposedLog.close();
    diagnosticLog.close();

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

    summary["coverage"] = {
        {"baseline", {
            {"globally_unique_states_visited", baselineGlobalStates.size()},
            {"globally_unique_state_action_pairs_visited", baselineGlobalVisitedPairs.size()},
            {"valid_state_action_pairs_available_on_encountered_states", baselineGlobalAvailablePairs.size()},
            {"valid_state_action_pair_coverage_on_encountered_states",
                baselineGlobalAvailablePairs.empty() ? 0.0 :
                static_cast<double>(baselineGlobalVisitedPairs.size()) / baselineGlobalAvailablePairs.size()}
        }},
        {"proposed", {
            {"globally_unique_states_visited", proposedGlobalStates.size()},
            {"globally_unique_state_action_pairs_visited", proposedGlobalVisitedPairs.size()},
            {"valid_state_action_pairs_available_on_encountered_states", proposedGlobalAvailablePairs.size()},
            {"valid_state_action_pair_coverage_on_encountered_states",
                proposedGlobalAvailablePairs.empty() ? 0.0 :
                static_cast<double>(proposedGlobalVisitedPairs.size()) / proposedGlobalAvailablePairs.size()}
        }}
    };
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
    summary["synthetic_user_diagnostics"] = {
        {"user_model",userModel},
        {"baseline",{{"positive_responses",baselineDiagnosticAggregate.synthetic_positive_responses},
                     {"negative_responses",baselineDiagnosticAggregate.synthetic_negative_responses},
                     {"no_responses",baselineDiagnosticAggregate.synthetic_no_responses},
                     {"requests_observed",baselineDiagnosticAggregate.synthetic_requests_observed},
                     {"requests_acknowledged",baselineDiagnosticAggregate.synthetic_requests_acknowledged},
                     {"requests_pending_at_episode_end",baselineDiagnosticAggregate.synthetic_requests_pending_at_end}}},
        {"proposed",{{"positive_responses",proposedDiagnosticAggregate.synthetic_positive_responses},
                     {"negative_responses",proposedDiagnosticAggregate.synthetic_negative_responses},
                     {"no_responses",proposedDiagnosticAggregate.synthetic_no_responses},
                     {"requests_observed",proposedDiagnosticAggregate.synthetic_requests_observed},
                     {"requests_acknowledged",proposedDiagnosticAggregate.synthetic_requests_acknowledged},
                     {"requests_pending_at_episode_end",proposedDiagnosticAggregate.synthetic_requests_pending_at_end}}},
        {"note","Counts follow each agent's realised actions. Profiles and exogenous per-step draws are paired, but responses may diverge after actions differ."}
    };
    summary["training_diagnostics"] = {
        {"aggregation_level", "all decisions across all completed episodes in this run"},
        {"baseline", trainingDiagnosticsToJson(baselineDiagnosticAggregate)},
        {"proposed", trainingDiagnosticsToJson(proposedDiagnosticAggregate)},
        {"metric_definitions", {
            {"insist_predicate_eligible_decisions", "Count of observations for which isActionValidForObservation(INSIST, observation) is true."},
            {"insist_valid_action_available_decisions", "Count of observations where INSIST appears in validActionsForObservation."},
            {"insist_policy_selections_before_overrides", "Count where the policy action before proposed-agent overrides is INSIST."},
            {"insist_final_selections_after_overrides", "Count where the final selected action is INSIST."},
            {"insist_environment_execution_attempts", "Count of INSIST actions passed into applyEnvironmentAction; this is an attempt, not proof of acceptance."},
            {"repetition_penalty_trigger_count", "Count of decisions where the shared repetition penalty is greater than zero."},
            {"within_state_valid_action_value_spread", "Maximum minus minimum Q estimate across valid actions for a decision with at least one stored valid action value; unvisited action entries may have the QTable default value."},
            {"repeated_action_vs_best_alternative_gap", "Best alternative valid-action Q estimate minus the repeated action Q estimate, sampled on repeated-category decisions with Q-table support; positive means the alternative has higher Q."}
        }}
    };
    summary["diagnostic_artifacts"] = {
        {"event_log", mode + "_diagnostic_events.jsonl"},
        {"run_manifest", mode + "_run_manifest.json"},
        {"baseline_step_log", mode + "_baseline_steps.jsonl"},
        {"proposed_step_log", mode + "_proposed_steps.jsonl"}
    };

    std::ofstream summaryFile(outDir / (mode + "_summary.json"));
    if (!summaryFile)
    {
        std::cerr << "Unable to write experiment summary in " << outDir << '\n';
        return false;
    }
    summaryFile << summary.dump(2) << '\n';
    summaryFile.close();

    nlohmann::json manifest;
    manifest["schema_version"] = 1;
    manifest["run_id"] = mode + "-seed-" + std::to_string(seed) + "-" +
        std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    manifest["started_at_utc"] = runStartedAtUtc;
    manifest["completed_at_utc"] = utcTimestampNow();
    manifest["mode"] = mode;
    manifest["user_model"] = userModel;
    manifest["seed"] = seed;
    manifest["environment_seed"] = environmentSeed;
    manifest["episodes_requested"] = episodes;
    manifest["max_steps_per_episode"] = steps;
    manifest["command_arguments"] = commandArguments;
    manifest["output_directory"] = outDir.string();
    manifest["build"] = buildProvenanceJson();
    manifest["artifacts"] = {
        {"summary", (outDir / (mode + "_summary.json")).string()},
        {"event_log", (outDir / (mode + "_diagnostic_events.jsonl")).string()},
        {"baseline_step_log", (outDir / (mode + "_baseline_steps.jsonl")).string()},
        {"proposed_step_log", (outDir / (mode + "_proposed_steps.jsonl")).string()}
    };
    for (const auto& artifactName : {
            mode + "_summary.json",
            mode + "_diagnostic_events.jsonl",
            mode + "_baseline_steps.jsonl",
            mode + "_proposed_steps.jsonl"}) {
        const fs::path artifactPath = outDir / artifactName;
        if (!fs::exists(artifactPath)) continue;
        std::error_code artifactError;
        const auto artifactSize = fs::file_size(artifactPath, artifactError);
        manifest["artifact_files"][artifactName] = {
            {"path", artifactPath.string()},
            {"fnv1a64", fnv1a64FileHash(artifactPath)}
        };
        if (!artifactError)
            manifest["artifact_files"][artifactName]["size_bytes"] = artifactSize;
    }
    if (fs::exists(baselinePolicy)) {
        manifest["policy_files"]["baseline"] = {
            {"path", baselinePolicy.string()},
            {"fnv1a64", fnv1a64FileHash(baselinePolicy)}
        };
    }
    if (fs::exists(proposedPolicy)) {
        manifest["policy_files"]["proposed"] = {
            {"path", proposedPolicy.string()},
            {"fnv1a64", fnv1a64FileHash(proposedPolicy)}
        };
    }
    std::ofstream manifestFile(outDir / (mode + "_run_manifest.json"));
    if (!manifestFile) {
        std::cerr << "Unable to write run manifest in " << outDir << '\n';
        return false;
    }
    manifestFile << manifest.dump(2) << '\n';

    std::cout << mode << " experiment complete (" << episodes << " episodes, seed "
              << seed << ").\n"
              << "Baseline mean reward per step: " << summary["baseline"]["mean_reward_per_step"] << '\n'
              << "Proposed mean reward per step: " << summary["proposed"]["mean_reward_per_step"] << '\n'
              << "Results: " << (outDir / (mode + "_summary.json")) << '\n'
              << "Diagnostic events: " << (outDir / (mode + "_diagnostic_events.jsonl")) << '\n'
              << "Run manifest: " << (outDir / (mode + "_run_manifest.json")) << '\n';

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

bool testFetchObjectPreferenceBias() {
    ProposedAgent ropeAgent;
    if (ropeAgent.getPreferredFetchObject() != "ball") return false;
    ropeAgent.recordUserStatement("likes", "rope");
    if (ropeAgent.getPreferredFetchObject() != "rope") return false;
    ProposedAgent stickAgent;
    stickAgent.recordUserStatement("likes", "stick");
    if (stickAgent.getPreferredFetchObject() != "stick") return false;
    ProposedAgent dislikeAgent;
    dislikeAgent.recordUserStatement("dislikes", "ball");
    return dislikeAgent.getPreferredFetchObject() == "stick";
}

bool testTrainingEpsilonDecay()
{
    const float early = computeTrainingEpsilon(0, 100);
    const float mid = computeTrainingEpsilon(50, 100);
    const float late = computeTrainingEpsilon(99, 100);
    return early >= 0.495f && early <= 0.505f &&
              mid >= 0.05f && mid <= 0.50f &&
              late >= 0.049f && late <= 0.051f;
}

bool testIntrinsicBonusCalculationParity()
{
    const float first = computeIntrinsicBonus(0);
    const float second = computeIntrinsicBonus(1);
    const float third = computeIntrinsicBonus(3);
    return std::abs(first - 0.02f) < 1e-6f &&
              std::abs(second - 0.02f / std::sqrt(2.0f)) < 1e-6f &&
              std::abs(third - 0.02f / std::sqrt(4.0f)) < 1e-6f;
}

bool runTrainingSelfTests()
{
    const bool epsilonPassed = testTrainingEpsilonDecay();
    std::cout << "Cosine epsilon decay remains within [0.05, 0.50]: "
              << (epsilonPassed ? "PASS" : "FAIL") << '\n';
    const bool intrinsicPassed = testIntrinsicBonusCalculationParity();
    std::cout << "Intrinsic exploration bonus computes with the specified eta and visit count: "
              << (intrinsicPassed ? "PASS" : "FAIL") << '\n';
    const bool fetchChoicePassed = testFetchObjectPreferenceBias();
    std::cout << "Remembered fetch-object preference biases object selection: "
              << (fetchChoicePassed ? "PASS" : "FAIL") << '\n';
    const bool scenarioGenerationPassed = testScenarioGenerationAndIgnoredRequest();
    std::cout << "Seeded scenario randomisation and ignored-request/INSIST eligibility: "
              << (scenarioGenerationPassed ? "PASS" : "FAIL") << '\n';
    const bool syntheticUserPassed = testSyntheticUserDeterminismAndLifecycle();
    std::cout << "Synthetic-user deterministic profile, paired draws, and request lifecycle: "
              << (syntheticUserPassed ? "PASS" : "FAIL") << '\n';
    const bool repetitionPenaltyPassed = testSharedRepetitionPenalty();
    std::cout << "Shared bounded repetition/boredom reward penalty: "
              << (repetitionPenaltyPassed ? "PASS" : "FAIL") << '\n';
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
    const EnvironmentObservation diagnosticObservation{1, 1, 0, 1, 0.82f};
    BaselineAgent diagnosticBaseline;
    ProposedAgent diagnosticProposed;
    diagnosticBaseline.setLearningEnabled(false);
    diagnosticProposed.setLearningEnabled(false);
    diagnosticBaseline.selectAction(diagnosticObservation);
    diagnosticProposed.selectAction(diagnosticObservation);
    const auto baselineQValues = diagnosticBaseline.getValidActionQValues(diagnosticObservation);
    const auto proposedQValues = diagnosticProposed.getLastDecisionTrace().valid_action_q_values;
    const bool comparableQDiagnostics = baselineQValues.size() == proposedQValues.size() &&
           !baselineQValues.empty() &&
           std::equal(baselineQValues.begin(), baselineQValues.end(), proposedQValues.begin(),
            [](const auto& lhs, const auto& rhs) { return lhs.first == rhs.first; });
    std::cout << "Comparable baseline/proposed valid-action Q diagnostics: "
              << (comparableQDiagnostics ? "PASS" : "FAIL") << '\n';
    return epsilonPassed && intrinsicPassed && fetchChoicePassed && scenarioGenerationPassed &&
              syntheticUserPassed && repetitionPenaltyPassed && comparableQDiagnostics &&
              energyDrainPassed && recoveryPriorityPassed && depletionTransitionPassed &&
              temporalDecayPassed && transitionsPassed && retryPassed &&
              taskPriorityPassed && completionContextPassed;
}