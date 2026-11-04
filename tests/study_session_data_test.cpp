#include "Components/Study/study-session-data.h"

#include <cassert>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

namespace {
study::QuestionnaireDefinition testDefinition() {
    using study::AnswerType;
    study::QuestionnaireDefinition definition;
    definition.version = "test-fixture-v1";
    definition.conditionQuestions = {
        {"perceived_believability_test", AnswerType::Likert, true, 1, 5, {}, 2000},
        {"interaction_choice_test", AnswerType::SingleChoice, true, {}, {},
         {"choice_one", "choice_two"}, 2000},
        {"optional_comment_test", AnswerType::Text, false, {}, {}, {}, 40}
    };
    definition.comparisonQuestions = {
        {"comparison_rating_test", AnswerType::Integer, true, 1, 7, {}, 2000},
        {"comparison_preference_test", AnswerType::MultipleChoice, true, {}, {},
         {"option_a", "option_b"}, 2000}
    };
    return definition;
}

study::StudySessionData validSessionOne() {
    using namespace study;
    StudySessionData data;
    data.participantNumber = 3;
    data.sessionIndex = 1;
    data.condition = 'A';
    data.questionnaireVersion = "test-fixture-v1";
    data.conditionQuestionnaire.answers = {
        {"perceived_believability_test", AnswerType::Likert, std::int64_t{4}},
        {"interaction_choice_test", AnswerType::SingleChoice, std::string{"choice_one"}}
    };
    return data;
}

study::StudySessionData validSessionTwo() {
    using namespace study;
    StudySessionData data;
    data.participantNumber = 4;
    data.sessionIndex = 2;
    data.condition = 'A';
    data.questionnaireVersion = "test-fixture-v1";
    data.conditionQuestionnaire.answers = {
        {"perceived_believability_test", AnswerType::Likert, std::int64_t{3}},
        {"interaction_choice_test", AnswerType::SingleChoice, std::string{"choice_two"}}
    };
    data.comparisonQuestionnaire = QuestionnaireResponse{{
        {"comparison_rating_test", AnswerType::Integer, std::int64_t{6}},
        {"comparison_preference_test", AnswerType::MultipleChoice,
         std::vector<std::string>{"option_a", "option_b"}}
    }};
    return data;
}

void validPayloadsAndAssignment() {
    const auto definition = testDefinition();
    const auto first = validSessionOne();
    const auto second = validSessionTwo();
    assert(first.validate(definition).empty());
    assert(second.validate(definition).empty());
    assert(first.submissionId() == "P03-S1");
    assert(second.submissionId() == "P04-S2");

    const auto firstJson = first.toJson();
    assert(firstJson.at("responses").at("comparison_questionnaire").is_null());
    assert(firstJson.at("responses").at("condition_questionnaire").at("answers").size() == 2);

    const auto secondJson = second.toJson();
    assert(secondJson.at("responses").at("comparison_questionnaire").is_object());
    assert(secondJson.at("responses").at("comparison_questionnaire").at("answers").size() == 2);
    assert(secondJson.at("questionnaire_version") == "test-fixture-v1");
}

void invalidResponsesAreRejected() {
    const auto definition = testDefinition();

    auto missing = validSessionOne();
    missing.conditionQuestionnaire.answers.pop_back();
    assert(!missing.validate(definition).empty());

    auto outOfRange = validSessionOne();
    outOfRange.conditionQuestionnaire.answers[0].value = std::int64_t{6};
    assert(!outOfRange.validate(definition).empty());

    auto invalidOption = validSessionOne();
    invalidOption.conditionQuestionnaire.answers[1].value = std::string{"not_an_option"};
    assert(!invalidOption.validate(definition).empty());

    auto duplicate = validSessionOne();
    duplicate.conditionQuestionnaire.answers.push_back(duplicate.conditionQuestionnaire.answers[0]);
    assert(!duplicate.validate(definition).empty());

    auto wrongType = validSessionOne();
    wrongType.conditionQuestionnaire.answers[0].value = std::string{"four"};
    assert(!wrongType.validate(definition).empty());

    auto wrongAssignment = validSessionOne();
    wrongAssignment.condition = 'B';
    assert(!wrongAssignment.validate(definition).empty());

    auto missingComparison = validSessionTwo();
    missingComparison.comparisonQuestionnaire.reset();
    assert(!missingComparison.validate(definition).empty());

    auto unexpectedComparison = validSessionOne();
    unexpectedComparison.comparisonQuestionnaire = study::QuestionnaireResponse{};
    assert(!unexpectedComparison.validate(definition).empty());

    auto oversizedText = validSessionOne();
    oversizedText.conditionQuestionnaire.answers.push_back({
        "optional_comment_test", study::AnswerType::Text,
        std::string(41, 'x')
    });
    assert(!oversizedText.validate(definition).empty());
}
}

int main() {
    validPayloadsAndAssignment();
    invalidResponsesAreRejected();
    std::cout << "[PASS] questionnaire response contract and validation\n";
}
