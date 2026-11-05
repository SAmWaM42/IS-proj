#include "questionnaire.h"
#include "Components/Simulation/ui-layout.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <random>

namespace questionnaire {
namespace {

// Palette and geometry follow CompanionUI (cream paper, cocoa ink, terracotta accent).
const sf::Color kPanel(255, 250, 240);
const sf::Color kInk(84, 58, 44);
const sf::Color kMuted(150, 118, 96);
const sf::Color kAccent(196, 112, 84);
const sf::Color kAccentDark(150, 78, 54);
const sf::Color kAccentSoft(247, 218, 192);
const sf::Color kBorder(228, 204, 174);
const sf::Color kCream(255, 246, 228);
const sf::Color kShadowSoft(88, 56, 40, 46);
const sf::Color kTrack(238, 224, 204);
constexpr float kPi = 3.14159265f;

constexpr float kContentX = 100.0f;      // left edge of the content column
constexpr float kContentW = 1080.0f;
constexpr float kContentTop = 150.0f;
constexpr float kContentBottom = 782.0f;
constexpr float kPanelPad = 24.0f;
constexpr float kStatementW = 560.0f;    // width of the statement column in a Likert row
constexpr float kRadioX0 = 724.0f;       // centre of the first Likert radio
constexpr float kColW = 96.0f;           // spacing between Likert radios
constexpr float kHeaderH = 50.0f;
const sf::FloatRect kBackRect({76.0f, 822.0f}, {150.0f, 48.0f});
const sf::FloatRect kNextRect({1034.0f, 822.0f}, {170.0f, 48.0f});
const sf::FloatRect kContentRect({kContentX - 12.0f, kContentTop}, {kContentW + 24.0f, kContentBottom - kContentTop});

const char* kLikertLabels[5] = {"Strongly\ndisagree", "Disagree", "Neutral", "Agree", "Strongly\nagree"};

// Rounded rectangle built from a convex polygon (same helper as companion-ui.cpp).
void roundedBox(sf::RenderTarget& target, float x, float y, float w, float h, float radius,
                sf::Color fill, sf::Color outline = sf::Color::Transparent, float outlineThickness = 0.0f) {
    radius = std::min(radius, std::min(w, h) * 0.5f);
    const int segments = 6;
    sf::ConvexShape shape;
    shape.setPointCount(static_cast<std::size_t>(4 * (segments + 1)));
    const float cx[4] = {x + w - radius, x + w - radius, x + radius, x + radius};
    const float cy[4] = {y + radius, y + h - radius, y + h - radius, y + radius};
    std::size_t index = 0;
    for (int corner = 0; corner < 4; ++corner) {
        for (int s = 0; s <= segments; ++s) {
            const float degrees = -90.0f + 90.0f * static_cast<float>(corner) +
                                  90.0f * static_cast<float>(s) / static_cast<float>(segments);
            const float angle = degrees * kPi / 180.0f;
            shape.setPoint(index++, {cx[corner] + radius * std::cos(angle), cy[corner] + radius * std::sin(angle)});
        }
    }
    shape.setFillColor(fill);
    shape.setOutlineColor(outline);
    shape.setOutlineThickness(outlineThickness);
    target.draw(shape);
}

void disc(sf::RenderTarget& target, float cx, float cy, float radius, sf::Color fill,
          sf::Color outline = sf::Color::Transparent, float thickness = 0.0f) {
    sf::CircleShape shape(radius);
    shape.setOrigin({radius, radius});
    shape.setPosition({cx, cy});
    shape.setFillColor(fill);
    shape.setOutlineColor(outline);
    shape.setOutlineThickness(thickness);
    target.draw(shape);
}

std::string utcNow() {
    const std::time_t now = std::time(nullptr);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return buffer;
}

void appendUtf8(std::string& out, char32_t code) {
    if (code < 0x80) {
        out.push_back(static_cast<char>(code));
    } else if (code < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (code >> 6)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    } else if (code < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (code >> 12)));
        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (code >> 18)));
        out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    }
}

int utf8Length(const std::string& value) {
    int count = 0;
    for (unsigned char ch : value)
        if ((ch & 0xC0) != 0x80) ++count;
    return count;
}

void popUtf8(std::string& value) {
    if (value.empty()) return;
    std::size_t end = value.size() - 1;
    while (end > 0 && (static_cast<unsigned char>(value[end]) & 0xC0) == 0x80) --end;
    value.erase(end);
}

bool blank(const std::string& value) {
    return std::all_of(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch) != 0; });
}

// ---- Geometry shared by drawing and hit testing (content space, before scrolling) ----
// Slot heights are computed in View::layoutPage from the same constants.
float promptHeight(std::size_t lines, float lineHeight) { return static_cast<float>(lines) * lineHeight + 12.0f; }

}  // namespace

View::View(sf::RenderWindow& windowRef, const sf::Font& fontRef) : window(windowRef), font(fontRef) {}

double View::elapsedMs() const {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - startTime).count();
}

float View::textWidth(const std::string& value, unsigned int size) const {
    sf::Text text(font, value, size);
    return text.getLocalBounds().size.x;
}

std::vector<std::string> View::wrap(const std::string& value, unsigned int size, float maxWidth) const {
    std::vector<std::string> lines;
    if (value.empty()) return lines;
    std::size_t start = 0;
    while (start <= value.size()) {
        std::size_t end = value.find('\n', start);
        if (end == std::string::npos) end = value.size();
        const std::string paragraph = value.substr(start, end - start);
        std::string line, word;
        auto flushWord = [&]() {
            if (word.empty()) return;
            const std::string candidate = line.empty() ? word : line + " " + word;
            if (!line.empty() && textWidth(candidate, size) > maxWidth) {
                lines.push_back(line);
                line = word;
            } else {
                line = candidate;
            }
            word.clear();
        };
        for (char ch : paragraph) {
            if (ch == ' ') flushWord();
            else word.push_back(ch);
        }
        flushWord();
        lines.push_back(line);
        if (end >= value.size()) break;
        start = end + 1;
    }
    return lines;
}

void View::drawText(const std::string& value, float x, float y, unsigned int size, sf::Color colour) const {
    sf::Text text(font, value, size);
    text.setFillColor(colour);
    text.setPosition({x, y});
    window.draw(text);
}

void View::drawCentred(const std::string& value, float centreX, float y, unsigned int size, sf::Color colour) const {
    sf::Text text(font, value, size);
    text.setFillColor(colour);
    const sf::FloatRect bounds = text.getLocalBounds();
    text.setPosition({centreX - bounds.size.x * 0.5f - bounds.position.x, y});
    window.draw(text);
}

void View::begin(const Definition& newDefinition, const nlohmann::json& newContext, unsigned int shuffleSeed) {
    definition = newDefinition;
    context = newContext;
    seed = shuffleSeed;
    answers.clear();
    order.clear();
    for (std::size_t p = 0; p < definition.pages.size(); ++p) {
        const Page& page = definition.pages[p];
        answers.emplace_back(page.questions.size());
        std::vector<int> indices(page.questions.size());
        for (std::size_t i = 0; i < indices.size(); ++i) indices[i] = static_cast<int>(i);
        if (page.shuffleQuestions) {
            std::mt19937 rng(seed + static_cast<unsigned int>(p) * 7919u);
            std::shuffle(indices.begin(), indices.end(), rng);
        }
        order.push_back(indices);
    }
    pageIndex = 0;
    focusedQuestion = -1;
    showMissing = false;
    status.clear();
    scroll = 0.0f;
    clock = 0.0f;
    done = false;
    running = !definition.pages.empty();
    startTime = std::chrono::steady_clock::now();
    startedAt = utcNow();
    completedAt.clear();
    completedMs = 0.0;
    if (running) layoutPage();
}

void View::layoutPage() {
    slots.clear();
    const Page& page = definition.pages[static_cast<std::size_t>(pageIndex)];
    float y = kContentTop + 6.0f;
    descriptionLines = wrap(page.description, 16, kContentW - 20.0f);
    if (!descriptionLines.empty()) y += static_cast<float>(descriptionLines.size()) * 22.0f + 14.0f;
    pageHasLikert = std::any_of(page.questions.begin(), page.questions.end(),
                                [](const Question& q) { return q.type == QuestionType::Likert; });
    if (pageHasLikert) {
        headerY = y;
        y += kHeaderH;
    }
    for (int index : order[static_cast<std::size_t>(pageIndex)]) {
        if (!isVisible(pageIndex, index)) continue;
        const Question& q = page.questions[static_cast<std::size_t>(index)];
        Slot slot;
        slot.question = index;
        slot.y = y;
        switch (q.type) {
            case QuestionType::Likert:
                slot.lines = wrap(q.text, 17, kStatementW);
                slot.height = std::max(64.0f, static_cast<float>(slot.lines.size()) * 22.0f + 24.0f);
                break;
            case QuestionType::Choice:
                slot.lines = wrap(q.text, 18, kContentW - 40.0f);
                slot.height = promptHeight(slot.lines.size(), 24.0f) + static_cast<float>(q.options.size()) * 52.0f + 8.0f;
                break;
            case QuestionType::Scale:
                slot.lines = wrap(q.text, 18, kContentW - 40.0f);
                slot.height = promptHeight(slot.lines.size(), 24.0f) + 100.0f;
                break;
            case QuestionType::Text:
                slot.lines = wrap(q.text, 18, kContentW - 40.0f);
                slot.height = promptHeight(slot.lines.size(), 24.0f) + (q.paragraph ? 112.0f : 50.0f) + 8.0f;
                break;
        }
        y += slot.height + (q.type == QuestionType::Likert ? 4.0f : 14.0f);
        slots.push_back(slot);
    }
    contentBottom = y + 10.0f;
    scroll = 0.0f;
}

void View::goToPage(int index) {
    pageIndex = index;
    focusedQuestion = -1;
    showMissing = false;
    layoutPage();
}

bool View::isVisible(int pageNumber, int question) const {
    const Question& q = definition.pages[static_cast<std::size_t>(pageNumber)]
                            .questions[static_cast<std::size_t>(question)];
    if (q.visibleWhenQuestionId.empty()) return true;

    for (std::size_t p = 0; p < definition.pages.size(); ++p) {
        const Page& parentPage = definition.pages[p];
        for (std::size_t i = 0; i < parentPage.questions.size(); ++i) {
            if (parentPage.questions[i].id != q.visibleWhenQuestionId) continue;
            const Answer& parentAnswer = answers[p][i];
            const Question& parentQuestion = parentPage.questions[i];
            if (parentQuestion.type != QuestionType::Choice || parentAnswer.index < 0 ||
                parentAnswer.index >= static_cast<int>(parentQuestion.options.size())) return false;
            const std::string& selected =
                parentQuestion.options[static_cast<std::size_t>(parentAnswer.index)];
            return std::find(q.visibleWhenValues.begin(), q.visibleWhenValues.end(), selected) !=
                   q.visibleWhenValues.end();
        }
    }
    return false;
}

bool View::isMissing(int pageNumber, int question) const {
    if (!isVisible(pageNumber, question)) return false;
    const Question& q = definition.pages[static_cast<std::size_t>(pageNumber)].questions[static_cast<std::size_t>(question)];
    if (!q.required) return false;
    const Answer& a = answers[static_cast<std::size_t>(pageNumber)][static_cast<std::size_t>(question)];
    if (q.type == QuestionType::Text) return blank(a.text);
    return a.index < 0;
}

bool View::pageComplete() const {
    const Page& page = definition.pages[static_cast<std::size_t>(pageIndex)];
    for (std::size_t i = 0; i < page.questions.size(); ++i)
        if (isVisible(pageIndex, static_cast<int>(i)) && isMissing(pageIndex, static_cast<int>(i))) return false;
    return true;
}

void View::setAnswer(int question, int index) {
    Answer& a = answers[static_cast<std::size_t>(pageIndex)][static_cast<std::size_t>(question)];
    if (a.index != index) {
        if (a.index >= 0) ++a.changes;
        a.index = index;
        if (a.answeredMs < 0.0) a.answeredMs = elapsedMs();
    }
    status.clear();
    const Page& page = definition.pages[static_cast<std::size_t>(pageIndex)];
    const std::string& answeredId = page.questions[static_cast<std::size_t>(question)].id;
    const bool affectsVisibility = std::any_of(
        page.questions.begin(), page.questions.end(),
        [&](const Question& candidate) {
            return candidate.visibleWhenQuestionId == answeredId;
        });
    if (affectsVisibility) layoutPage();
}

void View::next() {
    if (!pageComplete()) {
        showMissing = true;
        status = "Please answer the highlighted questions.";
        // Scroll the first unanswered question into view.
        for (const Slot& slot : slots) {
            if (isMissing(pageIndex, slot.question)) {
                scroll = std::max(0.0f, slot.y - kContentTop - 12.0f);
                break;
            }
        }
        const float maxScroll = std::max(0.0f, contentBottom - kContentBottom);
        scroll = std::min(scroll, maxScroll);
        return;
    }
    if (pageIndex + 1 >= static_cast<int>(definition.pages.size())) {
        done = true;
        running = false;
        completedMs = elapsedMs();
        completedAt = utcNow();
        return;
    }
    goToPage(pageIndex + 1);
}

void View::back() {
    if (pageIndex > 0) goToPage(pageIndex - 1);
}

void View::update(float elapsedSeconds) {
    clock += elapsedSeconds;
}

bool View::handleClick(sf::Vector2f point) {
    if (!running) return false;
    if (kBackRect.contains(point)) { back(); return true; }
    if (kNextRect.contains(point)) { next(); return true; }
    if (!kContentRect.contains(point)) {
        focusedQuestion = -1;
        return true;
    }
    const sf::Vector2f c(point.x, point.y + scroll);   // content space
    const Page& page = definition.pages[static_cast<std::size_t>(pageIndex)];
    for (const Slot& slot : slots) {
        const Question& q = page.questions[static_cast<std::size_t>(slot.question)];
        const float promptH = promptHeight(slot.lines.size(), 24.0f);
        switch (q.type) {
            case QuestionType::Likert:
                if (c.y >= slot.y && c.y < slot.y + slot.height) {
                    for (int k = 0; k < 5; ++k) {
                        const float cx = kRadioX0 + static_cast<float>(k) * kColW;
                        if (std::fabs(c.x - cx) <= kColW * 0.5f) {
                            setAnswer(slot.question, k + 1);
                            focusedQuestion = -1;
                            return true;
                        }
                    }
                }
                break;
            case QuestionType::Choice:
                for (std::size_t k = 0; k < q.options.size(); ++k) {
                    const sf::FloatRect r({kContentX + 10.0f, slot.y + promptH + static_cast<float>(k) * 52.0f},
                                          {std::min(560.0f, kContentW - 20.0f), 44.0f});
                    if (r.contains(c)) {
                        setAnswer(slot.question, static_cast<int>(k));
                        focusedQuestion = -1;
                        return true;
                    }
                }
                break;
            case QuestionType::Scale:
                for (int k = 0; k < 5; ++k) {
                    const float cx = kContentX + 100.0f + static_cast<float>(k) * 100.0f;
                    const float cy = slot.y + promptH + 32.0f;
                    if (std::hypot(c.x - cx, c.y - cy) <= 30.0f) {
                        setAnswer(slot.question, k + 1);
                        focusedQuestion = -1;
                        return true;
                    }
                }
                break;
            case QuestionType::Text: {
                const sf::FloatRect box({kContentX + 10.0f, slot.y + promptH}, {kContentW - 20.0f, q.paragraph ? 112.0f : 46.0f});
                if (box.contains(c)) {
                    focusedQuestion = slot.question;
                    return true;
                }
                break;
            }
            default:
                break;
        }
    }
    focusedQuestion = -1;
    return true;
}

void View::handleText(char32_t unicode) {
    if (!running || focusedQuestion < 0) return;
    const Question& q = definition.pages[static_cast<std::size_t>(pageIndex)].questions[static_cast<std::size_t>(focusedQuestion)];
    if (q.type != QuestionType::Text) return;
    if (unicode < 32 || unicode == 127) return;   // control characters (Enter and Backspace go through handleKey)
    Answer& a = answers[static_cast<std::size_t>(pageIndex)][static_cast<std::size_t>(focusedQuestion)];
    if (utf8Length(a.text) >= q.maxLength) return;
    appendUtf8(a.text, unicode);
    a.index = 0;
    if (a.answeredMs < 0.0) a.answeredMs = elapsedMs();
    status.clear();
}

void View::handleKey(sf::Keyboard::Key key) {
    if (!running) return;
    if (focusedQuestion >= 0) {
        const Question& q = definition.pages[static_cast<std::size_t>(pageIndex)].questions[static_cast<std::size_t>(focusedQuestion)];
        Answer& a = answers[static_cast<std::size_t>(pageIndex)][static_cast<std::size_t>(focusedQuestion)];
        if (key == sf::Keyboard::Key::Backspace) {
            popUtf8(a.text);
            if (a.text.empty()) a.index = -1;
        } else if (key == sf::Keyboard::Key::Enter) {
            if (q.paragraph && utf8Length(a.text) < q.maxLength) {
                a.text.push_back('\n');
                a.index = 0;
            } else if (!q.paragraph) {
                focusedQuestion = -1;
            }
        }
        return;
    }
    if (key == sf::Keyboard::Key::Enter) next();
}

void View::handleScroll(float delta) {
    if (!running) return;
    const float maxScroll = std::max(0.0f, contentBottom - kContentBottom);
    scroll = std::clamp(scroll - delta * 48.0f, 0.0f, maxScroll);
}

void View::drawSlot(const Slot& slot, int position, float y) {
    const Page& page = definition.pages[static_cast<std::size_t>(pageIndex)];
    const Question& q = page.questions[static_cast<std::size_t>(slot.question)];
    const Answer& a = answers[static_cast<std::size_t>(pageIndex)][static_cast<std::size_t>(slot.question)];
    const sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    const bool missing = showMissing && isMissing(pageIndex, slot.question);
    const float promptH = promptHeight(slot.lines.size(), 24.0f);

    auto prompt = [&]() {
        for (std::size_t i = 0; i < slot.lines.size(); ++i)
            drawText(slot.lines[i], kContentX + 10.0f, y + static_cast<float>(i) * 24.0f, 18, kInk);
        if (missing) drawText("Please answer", kContentX + kContentW - 130.0f, y, 14, kAccentDark);
    };

    switch (q.type) {
        case QuestionType::Likert: {
            const float rowH = slot.height - 4.0f;
            roundedBox(window, kContentX, y, kContentW, rowH, 12, position % 2 == 0 ? kCream : kPanel,
                       missing ? kAccent : sf::Color::Transparent, missing ? 2.0f : 0.0f);
            const float textY = y + (rowH - static_cast<float>(slot.lines.size()) * 22.0f) * 0.5f - 1.0f;
            for (std::size_t i = 0; i < slot.lines.size(); ++i)
                drawText(slot.lines[i], kContentX + 22.0f, textY + static_cast<float>(i) * 22.0f, 17, kInk);
            for (int k = 0; k < 5; ++k) {
                const float cx = kRadioX0 + static_cast<float>(k) * kColW;
                const float cy = y + rowH * 0.5f;
                const bool selected = a.index == k + 1;
                const bool hover = std::hypot(mouse.x - cx, mouse.y - cy) <= 22.0f;
                disc(window, cx, cy, 13.0f, selected ? kAccent : (hover ? kAccentSoft : kPanel),
                     selected ? kAccentDark : kBorder, 2.0f);
                if (selected) disc(window, cx, cy, 5.0f, sf::Color::White);
            }
            break;
        }
        case QuestionType::Choice: {
            prompt();
            for (std::size_t k = 0; k < q.options.size(); ++k) {
                const float oy = y + promptH + static_cast<float>(k) * 52.0f;
                const float ow = std::min(560.0f, kContentW - 20.0f);
                const bool selected = a.index == static_cast<int>(k);
                const bool hover = mouse.x >= kContentX + 10.0f && mouse.x < kContentX + 10.0f + ow && mouse.y >= oy && mouse.y < oy + 44.0f;
                roundedBox(window, kContentX + 10.0f, oy + 2.0f, ow, 44.0f, 12.0f, kShadowSoft);
                roundedBox(window, kContentX + 10.0f, oy, ow, 44.0f, 12.0f,
                           selected ? kAccentSoft : (hover ? sf::Color(255, 240, 214) : kPanel),
                           selected ? kAccent : (missing ? kAccent : kBorder), selected ? 2.0f : 1.5f);
                disc(window, kContentX + 36.0f, oy + 22.0f, 10.0f, selected ? kAccent : kPanel, selected ? kAccentDark : kBorder, 2.0f);
                if (selected) disc(window, kContentX + 36.0f, oy + 22.0f, 4.0f, sf::Color::White);
                drawText(q.options[k], kContentX + 58.0f, oy + 11.0f, 17, selected ? kAccentDark : kInk);
            }
            break;
        }
        case QuestionType::Scale: {
            prompt();
            for (int k = 0; k < 5; ++k) {
                const float cx = kContentX + 100.0f + static_cast<float>(k) * 100.0f;
                const float cy = y + promptH + 32.0f;
                const bool selected = a.index == k + 1;
                const bool hover = std::hypot(mouse.x - cx, mouse.y - cy) <= 28.0f;
                disc(window, cx, cy + 2.0f, 26.0f, kShadowSoft);
                disc(window, cx, cy, 26.0f, selected ? kAccent : (hover ? kAccentSoft : kPanel),
                     selected ? kAccentDark : (missing ? kAccent : kBorder), 2.0f);
                drawCentred(std::to_string(k + 1), cx, cy - 12.0f, 20, selected ? sf::Color::White : kInk);
            }
            if (!q.lowLabel.empty()) drawText(q.lowLabel, kContentX + 74.0f, y + promptH + 70.0f, 13, kMuted);
            if (!q.highLabel.empty()) {
                const float w = textWidth(q.highLabel, 13);
                drawText(q.highLabel, kContentX + 526.0f - w, y + promptH + 70.0f, 13, kMuted);
            }
            break;
        }
        case QuestionType::Text: {
            prompt();
            const float bx = kContentX + 10.0f, by = y + promptH, bw = kContentW - 20.0f;
            const float bh = q.paragraph ? 112.0f : 46.0f;
            const bool focused = focusedQuestion == slot.question;
            roundedBox(window, bx, by, bw, bh, 12.0f, kPanel, focused ? kAccent : (missing ? kAccent : kBorder), focused ? 2.5f : 1.5f);
            if (a.text.empty() && !focused) {
                drawText(q.required ? "Click here to type your answer" : "Click here to type (optional)", bx + 14.0f, by + 12.0f, 16, sf::Color(190, 168, 148));
            } else {
                std::string shown = a.text;
                if (focused && std::fmod(clock, 1.0f) < 0.55f) shown += "|";
                const std::vector<std::string> lines = wrap(shown, 17, bw - 28.0f);
                const std::size_t maxLines = q.paragraph ? 4 : 1;
                const std::size_t first = lines.size() > maxLines ? lines.size() - maxLines : 0;   // show the newest lines
                for (std::size_t i = first; i < lines.size(); ++i)
                    drawText(lines[i], bx + 14.0f, by + 10.0f + static_cast<float>(i - first) * 24.0f, 17, kInk);
            }
            break;
        }
    }
}

void View::draw() {
    if (!running) return;
    const Page& page = definition.pages[static_cast<std::size_t>(pageIndex)];
    const int pageCount = static_cast<int>(definition.pages.size());

    // All questionnaire geometry is authored in the shared 1280x900 design
    // space. Preserve the app's aspect-ratio viewport when drawing the panel.
    const sf::View baseView = window.getView();

    // Title, page title and progress
    drawText(definition.title, kContentX - kPanelPad, 26.0f, 28, kInk);
    drawText(page.title, kContentX - kPanelPad, 68.0f, 18, kMuted);
    const std::string progress = "Page " + std::to_string(pageIndex + 1) + " of " + std::to_string(pageCount);
    drawText(progress, kContentX + kContentW + kPanelPad - textWidth(progress, 14), 72.0f, 14, kMuted);
    const float barX = kContentX - kPanelPad, barW = kContentW + 2.0f * kPanelPad;
    roundedBox(window, barX, 104.0f, barW, 8.0f, 4.0f, kTrack);
    roundedBox(window, barX, 104.0f, barW * static_cast<float>(pageIndex + 1) / static_cast<float>(pageCount), 8.0f, 4.0f, kAccent);

    // Panel
    roundedBox(window, barX, 134.0f, barW, 668.0f, 22.0f, kShadowSoft);
    roundedBox(window, barX, 128.0f, barW, 668.0f, 22.0f, kPanel, kBorder, 2.0f);

    // Content, clipped to the panel; the view maps world coordinates 1:1 to pixels.
    sf::View clip(kContentRect);
    clip.setViewport(ui_layout::designRectViewport(window, kContentRect));
    window.setView(clip);
    float y = kContentTop + 6.0f - scroll;
    for (std::size_t i = 0; i < descriptionLines.size(); ++i)
        drawText(descriptionLines[i], kContentX + 10.0f, y + static_cast<float>(i) * 22.0f, 16, kMuted);
    if (pageHasLikert) {
        const float hy = headerY - scroll;
        for (int k = 0; k < 5; ++k) {
            const std::string label = kLikertLabels[k];
            const std::size_t newline = label.find('\n');
            const float cx = kRadioX0 + static_cast<float>(k) * kColW;
            if (newline == std::string::npos) {
                drawCentred(label, cx, hy + 12.0f, 13, kMuted);
            } else {
                drawCentred(label.substr(0, newline), cx, hy + 4.0f, 13, kMuted);
                drawCentred(label.substr(newline + 1), cx, hy + 21.0f, 13, kMuted);
            }
        }
    }
    int position = 0;
    for (const Slot& slot : slots) {
        const float sy = slot.y - scroll;
        if (sy + slot.height >= kContentTop - 10.0f && sy <= kContentBottom + 10.0f) drawSlot(slot, position, sy);
        ++position;
    }
    window.setView(baseView);

    // Scroll indicator
    const float maxScroll = std::max(0.0f, contentBottom - kContentBottom);
    if (maxScroll > 1.0f) {
        const float trackH = kContentBottom - kContentTop - 20.0f;
        const float thumbH = std::max(40.0f, trackH * (kContentBottom - kContentTop) / (contentBottom - kContentTop));
        const float thumbY = kContentTop + 10.0f + (trackH - thumbH) * (scroll / maxScroll);
        roundedBox(window, kContentX + kContentW + 12.0f, kContentTop + 10.0f, 6.0f, trackH, 3.0f, kTrack);
        roundedBox(window, kContentX + kContentW + 12.0f, thumbY, 6.0f, thumbH, 3.0f, kAccent);
    }
    drawFooter();
}

void View::drawFooter() {
    const sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    const bool canGoBack = pageIndex > 0;
    const bool last = pageIndex + 1 >= static_cast<int>(definition.pages.size());

    const bool backHover = canGoBack && kBackRect.contains(mouse);
    roundedBox(window, kBackRect.position.x, kBackRect.position.y + 3.0f, kBackRect.size.x, kBackRect.size.y, 14.0f, kShadowSoft);
    roundedBox(window, kBackRect.position.x, kBackRect.position.y, kBackRect.size.x, kBackRect.size.y, 14.0f,
               !canGoBack ? sf::Color(243, 233, 218) : (backHover ? sf::Color(255, 240, 214) : kPanel),
               !canGoBack ? sf::Color(232, 218, 198) : kBorder, 1.5f);
    drawCentred("Back", kBackRect.position.x + kBackRect.size.x * 0.5f, kBackRect.position.y + 12.0f, 18,
                canGoBack ? kInk : sf::Color(190, 168, 148));

    const bool nextHover = kNextRect.contains(mouse);
    roundedBox(window, kNextRect.position.x, kNextRect.position.y + 3.0f, kNextRect.size.x, kNextRect.size.y, 14.0f, kShadowSoft);
    roundedBox(window, kNextRect.position.x, kNextRect.position.y, kNextRect.size.x, kNextRect.size.y, 14.0f,
               nextHover ? kAccentDark : kAccent, kAccentDark, 2.0f);
    drawCentred(last ? "Submit" : "Next", kNextRect.position.x + kNextRect.size.x * 0.5f, kNextRect.position.y + 12.0f, 18, sf::Color::White);

    if (!status.empty()) drawCentred(status, 640.0f, 836.0f, 16, kAccentDark);
}

nlohmann::json View::result() const {
    nlohmann::json out;
    out["schema_version"] = 1;
    out["questionnaire_id"] = definition.id;
    out["title"] = definition.title;
    out["context"] = context;
    out["started_at"] = startedAt;
    out["completed_at"] = completedAt;
    out["completion_status"] = done ? "completed" : "in_progress";
    out["current_page"] = pageIndex + 1;
    out["page_count"] = definition.pages.size();
    out["last_saved_at"] = utcNow();
    out["duration_seconds"] = (done ? completedMs : elapsedMs()) / 1000.0;
    out["shuffle_seed"] = seed;
    nlohmann::json responses = nlohmann::json::array();
    bool attentionPassed = true;
    bool anyAttention = false;
    for (std::size_t p = 0; p < definition.pages.size(); ++p) {
        const Page& page = definition.pages[p];
        for (std::size_t i = 0; i < page.questions.size(); ++i) {
            const Question& q = page.questions[i];
            if (!isVisible(static_cast<int>(p), static_cast<int>(i))) continue;
            const Answer& a = answers[p][i];
            nlohmann::json row;
            row["id"] = q.id;
            row["page"] = static_cast<int>(p) + 1;
            row["display_position"] = static_cast<int>(std::find(order[p].begin(), order[p].end(), static_cast<int>(i)) - order[p].begin()) + 1;
            switch (q.type) {
                case QuestionType::Likert: row["type"] = "likert"; row["value"] = a.index >= 0 ? nlohmann::json(a.index) : nlohmann::json(); break;
                case QuestionType::Scale: row["type"] = "scale"; row["value"] = a.index >= 0 ? nlohmann::json(a.index) : nlohmann::json(); break;
                case QuestionType::Choice:
                    row["type"] = "choice";
                    row["index"] = a.index;
                    row["value"] = (a.index >= 0 && a.index < static_cast<int>(q.options.size())) ? nlohmann::json(q.options[static_cast<std::size_t>(a.index)]) : nlohmann::json();
                    break;
                case QuestionType::Text: row["type"] = "text"; row["value"] = a.text; break;
                default: break;
            }
            if (!q.construct.empty()) row["construct"] = q.construct;
            if (q.reverseScored) row["reverse_scored"] = true;
            if (q.attentionCheck) {
                anyAttention = true;
                const bool passed = a.index == q.attentionAnswer;
                row["attention_check"] = true;
                row["expected"] = q.attentionAnswer;
                row["passed"] = passed;
                attentionPassed = attentionPassed && passed;
            }
            row["answered_at_ms"] = a.answeredMs;
            row["changes"] = a.changes;
            responses.push_back(row);
        }
    }
    out["responses"] = responses;
    if (anyAttention) out["attention_checks_passed"] = attentionPassed;
    return out;
}

}  // namespace questionnaire
