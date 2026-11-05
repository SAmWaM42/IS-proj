#include "questionnaire-submission.h"

#include <stdexcept>
#include <string>

namespace study {
namespace {

nlohmann::json toQuestionnaireResponse(const nlohmann::json& result) {
    if (!result.is_object() ||
        result.value("completion_status", std::string{}) != "completed" ||
        !result.contains("responses") || !result.at("responses").is_array()) {
        throw std::invalid_argument("Only a completed questionnaire can be submitted.");
    }

    nlohmann::json answers = nlohmann::json::array();
    for (const auto& row : result.at("responses")) {
        if (!row.is_object() || !row.contains("id") || !row.at("id").is_string() ||
            !row.contains("type") || !row.at("type").is_string() ||
            !row.contains("value")) {
            throw std::invalid_argument("Questionnaire result contains an incomplete answer.");
        }

        const std::string questionId = row.at("id").get<std::string>();
        const std::string sourceType = row.at("type").get<std::string>();
        const auto& value = row.at("value");
        std::string answerType;

        if (sourceType == "likert") {
            answerType = "likert";
            if (!value.is_number_integer())
                throw std::invalid_argument("Likert answers must be integers.");
        } else if (sourceType == "scale") {
            answerType = "integer";
            if (!value.is_number_integer())
                throw std::invalid_argument("Scale answers must be integers.");
        } else if (sourceType == "choice") {
            answerType = "single_choice";
            if (!value.is_string())
                throw std::invalid_argument("Choice answers must be strings.");
        } else if (sourceType == "text") {
            answerType = "text";
            if (!value.is_string())
                throw std::invalid_argument("Text answers must be strings.");
        } else {
            throw std::invalid_argument("Unsupported questionnaire answer type: " + sourceType);
        }

        if (questionId.empty())
            throw std::invalid_argument("Questionnaire answer has an empty question ID.");

        answers.push_back({
            {"question_id", questionId},
            {"answer_type", answerType},
            {"value", value}
        });
    }

    return {{"answers", std::move(answers)}};
}

std::string questionnaireVersion(const nlohmann::json& result) {
    if (!result.contains("context") || !result.at("context").is_object() ||
        !result.at("context").contains("questionnaire_version") ||
        !result.at("context").at("questionnaire_version").is_string()) {
        throw std::invalid_argument("Questionnaire version is missing from result context.");
    }
    const auto version = result.at("context").at("questionnaire_version").get<std::string>();
    if (version.empty())
        throw std::invalid_argument("Questionnaire version cannot be empty.");
    return version;
}

} // namespace

nlohmann::json makeSubmissionData(const nlohmann::json& conditionResult) {
    return {
        {"questionnaire_version", questionnaireVersion(conditionResult)},
        {"responses", {
            {"condition_questionnaire", toQuestionnaireResponse(conditionResult)},
            {"comparison_questionnaire", nullptr}
        }}
    };
}

nlohmann::json makeSubmissionData(const nlohmann::json& conditionResult,
                                  const nlohmann::json& comparisonResult) {
    const std::string version = questionnaireVersion(conditionResult);
    if (questionnaireVersion(comparisonResult) != version)
        throw std::invalid_argument("Condition and comparison questionnaire versions do not match.");

    return {
        {"questionnaire_version", version},
        {"responses", {
            {"condition_questionnaire", toQuestionnaireResponse(conditionResult)},
            {"comparison_questionnaire", toQuestionnaireResponse(comparisonResult)}
        }}
    };
}

} // namespace study
