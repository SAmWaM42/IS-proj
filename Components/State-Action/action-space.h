#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "Components/Memory/memory.h"

// One canonical action vocabulary is shared by training, both agents, and the UI.
inline const std::vector<std::string>& companionActionSet() {
    static const std::vector<std::string> actions = {
        "FETCH", "WANDER", "BARK", "ACT_CONFUSED", "SLEEP_LAY",
        "NUZZLE", "INSIST", "GREETING", "APPROACH_FOLLOW", "SNIFF",
        "WHINE", "HIDE_AVOID", "EAT", "PLAY"
    };
    return actions;
}

// Availability depends only on observable environment context, never on hidden
// simulated-user traits or on whether the agent is baseline/proposed.
inline bool isActionValidForObservation(
    const std::string& action,
    const EnvironmentObservation& observation) {
    if (action == "FETCH")
        // Distance affects effort/success in the environment; only an unavailable
        // target is a hard physical constraint.
        return observation.task_status == 1 && observation.target_proximity < 3;
    if (action == "NUZZLE")
        return observation.target_proximity == 0 && observation.user_status != 2;
    if (action == "GREETING")
        return observation.user_status == 0 || (observation.goal_state == 0 && observation.user_status != 2);
    if (action == "BARK")
        return observation.signal_strength >= 0.80f;
    if (action == "SLEEP_LAY")
        return observation.goal_state == 2 && observation.task_status == 0 &&
               observation.signal_strength < 0.80f;
    if (action == "INSIST")
        return observation.task_status == 1 && observation.user_status == 0;
    if (action == "APPROACH_FOLLOW")
        return observation.target_proximity > 0 && observation.user_status != 2;
    if (action == "SNIFF")
        return observation.target_proximity <= 1;
    if (action == "WHINE")
        return observation.user_status == 2 || observation.signal_strength >= 0.80f;
    if (action == "HIDE_AVOID")
        return observation.user_status == 2 || observation.signal_strength >= 0.90f;
    if (action == "EAT")
        return observation.food_available;
    if (action == "PLAY")
        return observation.play_available && observation.user_status != 2;

    // WANDER and ACT_CONFUSED are safe general-purpose fallback behaviours.
    return action == "WANDER" || action == "ACT_CONFUSED";
}

inline std::vector<std::string> validActionsForObservation(
    const EnvironmentObservation& observation) {
    std::vector<std::string> valid;
    for (const auto& action : companionActionSet()) {
        if (isActionValidForObservation(action, observation))
            valid.push_back(action);
    }
    if (valid.empty())
        valid.push_back("WANDER");
    return valid;
}

inline std::string validActionOrFallback(
    const std::string& proposedAction,
    const std::string& contextualFallback,
    const EnvironmentObservation& observation) {
    if (isActionValidForObservation(proposedAction, observation))
        return proposedAction;
    if (isActionValidForObservation(contextualFallback, observation))
        return contextualFallback;
    for (const auto& action : companionActionSet()) {
        if (isActionValidForObservation(action, observation))
            return action;
    }
    return "WANDER";
}

inline std::string actionLabel(const std::string& action) {
    if (action == "FETCH") return "Fetches the nearby object";
    if (action == "WANDER") return "Explores the environment";
    if (action == "BARK") return "Vocalises";
    if (action == "ACT_CONFUSED") return "Pauses and reorients";
    if (action == "SLEEP_LAY") return "Lies down to rest";
    if (action == "NUZZLE") return "Moves closer affectionately";
    if (action == "INSIST") return "Repeats or insists on a behaviour";
    if (action == "GREETING") return "Offers a greeting";
    if (action == "APPROACH_FOLLOW") return "Approaches or follows";
    if (action == "SNIFF") return "Sniffs the nearby area";
    if (action == "WHINE") return "Whines";
    if (action == "HIDE_AVOID") return "Hides or moves away";
    if (action == "EAT") return "Eats offered food";
    if (action == "PLAY") return "Invites or engages in play";
    return "Responds in its own way";
}
