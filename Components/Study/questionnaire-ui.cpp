#include "questionnaire-ui.h"

#include <algorithm>
#include <cstdint>
#include <variant>

namespace {
const sf::Color kPaper(250, 241, 225);
const sf::Color kPanel(255, 250, 240);
const sf::Color kInk(84, 58, 44);
const sf::Color kMuted(150, 118, 96);
const sf::Color kAccent(196, 112, 84);
const sf::Color kAccentSoft(247, 218, 192);
const sf::Color kBorder(228, 204, 174);

void drawText(sf::RenderTarget& target, const sf::Font& font, const std::string& value,
              float x, float y, unsigned int size, sf::Color colour) {
    sf::Text text(font, value, size);
    text.setFillColor(colour);
    text.setPosition({x, y});
    target.draw(text);
}

void drawChoice(sf::RenderTarget& target, const sf::Font& font, const std::string& label,
                sf::FloatRect bounds, bool selected) {
    sf::RectangleShape shape(bounds.size);
    shape.setPosition(bounds.position);
    shape.setFillColor(selected ? kAccentSoft : kPanel);
    shape.setOutlineColor(selected ? kAccent : kBorder);
    shape.setOutlineThickness(selected ? 2.0f : 1.0f);
    target.draw(shape);
    drawText(target, font, label, bounds.position.x + 12.0f,
             bounds.position.y + 13.0f, 17, kInk);
}
}

QuestionnaireUI::QuestionnaireUI(
    const std::vector<study::QuestionDefinition>& questions,
    const study::QuestionnaireResponse& initial)
    : questions_(questions) {
    for (const auto& answer : initial.answers)
        answers_[answer.questionId] = answer.value;
    rebuildVisibleQuestions();
}

void QuestionnaireUI::rebuildVisibleQuestions() {
    visibleQuestions_.clear();
    for (std::size_t i = 0; i < questions_.size(); ++i) {
        const auto& question = questions_[i];
        if (!question.visibleWhenQuestionId.empty()) {
            const auto condition = answers_.find(question.visibleWhenQuestionId);
            if (condition == answers_.end() ||
                !std::holds_alternative<std::string>(condition->second)) continue;
            const auto& selected = std::get<std::string>(condition->second);
            if (std::find(question.visibleWhenValues.begin(),
                          question.visibleWhenValues.end(), selected) ==
                question.visibleWhenValues.end()) continue;
        }
        visibleQuestions_.push_back(i);
    }
    if (visibleQuestions_.empty()) currentIndex_ = 0;
    else currentIndex_ = std::min(currentIndex_, visibleQuestions_.size() - 1);
}

const study::QuestionDefinition* QuestionnaireUI::currentQuestion() const {
    if (currentIndex_ >= visibleQuestions_.size()) return nullptr;
    return &questions_[visibleQuestions_[currentIndex_]];
}

bool QuestionnaireUI::currentAnswerValid() const {
    const auto* question = currentQuestion();
    if (!question) return false;
    const auto found = answers_.find(question->id);
    if (found == answers_.end()) return !question->required;
    if (question->type == study::AnswerType::Text) {
        const auto* value = std::get_if<std::string>(&found->second);
        return value && value->size() <= question->maxLength &&
            (!question->required || value->find_first_not_of(" \t\r\n") != std::string::npos);
    }
    if (question->type == study::AnswerType::Likert) {
        const auto* value = std::get_if<std::int64_t>(&found->second);
        return value && (!question->minimum || *value >= *question->minimum) &&
            (!question->maximum || *value <= *question->maximum);
    }
    if (question->type == study::AnswerType::SingleChoice) {
        const auto* value = std::get_if<std::string>(&found->second);
        return value && std::find(question->options.begin(), question->options.end(), *value)
            != question->options.end();
    }
    return false;
}

bool QuestionnaireUI::allRequiredAnswersValid() const {
    for (const auto index : visibleQuestions_) {
        const auto& question = questions_[index];
        if (!question.required) continue;
        const auto found = answers_.find(question.id);
        if (found == answers_.end()) return false;
        if (question.type == study::AnswerType::Text) {
            const auto* value = std::get_if<std::string>(&found->second);
            if (!value || value->find_first_not_of(" \t\r\n") == std::string::npos ||
                value->size() > question.maxLength) return false;
        } else if (question.type == study::AnswerType::Likert) {
            const auto* value = std::get_if<std::int64_t>(&found->second);
            if (!value || (question.minimum && *value < *question.minimum) ||
                (question.maximum && *value > *question.maximum)) return false;
        } else if (question.type == study::AnswerType::SingleChoice) {
            const auto* value = std::get_if<std::string>(&found->second);
            if (!value || std::find(question.options.begin(), question.options.end(), *value)
                == question.options.end()) return false;
        }
    }
    return true;
}

void QuestionnaireUI::selectOption(const std::string& option) {
    const auto* question = currentQuestion();
    if (!question) return;
    if (question->type == study::AnswerType::Likert) {
        try {
            const auto value = static_cast<std::int64_t>(std::stoll(option));
            answers_[question->id] = value;
        } catch (...) {
            message_ = "That response could not be selected.";
        }
    } else if (question->type == study::AnswerType::SingleChoice) {
        answers_[question->id] = option;
    }
    message_.clear();
}

void QuestionnaireUI::advance() {
    if (!currentAnswerValid()) {
        message_ = "Please answer this question before continuing.";
        return;
    }
    if (currentIndex_ + 1 < visibleQuestions_.size()) {
        ++currentIndex_;
        textBuffer_.clear();
        const auto* question = currentQuestion();
        if (question && question->type == study::AnswerType::Text) {
            const auto found = answers_.find(question->id);
            if (found != answers_.end() && std::holds_alternative<std::string>(found->second))
                textBuffer_ = std::get<std::string>(found->second);
        }
        message_.clear();
    } else {
        finish();
    }
}

void QuestionnaireUI::retreat() {
    if (currentIndex_ > 0) {
        --currentIndex_;
        textBuffer_.clear();
        const auto* question = currentQuestion();
        if (question && question->type == study::AnswerType::Text) {
            const auto found = answers_.find(question->id);
            if (found != answers_.end() && std::holds_alternative<std::string>(found->second))
                textBuffer_ = std::get<std::string>(found->second);
        }
        message_.clear();
    }
}

void QuestionnaireUI::finish() {
    if (!allRequiredAnswersValid()) {
        message_ = "Please complete all required questions before submitting.";
        return;
    }
    completed_ = true;
    message_.clear();
}

void QuestionnaireUI::handleEvent(const sf::Event& event, sf::RenderWindow& window) {
    if (completed_) return;
    if (const auto* click = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (click->button != sf::Mouse::Button::Left) return;
        const auto point = window.mapPixelToCoords(click->position);
        const auto* question = currentQuestion();
        if (!question) return;

        if (point.y >= 745.0f && point.y <= 805.0f) {
            if (point.x >= 80.0f && point.x <= 250.0f) retreat();
            else if (point.x >= 1000.0f && point.x <= 1200.0f) advance();
            return;
        }

        if (question->type == study::AnswerType::Likert) {
            const auto& labels = question->scaleLabels;
            const float gap = 12.0f;
            const float width = std::min(205.0f, (1120.0f - gap * (labels.size() - 1)) /
                                                     static_cast<float>(labels.size()));
            for (std::size_t i = 0; i < labels.size(); ++i) {
                const sf::FloatRect bounds({80.0f + static_cast<float>(i) * (width + gap), 365.0f},
                                           {width, 76.0f});
                if (bounds.contains(point)) {
                    selectOption(std::to_string(i + 1));
                    return;
                }
            }
        } else if (question->type == study::AnswerType::SingleChoice) {
            const auto& options = question->options;
            for (std::size_t i = 0; i < options.size(); ++i) {
                const sf::FloatRect bounds({180.0f, 330.0f + static_cast<float>(i) * 72.0f},
                                           {920.0f, 58.0f});
                if (bounds.contains(point)) {
                    selectOption(options[i]);
                    return;
                }
            }
        }
    } else if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        if (key->code == sf::Keyboard::Key::Escape) return;
        if (key->code == sf::Keyboard::Key::Enter) advance();
        else if (key->code == sf::Keyboard::Key::Backspace && !textBuffer_.empty()) {
            textBuffer_.pop_back();
            if (const auto* question = currentQuestion())
                answers_[question->id] = textBuffer_;
        } else if (key->code == sf::Keyboard::Key::Left) retreat();
    } else if (const auto* text = event.getIf<sf::Event::TextEntered>()) {
        const auto* question = currentQuestion();
        if (question && question->type == study::AnswerType::Text &&
            text->unicode >= 32 && text->unicode < 127 &&
            textBuffer_.size() < question->maxLength) {
            textBuffer_.push_back(static_cast<char>(text->unicode));
            answers_[question->id] = textBuffer_;
            message_.clear();
        }
    }
}

void QuestionnaireUI::draw(sf::RenderTarget& target, const sf::Font& font) const {
    target.clear(kPaper);
    sf::RectangleShape panel({1160.0f, 770.0f});
    panel.setPosition({60.0f, 55.0f});
    panel.setFillColor(kPanel);
    panel.setOutlineColor(kBorder);
    panel.setOutlineThickness(2.0f);
    target.draw(panel);

    drawText(target, font, "COMPANION QUESTIONNAIRE", 95.0f, 82.0f, 18, kMuted);
    const auto* question = currentQuestion();
    if (!question) {
        drawText(target, font, "No questions are available.", 100.0f, 180.0f, 24, kInk);
        return;
    }

    drawText(target, font,
             "Question " + std::to_string(currentIndex_ + 1) + " of " +
                 std::to_string(visibleQuestions_.size()),
             95.0f, 130.0f, 16, kMuted);
    sf::RectangleShape track({1080.0f, 8.0f});
    track.setPosition({95.0f, 165.0f});
    track.setFillColor(kBorder);
    target.draw(track);
    sf::RectangleShape progress({1080.0f * static_cast<float>(currentIndex_ + 1) /
                                     static_cast<float>(std::max<std::size_t>(1, visibleQuestions_.size())),
                                 8.0f});
    progress.setPosition({95.0f, 165.0f});
    progress.setFillColor(kAccent);
    target.draw(progress);

    drawText(target, font, question->prompt, 100.0f, 220.0f, 25, kInk);
    if (question->required)
        drawText(target, font, "Required", 100.0f, 275.0f, 14, kMuted);
    else
        drawText(target, font, "Optional", 100.0f, 275.0f, 14, kMuted);

    if (question->type == study::AnswerType::Likert) {
        for (std::size_t i = 0; i < question->scaleLabels.size(); ++i) {
            const float gap = 12.0f;
            const float width = std::min(205.0f, (1120.0f - gap * (question->scaleLabels.size() - 1)) /
                                                     static_cast<float>(question->scaleLabels.size()));
            const sf::FloatRect bounds({80.0f + static_cast<float>(i) * (width + gap), 365.0f},
                                       {width, 76.0f});
            const auto found = answers_.find(question->id);
            const bool selected = found != answers_.end() &&
                std::holds_alternative<std::int64_t>(found->second) &&
                std::get<std::int64_t>(found->second) == static_cast<std::int64_t>(i + 1);
            drawChoice(target, font, question->scaleLabels[i], bounds, selected);
        }
    } else if (question->type == study::AnswerType::SingleChoice) {
        for (std::size_t i = 0; i < question->options.size(); ++i) {
            const sf::FloatRect bounds({180.0f, 330.0f + static_cast<float>(i) * 72.0f},
                                       {920.0f, 58.0f});
            const auto found = answers_.find(question->id);
            const bool selected = found != answers_.end() &&
                std::holds_alternative<std::string>(found->second) &&
                std::get<std::string>(found->second) == question->options[i];
            drawChoice(target, font, question->options[i], bounds, selected);
        }
    } else if (question->type == study::AnswerType::Text) {
        sf::RectangleShape input({920.0f, 180.0f});
        input.setPosition({180.0f, 330.0f});
        input.setFillColor(sf::Color::White);
        input.setOutlineColor(kAccent);
        input.setOutlineThickness(2.0f);
        target.draw(input);
        drawText(target, font, textBuffer_.empty() ? "Click here and type your response..." : textBuffer_,
                 195.0f, 350.0f, 18, textBuffer_.empty() ? kMuted : kInk);
    } else {
        drawText(target, font, "This question type is not supported by this screen.",
                 180.0f, 350.0f, 18, kMuted);
    }

    drawChoice(target, font, "Back", sf::FloatRect({80.0f, 745.0f}, {170.0f, 54.0f}),
               currentIndex_ > 0);
    drawChoice(target, font, currentIndex_ + 1 == visibleQuestions_.size() ? "Submit" : "Continue",
               sf::FloatRect({1000.0f, 745.0f}, {200.0f, 54.0f}), false);
    if (!message_.empty()) drawText(target, font, message_, 280.0f, 760.0f, 15, kAccent);
}

bool QuestionnaireUI::completed() const noexcept { return completed_; }

study::QuestionnaireResponse QuestionnaireUI::response() const {
    study::QuestionnaireResponse result;
    for (const auto index : visibleQuestions_) {
        const auto& question = questions_[index];
        const auto found = answers_.find(question.id);
        if (found == answers_.end()) continue;
        if (question.type == study::AnswerType::Text &&
            std::get<std::string>(found->second).empty()) continue;
        result.answers.push_back({question.id, question.type, found->second});
    }
    return result;
}

const std::string& QuestionnaireUI::message() const noexcept { return message_; }
