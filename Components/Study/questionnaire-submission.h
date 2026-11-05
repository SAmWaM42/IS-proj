#pragma once

#include <json.hpp>

namespace study {

// Convert the active SFML questionnaire View result into the versioned
// questionnaire-submission contract consumed by SubmissionManager and the API.
nlohmann::json makeSubmissionData(const nlohmann::json& conditionResult);
nlohmann::json makeSubmissionData(const nlohmann::json& conditionResult,
                                  const nlohmann::json& comparisonResult);

} // namespace study
