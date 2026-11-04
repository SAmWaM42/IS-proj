#include "questionnaire-definition.h"

namespace study {
namespace {

QuestionDefinition likert(const char* id, const char* prompt) {
    QuestionDefinition q;
    q.id = id;
    q.type = AnswerType::Likert;
    q.required = true;
    q.minimum = 1;
    q.maximum = 5;
    q.prompt = prompt;
    return q;
}

QuestionDefinition choice(const char* id, const char* prompt,
                          std::vector<std::string> options, bool required = true) {
    QuestionDefinition q;
    q.id = id;
    q.type = AnswerType::SingleChoice;
    q.required = required;
    q.options = std::move(options);
    q.prompt = prompt;
    return q;
}

QuestionDefinition paragraph(const char* id, const char* prompt, bool required = false) {
    QuestionDefinition q;
    q.id = id;
    q.type = AnswerType::Text;
    q.required = required;
    q.maxLength = 2000;
    q.prompt = prompt;
    return q;
}

} // namespace

QuestionnaireDefinition companionBelievabilityQuestionnaire() {
    QuestionnaireDefinition definition;
    definition.version = "companion-believability-v1";

    // Section 3: identical questionnaire after condition A and condition B.
    // Grid 1 — behavioural consistency, adaptability, preference response,
    // autonomy and variation.
    definition.conditionQuestions = {
        likert("consistency_personality_stability",
               "The companion's personality felt stable throughout the session."),
        likert("consistency_similar_reactions",
               "The companion reacted in similar ways to similar things I did."),
        likert("consistency_random_or_contradictory",
               "The companion's behaviour seemed random or contradictory."),
        likert("adaptability_treatment_response",
               "The companion's behaviour changed in sensible ways depending on how I treated it."),
        likert("memory_earlier_treatment",
               "The companion seemed to remember how I had treated it earlier."),
        likert("memory_independent_response",
               "The companion responded the same way no matter what had happened before."),
        likert("preference_later_behaviour",
               "When I told the companion something about my preferences, its later behaviour seemed to reflect it."),
        likert("autonomy_self_initiated",
               "The companion did things on its own, not only in response to my buttons."),
        likert("variation_not_repetitive",
               "The companion's behaviour was varied and did not repeat the same responses."),
        likert("scripted_machine_like",
               "The companion felt scripted, like a machine running through commands."),

        // Grid 2 — affective expression, contextual appropriateness,
        // engagement, believability, and the attention check.
        likert("affect_observable_mood",
               "I could tell what kind of mood the companion was in from how it behaved."),
        likert("contextual_reaction_fit",
               "The companion's reactions suited the situation."),
        likert("affect_flat_or_emotionless",
               "The companion's reactions seemed flat or emotionless."),
        likert("contextual_unrequested_behaviour",
               "When the companion did not do what I asked, its behaviour still made sense."),
        likert("engagement_wanted_to_continue",
               "I wanted to keep interacting with the companion."),
        likert("engagement_attention",
               "I kept paying attention to what the companion was doing."),
        likert("engagement_boredom",
               "I became bored during the session."),
        likert("believability_animal",
               "The companion seemed like a believable animal."),
        likert("believability_personality",
               "I could imagine this companion having its own personality."),
        likert("believability_lifelike",
               "Overall, the companion behaved in a lifelike way."),
        likert("attention_check_agree",
               "To show you are reading carefully, please select Agree for this row."),

        // Additional per-condition questions.
        likert("believability_overall_rating",
               "Overall, how believable was this companion as an animal?"),
        paragraph("believability_real_animal_explanation",
                  "What made this companion feel most like a real animal?"),
        paragraph("believability_less_real_moment",
                  "Was there a moment that made it feel less like a real animal? What happened?"),
        choice("session_technical_issue",
               "Did anything not work properly during this session (for example a freeze or a delay)?",
               {"No", "Yes"}),
        paragraph("session_technical_issue_details",
                  "If yes, what happened?")
    };

    // Section 4: presented once, after both condition questionnaires.
    definition.comparisonQuestions = {
        choice("comparison_consistency",
               "Which companion behaved more consistently?",
               {"Companion A", "Companion B", "No noticeable difference"}),
        choice("comparison_adaptability",
               "Which companion adapted better to how you treated it?",
               {"Companion A", "Companion B", "No noticeable difference"}),
        choice("comparison_engagement",
               "Which companion was more engaging to interact with?",
               {"Companion A", "Companion B", "No noticeable difference"}),
        choice("comparison_believability",
               "Which companion was more believable as an animal overall?",
               {"Companion A", "Companion B", "No noticeable difference"}),
        likert("comparison_believability_confidence",
               "How confident are you in your answer to the previous question?"),
        choice("comparison_blinding_check",
               "Did you think the two companions were built differently?",
               {"Yes", "No", "Not sure"}),
        paragraph("comparison_perceived_differences",
                  "If yes or not sure, what did you think was different?"),
        paragraph("comparison_additional_comments",
                  "Anything else you would like to tell us?")
    };

    return definition;
}

} // namespace study
