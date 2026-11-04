#include "Components/Study/study-session-data.h"
#include "Components/Study/questionnaire-definition.h"

#include <cassert>
#include <iostream>
#include <limits>
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

    auto unknownQuestion = validSessionOne();
    unknownQuestion.conditionQuestionnaire.answers.push_back({
        "not_in_questionnaire", study::AnswerType::Boolean, true
    });
    assert(!unknownQuestion.validate(definition).empty());

    auto numericDefinition = definition;
    numericDefinition.conditionQuestions.push_back(
        {"optional_number_test", study::AnswerType::Number, false, {}, {}, {}, 2000});
    auto nonFinite = validSessionOne();
    nonFinite.conditionQuestionnaire.answers.push_back({
        "optional_number_test", study::AnswerType::Number,
        std::numeric_limits<double>::quiet_NaN()
    });
    assert(!nonFinite.validate(numericDefinition).empty());

    auto duplicateDefinition = definition;
    duplicateDefinition.conditionQuestions.push_back(
        duplicateDefinition.conditionQuestions.front());
    assert(!validSessionOne().validate(duplicateDefinition).empty());
}
}

void companionQuestionnaireDefinitionIsComplete() {
    const auto definition = study::companionBelievabilityQuestionnaire();
    assert(definition.version == "companion-believability-v1");
    assert(definition.conditionQuestions.size() == 26);
    assert(definition.comparisonQuestions.size() == 8);

    for (const auto& question : definition.conditionQuestions) {
        assert(!question.id.empty());
        assert(!question.prompt.empty());
        if (question.type == study::AnswerType::Likert) {
            assert(question.minimum && *question.minimum == 1);
            assert(question.maximum && *question.maximum == 5);
            assert(question.scaleLabels.size() == 5);
        }
    }
    for (const auto& question : definition.comparisonQuestions) {
        assert(!question.id.empty());
        assert(!question.prompt.empty());
    }
    assert(definition.conditionQuestions[20].id == "attention_check_agree");
    assert(definition.conditionQuestions[24].id == "session_technical_issue");
    assert(definition.comparisonQuestions[5].id == "comparison_blinding_check");
    assert(definition.conditionQuestions[21].scaleLabels.front() == "Not at all believable");
    assert(definition.comparisonQuestions[4].scaleLabels.front() == "I was guessing");
}

int main() {
    validPayloadsAndAssignment();
    invalidResponsesAreRejected();
    companionQuestionnaireDefinitionIsComplete();
    std::cout << "[PASS] questionnaire response contract and validation\n";
}
