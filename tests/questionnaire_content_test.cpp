#include "questionnaire.h"
#include "Components/Simulation/ui-layout.h"

#include <cassert>
#include <set>
#include <string>

int main() {
    const sf::FloatRect wideViewport = ui_layout::makeLetterboxedView({1920u, 1080u}).getViewport();
    assert(wideViewport.size.x < 1.0f);
    assert(wideViewport.size.y == 1.0f);
    assert(wideViewport.position.x > 0.0f);
    const sf::FloatRect portraitViewport = ui_layout::makeLetterboxedView({900u, 1280u}).getViewport();
    assert(portraitViewport.size.x == 1.0f);
    assert(portraitViewport.size.y < 1.0f);
    assert(portraitViewport.position.y > 0.0f);

    const auto ratingA = questionnaire::makeCompanionRating('A', 101);
    const auto ratingB = questionnaire::makeCompanionRating('B', 102);
    const auto comparison = questionnaire::makeComparison();

    assert(ratingA.id == "rating_A");
    assert(ratingB.id == "rating_B");
    assert(ratingA.pages.size() == 3);
    assert(ratingB.pages.size() == 3);
    assert(comparison.pages.size() == 2);

    std::set<std::string> ratingIds;
    int likertCount = 0;
    bool attentionCheckFound = false;
    bool reverseScoredFound = false;
    for (const auto& page : ratingA.pages) {
        for (const auto& question : page.questions) {
            assert(!question.id.empty());
            assert(ratingIds.insert(question.id).second);
            if (question.type == questionnaire::QuestionType::Likert) ++likertCount;
            if (question.attentionCheck) {
                attentionCheckFound = true;
                assert(question.attentionAnswer == 4);
            }
            if (question.reverseScored) reverseScoredFound = true;
        }
    }
    assert(likertCount == 8);
    assert(ratingIds.size() == 10);
    std::size_t ratingBCount = 0;
    for (const auto& page : ratingB.pages) ratingBCount += page.questions.size();
    assert(ratingBCount == 10);
    assert(attentionCheckFound);
    assert(reverseScoredFound);

    assert(ratingA.pages.back().questions.size() == 2);
    assert(ratingIds.count("session_feedback") == 1);
    for (const auto& page : ratingA.pages) assert(page.questions.size() <= 5);

    std::set<std::string> comparisonIds;
    for (const auto& page : comparison.pages) {
        for (const auto& question : page.questions) {
            assert(!question.id.empty());
            assert(comparisonIds.insert(question.id).second);
        }
    }
    assert(comparisonIds.size() == 7);
    const auto& comparisonFollowUp = comparison.pages.back().questions[1];
    assert(comparisonFollowUp.visibleWhenQuestionId == "cmp_built_differently");
    assert(comparisonFollowUp.visibleWhenValues.size() == 2);
    for (const auto& page : comparison.pages) assert(page.questions.size() <= 4);
    return 0;
}
