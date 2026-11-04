#include "study-session-data.h"

#include <algorithm>
#include <iomanip>
#include <set>
#include <sstream>
#include <type_traits>
#include <unordered_map>

namespace study {
namespace {

bool isNumericValue(const AnswerValue& value, double& output) {
    if (const auto* integer = std::get_if<std::int64_t>(&value)) {
        output = static_cast<double>(*integer);
        return true;
    }
    if (const auto* number = std::get_if<double>(&value)) {
        output = *number;
        return true;
    }
    return false;
}

bool valueMatchesType(AnswerType type, const AnswerValue& value) {
    switch (type) {
        case AnswerType::Likert:
        case AnswerType::Integer:
            return std::holds_alternative<std::int64_t>(value);
        case AnswerType::Number:
            return std::holds_alternative<std::int64_t>(value) ||
                   std::holds_alternative<double>(value);
        case AnswerType::SingleChoice:
        case AnswerType::Text:
            return std::holds_alternative<std::string>(value);
        case AnswerType::MultipleChoice:
            return std::holds_alternative<std::vector<std::string>>(value);
        case AnswerType::Boolean:
            return std::holds_alternative<bool>(value);
    }
    return false;
}

std::vector<std::string> validateSection(
    const QuestionnaireResponse& response,
    const std::vector<QuestionDefinition>& questions,
    const std::string& sectionName) {

    std::vector<std::string> errors;
    std::unordered_map<std::string, const QuestionDefinition*> definitions;
    for (const auto& question : questions) definitions.emplace(question.id, &question);

    std::set<std::string> seen;
    for (const auto& answer : response.answers) {
        if (!seen.insert(answer.questionId).second) {
            errors.push_back(sectionName + ": duplicate answer for question '" +
                             answer.questionId + "'.");
            continue;
        }

        const auto found = definitions.find(answer.questionId);
        if (found == definitions.end()) {
            errors.push_back(sectionName + ": unknown question ID '" +
                             answer.questionId + "'.");
            continue;
        }

        const auto& definition = *found->second;
        if (answer.type != definition.type || !valueMatchesType(definition.type, answer.value)) {
            errors.push_back(sectionName + ": answer type mismatch for question '" +
                             answer.questionId + "'.");
            continue;
        }

        if (definition.type == AnswerType::Likert || definition.type == AnswerType::Integer ||
            definition.type == AnswerType::Number) {
            double numeric = 0.0;
            if (!isNumericValue(answer.value, numeric)) continue;
            if (definition.minimum && numeric < *definition.minimum)
                errors.push_back(sectionName + ": answer below allowed minimum for '" +
                                 answer.questionId + "'.");
            if (definition.maximum && numeric > *definition.maximum)
                errors.push_back(sectionName + ": answer above allowed maximum for '" +
                                 answer.questionId + "'.");
        } else if (definition.type == AnswerType::SingleChoice) {
            const auto& selected = std::get<std::string>(answer.value);
            if (std::find(definition.options.begin(), definition.options.end(), selected) ==
                definition.options.end())
                errors.push_back(sectionName + ": invalid option for question '" +
                                 answer.questionId + "'.");
        } else if (definition.type == AnswerType::MultipleChoice) {
            const auto& selected = std::get<std::vector<std::string>>(answer.value);
            std::set<std::string> unique;
            if (selected.empty())
                errors.push_back(sectionName + ": empty selection for question '" +
                                 answer.questionId + "'.");
            for (const auto& option : selected) {
                if (!unique.insert(option).second)
                    errors.push_back(sectionName + ": duplicate option for question '" +
                                     answer.questionId + "'.");
                if (std::find(definition.options.begin(), definition.options.end(), option) ==
                    definition.options.end())
                    errors.push_back(sectionName + ": invalid option for question '" +
                                     answer.questionId + "'.");
            }
        } else if (definition.type == AnswerType::Text) {
            const auto& text = std::get<std::string>(answer.value);
            if (text.size() > definition.maxLength)
                errors.push_back(sectionName + ": text answer exceeds maximum length for '" +
                                 answer.questionId + "'.");
            if (definition.required && text.find_first_not_of(" \t\r\n") == std::string::npos)
                errors.push_back(sectionName + ": required text answer is blank for '" +
                                 answer.questionId + "'.");
        }
    }

    for (const auto& question : questions) {
        if (question.required && seen.find(question.id) == seen.end())
            errors.push_back(sectionName + ": missing required answer for '" + question.id + "'.");
    }
    return errors;
}

nlohmann::json responseToJson(const QuestionnaireResponse& response) {
    nlohmann::json answers = nlohmann::json::array();
    for (const auto& answer : response.answers) {
        answers.push_back({
            {"question_id", answer.questionId},
            {"answer_type", toString(answer.type)},
            {"value", answerValueToJson(answer.value)}
        });
    }
    return {{"answers", std::move(answers)}};
}

} // namespace

std::string toString(AnswerType type) {
    switch (type) {
        case AnswerType::Likert: return "likert";
        case AnswerType::Integer: return "integer";
        case AnswerType::Number: return "number";
        case AnswerType::SingleChoice: return "single_choice";
        case AnswerType::MultipleChoice: return "multiple_choice";
        case AnswerType::Text: return "text";
        case AnswerType::Boolean: return "boolean";
    }
    return "unknown";
}

nlohmann::json answerValueToJson(const AnswerValue& value) {
    return std::visit([](const auto& item) -> nlohmann::json {
        return item;
    }, value);
}

std::string StudySessionData::submissionId() const {
    std::ostringstream id;
    id << 'P' << std::setw(2) << std::setfill('0') << participantNumber
       << "-S" << sessionIndex;
    return id.str();
}

std::vector<std::string> StudySessionData::validate(
    const QuestionnaireDefinition& definition) const {

    std::vector<std::string> errors;
    if (participantNumber < 1 || participantNumber > 9999)
        errors.push_back("participant_number must be between 1 and 9999.");
    if (sessionIndex != 1 && sessionIndex != 2)
        errors.push_back("session_index must be 1 or 2.");
    if (condition != 'A' && condition != 'B')
        errors.push_back("condition must be A or B.");

    if (participantNumber >= 1 && participantNumber <= 9999 &&
        (sessionIndex == 1 || sessionIndex == 2)) {
        const char expected = participantNumber % 2 == 1
            ? (sessionIndex == 1 ? 'A' : 'B')
            : (sessionIndex == 1 ? 'B' : 'A');
        if (condition != expected)
            errors.push_back("condition does not match the participant's counterbalanced assignment.");
    }

    if (definition.version.empty())
        errors.push_back("questionnaire definition version must not be empty.");
    if (questionnaireVersion != definition.version)
        errors.push_back("questionnaire_version does not match the active questionnaire definition.");

    auto conditionErrors = validateSection(conditionQuestionnaire,
                                            definition.conditionQuestions,
                                            "condition_questionnaire");
    errors.insert(errors.end(), conditionErrors.begin(), conditionErrors.end());

    if (sessionIndex == 1 && comparisonQuestionnaire.has_value())
        errors.push_back("comparison_questionnaire must be absent from session 1.");
    if (sessionIndex == 2 && !comparisonQuestionnaire.has_value())
        errors.push_back("comparison_questionnaire is required in session 2.");

    if (comparisonQuestionnaire) {
        auto comparisonErrors = validateSection(*comparisonQuestionnaire,
                                                definition.comparisonQuestions,
                                                "comparison_questionnaire");
        errors.insert(errors.end(), comparisonErrors.begin(), comparisonErrors.end());
    }
    return errors;
}

nlohmann::json StudySessionData::toJson() const {
    nlohmann::json comparison = nullptr;
    if (comparisonQuestionnaire)
        comparison = responseToJson(*comparisonQuestionnaire);

    return {
        {"schema_version", 1},
        {"submission_id", submissionId()},
        {"participant_number", participantNumber},
        {"session_index", sessionIndex},
        {"condition", std::string(1, condition)},
        {"questionnaire_version", questionnaireVersion},
        {"responses", {
            {"condition_questionnaire", responseToJson(conditionQuestionnaire)},
            {"comparison_questionnaire", std::move(comparison)}
        }}
    };
}

} // namespace study
