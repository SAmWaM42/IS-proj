#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <json.hpp>

namespace study {

enum class AnswerType {
    Likert,
    Integer,
    Number,
    SingleChoice,
    MultipleChoice,
    Text,
    Boolean
};

using AnswerValue = std::variant<std::int64_t, double, std::string, bool,
                                 std::vector<std::string>>;

struct QuestionDefinition {
    std::string id;
    AnswerType type = AnswerType::Text;
    bool required = true;
    std::optional<double> minimum;
    std::optional<double> maximum;
    std::vector<std::string> options;
    std::size_t maxLength = 2000;
};

struct QuestionnaireDefinition {
    std::string version;
    std::vector<QuestionDefinition> conditionQuestions;
    std::vector<QuestionDefinition> comparisonQuestions;
};

struct QuestionAnswer {
    std::string questionId;
    AnswerType type = AnswerType::Text;
    AnswerValue value = std::string{};
};

struct QuestionnaireResponse {
    std::vector<QuestionAnswer> answers;
};

class StudySessionData {
public:
    int participantNumber = 0;
    int sessionIndex = 0;
    char condition = '\0';
    std::string questionnaireVersion;
    QuestionnaireResponse conditionQuestionnaire;
    std::optional<QuestionnaireResponse> comparisonQuestionnaire;

    std::string submissionId() const;
    std::vector<std::string> validate(const QuestionnaireDefinition& definition) const;
    nlohmann::json toJson() const;
};

std::string toString(AnswerType type);
nlohmann::json answerValueToJson(const AnswerValue& value);

} // namespace study
