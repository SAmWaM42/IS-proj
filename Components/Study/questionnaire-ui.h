#pragma once

#include "Components/Study/study-session-data.h"

#include <map>
#include <string>
#include <vector>

#include <SFML/Graphics.hpp>

// Reusable, one-question-at-a-time questionnaire renderer. It owns response
// state, but not the application window, font, or questionnaire definition.
class QuestionnaireUI {
public:
    explicit QuestionnaireUI(const std::vector<study::QuestionDefinition>& questions,
                             const study::QuestionnaireResponse& initial = {});

    void handleEvent(const sf::Event& event, sf::RenderWindow& window);
    void draw(sf::RenderTarget& target, const sf::Font& font) const;

    bool completed() const noexcept;
    study::QuestionnaireResponse response() const;
    const std::string& message() const noexcept;

private:
    const std::vector<study::QuestionDefinition>& questions_;
    std::map<std::string, study::AnswerValue> answers_;
    std::vector<std::size_t> visibleQuestions_;
    std::size_t currentIndex_ = 0;
    std::string textBuffer_;
    std::string message_;
    bool completed_ = false;

    void rebuildVisibleQuestions();
    const study::QuestionDefinition* currentQuestion() const;
    bool currentAnswerValid() const;
    bool allRequiredAnswersValid() const;
    void selectOption(const std::string& option);
    void advance();
    void retreat();
    void finish();
};
