// Questionnaire content. Edit the wording here; the View in questionnaire.cpp draws whatever it is given.
// Item ids match the scoring key in the Google Form template (C1..C21, 3.x, 4.x).

#include "questionnaire.h"

#include <algorithm>
#include <random>

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

    Page about;
    about.title = "About you";
    about.questions.push_back(choice("age_range", "What is your age range?",
                                     {"18 to 24", "25 to 34", "35 to 44", "45 or older", "Prefer not to say"}));
    about.questions.push_back(choice("pet_experience", "Have you owned or regularly cared for a pet?",
                                     {"Currently", "In the past", "Never"}));
    d.pages.push_back(about);

    Page familiarity;
    familiarity.title = "About you";
    familiarity.questions.push_back(scale("game_familiarity", "How familiar are you with virtual pets or pet-style games?",
                                          "Not at all familiar", "Very familiar"));
    familiarity.questions.push_back(scale("computer_comfort", "How comfortable are you using a computer with a mouse and keyboard?",
                                          "Not at all comfortable", "Very comfortable"));
    d.pages.push_back(familiarity);
    return d;
}

Definition makeCompanionRating(char companionCode, unsigned int shuffleSeed) {
    const std::string name = std::string("Companion ") + companionCode;
    Definition d;
    d.id = std::string("rating_") + companionCode;
    d.title = name + ": your experience";

    // Scale items in canonical order (ids C1..C21). Reverse-worded rows and the attention check are marked.
    std::vector<Question> rows = {
        likert("C1", "The companion's personality felt stable throughout the session.", "Behavioural consistency"),
        likert("C2", "The companion reacted in similar ways to similar things I did.", "Behavioural consistency"),
        likert("C3", "The companion's behaviour seemed random or contradictory.", "Behavioural consistency", true),
        likert("C4", "The companion's behaviour changed in sensible ways depending on how I treated it.", "Adaptability"),
        likert("C5", "The companion seemed to remember how I had treated it earlier.", "Adaptability"),
        likert("C6", "The companion responded the same way no matter what had happened before.", "Adaptability", true),
        likert("C7", "When I told the companion something about my preferences, its later behaviour seemed to reflect it.", "Statement response"),
        likert("C8", "The companion did things on its own, not only in response to my buttons.", "Autonomy and variation"),
        likert("C9", "The companion's behaviour was varied and did not repeat the same responses.", "Autonomy and variation"),
        likert("C10", "The companion felt scripted, like a machine running through commands.", "Autonomy and variation", true),
        likert("C11", "I could tell what kind of mood the companion was in from how it behaved.", "Emotional expression"),
        likert("C12", "The companion's reactions suited the situation.", "Emotional expression"),
        likert("C13", "The companion's reactions seemed flat or emotionless.", "Emotional expression", true),
        likert("C14", "When the companion did not do what I asked, its behaviour still made sense.", "Contextual appropriateness"),
        likert("C15", "I wanted to keep interacting with the companion.", "Engagement"),
        likert("C16", "I kept paying attention to what the companion was doing.", "Engagement"),
        likert("C17", "I became bored during the session.", "Engagement", true),
        likert("C18", "The companion seemed like a believable animal.", "Overall believability"),
        likert("C19", "I could imagine this companion having its own personality.", "Overall believability"),
        likert("C20", "Overall, the companion behaved in a lifelike way.", "Overall believability"),
    };
    Question attention = likert("C21", "To show you are reading carefully, please select Agree for this row.", "");
    attention.attentionCheck = true;
    attention.attentionAnswer = 4;
    rows.push_back(attention);

    // One shuffle across all rows (stable per participant and companion), then paginate.
    std::mt19937 rng(shuffleSeed);
    std::shuffle(rows.begin(), rows.end(), rng);
    const std::size_t pageSizes[4] = {5, 5, 5, 6};
    std::size_t next = 0;
    for (int p = 0; p < 4; ++p) {
        Page page;
        page.title = "Statements " + std::to_string(p + 1) + " of 4";
        if (p == 0)
            page.description = "Think about " + name + ", the companion you just used. Choose how much you agree with each "
                               "statement. There are no right or wrong answers, and some statements are the opposite of others.";
        else
            page.description = "Choose how much you agree with each statement about " + name + ".";
        for (std::size_t i = 0; i < pageSizes[p] && next < rows.size(); ++i) page.questions.push_back(rows[next++]);
        d.pages.push_back(page);
    }

    Page overall;
    overall.title = "Overall";
    overall.questions.push_back(scale("summary_believability", "Overall, how believable was " + name + " as an animal?",
                                      "Not at all believable", "Extremely believable"));
    overall.questions.push_back(freeText("comment_most_lifelike", "What made this companion feel most like a real animal?", true));
    overall.questions.push_back(freeText("comment_broke_illusion", "Was there a moment that made it feel less like a real animal? What happened?", true));
    d.pages.push_back(overall);

    Page technical;
    technical.title = "Technical check";
    technical.questions.push_back(choice("technical_issue", "Did anything not work properly during this session (for example a freeze or a delay)?",
                                         {"No", "Yes"}));
    technical.questions.push_back(freeText("technical_detail", "If yes, what happened?", false));
    d.pages.push_back(technical);
    return d;
}

Definition makeComparison() {
    Definition d;
    d.id = "comparison";
    d.title = "Comparing Companion A and Companion B";
    const std::vector<std::string> pick = {"Companion A", "Companion B", "No noticeable difference"};

    Page first;
    first.title = "Which one?";
    first.description = "You have now used both companions. Answer these questions about the two of them together.";
    first.questions.push_back(choice("cmp_consistent", "Which companion behaved more consistently?", pick));
    first.questions.push_back(choice("cmp_adapted", "Which companion adapted better to how you treated it?", pick));
    d.pages.push_back(first);

    Page second;
    second.title = "Which one?";
    second.questions.push_back(choice("cmp_engaging", "Which companion was more engaging to interact with?", pick));
    second.questions.push_back(choice("cmp_believable", "Which companion was more believable as an animal overall?", pick));
    second.questions.push_back(scale("cmp_confidence", "How confident are you in your answer about believability?", "I was guessing", "Very sure"));
    d.pages.push_back(second);

    Page last;
    last.title = "One last thing";
    last.questions.push_back(choice("cmp_built_differently", "Did you think the two companions were built differently?", {"Yes", "No", "Not sure"}));
    last.questions.push_back(freeText("cmp_difference_detail", "If yes or not sure, what did you think was different?", true));
    last.questions.push_back(freeText("cmp_other_comments", "Anything else you would like to tell us?", true));
    d.pages.push_back(last);
    return d;
}

}  // namespace questionnaire
