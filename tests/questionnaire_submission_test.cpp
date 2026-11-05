#include "Components/Study/questionnaire-submission.h"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {
nlohmann::json completedForm(const std::string& tag, nlohmann::json answers) {
    return {
        {"completion_status", "completed"},
        {"context", {
            {"questionnaire_version", "companion-believability-short-v2"},
            {"session_index", (tag == "comparison" || tag == "rating-B") ? 2 : 1},
            {"condition_code", (tag == "comparison" || tag == "rating-B") ? "B" : "A"},
            {"tag", tag}
        }},
        {"responses", std::move(answers)}
    };
}
}

int main() {
    const auto condition = completedForm("rating-A", nlohmann::json::array({
        {{"id", "C1"}, {"type", "likert"}, {"value", 4}},
        {{"id", "summary_believability"}, {"type", "scale"}, {"value", 5}},
        {{"id", "cmp_choice"}, {"type", "choice"}, {"value", "Companion B"}},
        {{"id", "session_feedback"}, {"type", "text"}, {"value", "A short comment"}}
    }));

    const auto sessionOne = study::makeSubmissionData(condition);
    assert(sessionOne.at("questionnaire_version") == "companion-believability-short-v2");
    assert(sessionOne.at("responses").at("comparison_questionnaire").is_null());
    const auto& answers = sessionOne.at("responses").at("condition_questionnaire").at("answers");
    assert(answers.size() == 4);
    assert(answers[0].at("question_id") == "c1");
    assert(answers[0].at("answer_type") == "likert");
    assert(answers[1].at("answer_type") == "integer");
    assert(answers[2].at("answer_type") == "single_choice");
    assert(answers[3].at("answer_type") == "text");

    const auto comparison = completedForm("comparison", nlohmann::json::array({
        {{"id", "cmp_believable"}, {"type", "choice"}, {"value", "Companion A"}}
    }));
    const auto conditionB = completedForm("rating-B", nlohmann::json::array({
        {{"id", "C2"}, {"type", "likert"}, {"value", 3}}
    }));
    const auto sessionTwo = study::makeSubmissionData(conditionB, comparison);
    assert(sessionTwo.at("responses").at("comparison_questionnaire").at("answers").size() == 1);

    bool rejected = false;
    try {
        auto partial = condition;
        partial["completion_status"] = "in_progress";
        (void)study::makeSubmissionData(partial);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    rejected = false;
    try {
        auto invalid = condition;
        invalid["responses"][0]["type"] = "unsupported";
        (void)study::makeSubmissionData(invalid);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    std::cout << "Questionnaire submission adapter tests passed.\n";
    return 0;
}
