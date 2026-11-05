#pragma once

// In-app questionnaire window (cosy style, matches CompanionUI).
//
// The questionnaire is DATA: a Definition made of Pages made of Questions
// (see questionnaire_content.cpp for the companion-rating and comparison forms).
// The View draws it, collects answers and produces a JSON result.
//
//   questionnaire::View view(window, font);
//   view.begin(questionnaire::makeCompanionRating('A', seed), context, seed);
//   ... each frame:  view.update(dt);  view.draw();
//   ... events:      view.handleClick(point);  view.handleText(unicode);
//                    view.handleKey(key);      view.handleScroll(delta);
//   ... when view.finished():  nlohmann::json result = view.result();

#include <chrono>
#include <string>
#include <vector>

#include <SFML/Graphics.hpp>
#include <json.hpp>

namespace questionnaire {

enum class QuestionType {
    Info,     // read-only paragraph (for example consent text)
    Likert,   // statement answered on a 5-point agreement scale, shown as a grid row
    Choice,   // pick one of several options (stored value is the option text)
    Scale,    // linear 1 to 5 scale with optional end labels
    Text      // free text (single line or paragraph)
};

struct Question {
    std::string id;                    // key in the JSON output, for example "C1"
    QuestionType type = QuestionType::Likert;
    std::string text;                  // prompt shown to the participant
    std::vector<std::string> options;  // Choice: the option labels
    std::string lowLabel;              // Scale: label for 1
    std::string highLabel;             // Scale: label for 5
    bool required = true;
    int maxLength = 300;               // Text: maximum characters
    bool paragraph = false;            // Text: multi-line box
    // Scoring metadata. Never shown to the participant; written into the JSON.
    bool reverseScored = false;
    std::string construct;
    bool attentionCheck = false;       // Likert: a row that tells the participant which answer to pick
    int attentionAnswer = 0;           // expected value (1..5) for an attention check
    std::string visibleWhenQuestionId; // optional conditional parent question
    std::vector<std::string> visibleWhenValues;
};

struct Page {
    std::string title;
    std::string description;
    std::vector<Question> questions;
    bool shuffleQuestions = false;     // randomise the row order within this page (stable per participant)
};

struct Definition {
    std::string id;
    std::string title;
    std::vector<Page> pages;
};

class View {
public:
    View(sf::RenderWindow& window, const sf::Font& font);

    // Starts (or restarts) a questionnaire. 'context' is copied into the result
    // (run id, participant code, session, companion code, ...).
    void begin(const Definition& definition, const nlohmann::json& context, unsigned int shuffleSeed = 1);

    bool active() const { return running; }       // shown and waiting for the participant
    bool finished() const { return done; }        // Submit was pressed with all required answers given
    void close() { running = false; }

    void update(float elapsedSeconds);
    void draw();

    // Input. handleClick returns true when the click was used.
    bool handleClick(sf::Vector2f point);
    void handleText(char32_t unicode);
    void handleKey(sf::Keyboard::Key key);        // Backspace, Enter
    void handleScroll(float delta);               // mouse wheel

    nlohmann::json result() const;                // valid once finished()

private:
    struct Answer {
        int index = -1;                // Likert/Scale: value 1..5; Choice: option index
        std::string text;              // Text answers
        double answeredMs = -1.0;      // first answer, milliseconds since begin()
        int changes = 0;               // how many times the answer was changed
    };
    struct Slot {
        int question = 0;              // index into Page::questions
        float y = 0.0f;                // top, in content space (before scrolling)
        float height = 0.0f;
        std::vector<std::string> lines;
    };

    sf::RenderWindow& window;
    const sf::Font& font;
    Definition definition;
    nlohmann::json context;
    unsigned int seed = 1;
    bool running = false;
    bool done = false;
    int pageIndex = 0;
    int focusedQuestion = -1;          // Text question with keyboard focus (index into the page)
    bool showMissing = false;
    std::string status;
    float scroll = 0.0f;
    float contentBottom = 0.0f;
    float clock = 0.0f;
    std::chrono::steady_clock::time_point startTime;
    std::string startedAt;
    std::string completedAt;
    double completedMs = 0.0;
    std::vector<std::vector<Answer>> answers;       // [page][question]
    std::vector<std::vector<int>> order;            // [page] -> question indices in display order
    std::vector<Slot> slots;                        // layout of the current page
    std::vector<std::string> descriptionLines;
    bool pageHasLikert = false;
    float headerY = 0.0f;

    void layoutPage();
    void goToPage(int index);
    bool pageComplete() const;
    bool isMissing(int pageNumber, int question) const;
    bool isVisible(int pageNumber, int question) const;
    void setAnswer(int question, int index);
    double elapsedMs() const;
    std::vector<std::string> wrap(const std::string& value, unsigned int size, float maxWidth) const;
    float textWidth(const std::string& value, unsigned int size) const;
    void drawText(const std::string& value, float x, float y, unsigned int size, sf::Color colour) const;
    void drawCentred(const std::string& value, float centreX, float y, unsigned int size, sf::Color colour) const;
    void drawSlot(const Slot& slot, int position, float y);
    void drawFooter();
    void next();
    void back();
};

// ---- Content (questionnaire_content.cpp) -------------------------------------
Definition makeConsentAndBackground();
Definition makeCompanionRating(char companionCode, unsigned int shuffleSeed);
Definition makeComparison();

}  // namespace questionnaire
