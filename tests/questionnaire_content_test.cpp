#include "questionnaire.h"

#include <cassert>
#include <set>
#include <string>

int main() {
    const auto ratingA = questionnaire::makeCompanionRating('A', 101);
    const auto ratingB = questionnaire::makeCompanionRating('B', 102);
    const auto comparison = questionnaire::makeComparison();
    const auto consent = questionnaire::makeConsentAndBackground();

    assert(ratingA.id == "rating_A");
    assert(ratingB.id == "rating_B");
    assert(ratingA.pages.size() == 6);
    assert(ratingB.pages.size() == 6);
    assert(comparison.pages.size() == 3);
    assert(consent.pages.size() == 1); // Demographic questions remain omitted.

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
    assert(likertCount == 21);
    assert(ratingIds.size() == 26);
    assert(attentionCheckFound);
    assert(reverseScoredFound);

    std::set<std::string> comparisonIds;
    for (const auto& page : comparison.pages) {
        for (const auto& question : page.questions) {
            assert(!question.id.empty());
            assert(comparisonIds.insert(question.id).second);
        }
    }
    assert(comparisonIds.size() == 8);
    return 0;
}
