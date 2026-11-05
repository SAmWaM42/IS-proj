// Questionnaire content. Edit the wording here; the View in questionnaire.cpp draws whatever it is given.
// Item ids match the scoring key in the Google Form template (C1..C21, 3.x, 4.x).

#include "questionnaire.h"

#include <algorithm>
#include <random>
#include <utility>

namespace questionnaire {
namespace {

Question likert(const std::string& id, const std::string& text, const std::string& construct, bool reverse = false) {
    Question q;
    q.id = id;
    q.type = QuestionType::Likert;
    q.text = text;
    q.construct = construct;
    q.reverseScored = reverse;
    return q;
}

Question choice(const std::string& id, const std::string& text, std::vector<std::string> options, bool required = true) {
    Question q;
    q.id = id;
    q.type = QuestionType::Choice;
    q.text = text;
    q.options = std::move(options);
    q.required = required;
    return q;
}

Question scale(const std::string& id, const std::string& text, const std::string& low, const std::string& high) {
    Question q;
    q.id = id;
    q.type = QuestionType::Scale;
    q.text = text;
    q.lowLabel = low;
    q.highLabel = high;
    return q;
}

Question freeText(const std::string& id, const std::string& text, bool paragraph, bool required = false) {
    Question q;
    q.id = id;
    q.type = QuestionType::Text;
    q.text = text;
    q.paragraph = paragraph;
    q.required = required;
    q.maxLength = paragraph ? 400 : 80;
    return q;
}

Question info(const std::string& text) {
    Question q;
    q.id = "info";
    q.type = QuestionType::Info;
    q.text = text;
    q.required = false;
    return q;
}

}  // namespace

Definition makeConsentAndBackground() {
    Definition d;
    d.id = "consent_background";
    d.title = "Before you begin";

    Page consent;
    consent.title = "About this study";
    consent.description = "Please read this carefully. You can stop at any time.";
    consent.questions.push_back(info(
        "This study looks at how lifelike two virtual animal companions seem. You will interact with each companion "
        "for about ten minutes, answer questions about it afterwards, and compare the two at the end. Taking part is "
        "voluntary. We do not collect your name or email address. Your answers are stored under a participant code, "
        "sent over an encrypted connection to a private research folder, and used only for this project at [institution]. "
        "Results are reported as group summaries.\n"
        "Contact: [researcher name and email]. Ethics reference: [reference]."));
    consent.questions.push_back(choice("consent", "I am 18 or older, I have read the information above, and I agree to take part.",
                                       {"Yes, I agree", "No, I do not agree"}));
    d.pages.push_back(consent);

    // Demographic and familiarity questions are intentionally omitted in line
    // with the current study protocol decision. Add them only if the protocol changes.
    return d;
}

Definition makeCompanionRating(char companionCode, unsigned int shuffleSeed) {
    const std::string name = std::string("Companion ") + companionCode;
    Definition d;
    d.id = std::string("rating_") + companionCode;
    d.title = name + ": your experience";

    // A concise ten-item instrument: core consistency, adaptability, memory,
    // affect and believability indicators, one attention check, one overall
    // rating, and one optional combined feedback/technical-issue prompt.
    std::vector<Question> rows = {
        likert("C1", "The companion's personality felt stable throughout the session.", "Behavioural consistency"),
        likert("C3", "The companion's behaviour seemed random or contradictory.", "Behavioural consistency", true),
        likert("C4", "The companion's behaviour changed in sensible ways depending on how I treated it.", "Adaptability"),
        likert("C5", "The companion seemed to remember how I had treated it earlier.", "Memory"),
        likert("C11", "I could tell what kind of mood the companion was in from how it behaved.", "Emotional expression"),
        likert("C13", "The companion's reactions seemed flat or emotionless.", "Emotional expression", true),
        likert("C18", "The companion seemed like a believable animal.", "Overall believability")
    };
    Question attention = likert("C21", "To show you are reading carefully, please select Agree for this row.", "");
    attention.attentionCheck = true;
    attention.attentionAnswer = 4;
    rows.push_back(attention);

    std::mt19937 rng(shuffleSeed);
    std::shuffle(rows.begin(), rows.end(), rng);
    for (int p = 0; p < 2; ++p) {
        Page page;
        page.title = "Your experience (" + std::to_string(p + 1) + " of 2)";
        page.description = p == 0
            ? "Think about " + name + ", the companion you just used. Choose how much you agree with each statement."
            : "Continue rating " + name + ". There are no right or wrong answers.";
        const std::size_t first = static_cast<std::size_t>(p) * 4;
        const std::size_t last = std::min(first + 4, rows.size());
        for (std::size_t i = first; i < last; ++i) page.questions.push_back(rows[i]);
        d.pages.push_back(std::move(page));
    }

    Page overall;
    overall.title = "Overall impression";
    overall.questions.push_back(scale("summary_believability", "Overall, how believable was " + name + " as an animal?",
                                      "Not at all believable", "Extremely believable"));
    overall.questions.push_back(freeText(
        "session_feedback",
        "Did you encounter a technical problem or notice anything else that affected your experience? (Optional)",
        true));
    d.pages.push_back(std::move(overall));
    return d;
}

Definition makeComparison() {
    Definition d;
    d.id = "comparison";
    d.title = "Comparing Companion A and Companion B";
    const std::vector<std::string> pick = {"Companion A", "Companion B", "No noticeable difference"};

    Page first;
    first.title = "Comparing the companions (1 of 2)";
    first.description = "Answer based on your experience with both companions.";
    first.questions.push_back(choice("cmp_consistent", "Which companion behaved more consistently?", pick));
    first.questions.push_back(choice("cmp_adapted", "Which companion adapted better to how you treated it?", pick));
    first.questions.push_back(choice("cmp_believable", "Which companion was more believable as an animal overall?", pick));
    first.questions.push_back(scale("cmp_confidence", "How confident are you in your answer about believability?",
                                    "I was guessing", "Very sure"));
    d.pages.push_back(std::move(first));

    Page last;
    last.title = "Final thoughts (2 of 2)";
    last.questions.push_back(choice("cmp_built_differently", "Did you think the two companions were built differently?",
                                    {"Yes", "No", "Not sure"}));
    Question differenceDetail = freeText("cmp_difference_detail",
        "If yes or not sure, what did you think was different? (Optional)", true);
    differenceDetail.visibleWhenQuestionId = "cmp_built_differently";
    differenceDetail.visibleWhenValues = {"Yes", "Not sure"};
    last.questions.push_back(std::move(differenceDetail));
    last.questions.push_back(freeText("cmp_other_comments", "Anything else you would like to tell us? (Optional)", true));
    d.pages.push_back(std::move(last));
    return d;
}

}  // namespace questionnaire
