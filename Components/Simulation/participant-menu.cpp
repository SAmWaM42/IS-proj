#include "participant-menu.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace {
const sf::Color kBackground(250, 241, 225);
const sf::Color kPanel(255, 250, 240);
const sf::Color kInk(84, 58, 44);
const sf::Color kMuted(150, 118, 96);
const sf::Color kAccent(196, 112, 84);
const sf::Color kAccentDark(150, 78, 54);
const sf::Color kAccentSoft(247, 218, 192);
const sf::Color kBorder(228, 204, 174);
const sf::Color kGreen(118, 150, 92);
const sf::Color kShadowSoft(88, 56, 40, 46);

sf::FloatRect modeCardRect(int index) {
    return sf::FloatRect({index == 0 ? 110.0f : 670.0f, 175.0f}, {500.0f, 520.0f});
}

const sf::FloatRect kQuitRect({570.0f, 770.0f}, {140.0f, 44.0f});

void drawRoundedBox(sf::RenderTarget& target, float x, float y, float w, float h,
                    float radius, sf::Color fill,
                    sf::Color outline = sf::Color::Transparent,
                    float outlineThickness = 0.0f) {
    radius = std::min(radius, std::min(w, h) * 0.5f);
    constexpr int segments = 6;
    sf::ConvexShape shape;
    shape.setPointCount(static_cast<std::size_t>(4 * (segments + 1)));
    const float cx[4] = {x + w - radius, x + w - radius, x + radius, x + radius};
    const float cy[4] = {y + radius, y + h - radius, y + h - radius, y + radius};
    std::size_t index = 0;
    for (int corner = 0; corner < 4; ++corner) {
        for (int s = 0; s <= segments; ++s) {
            const float degrees = -90.0f + 90.0f * static_cast<float>(corner) +
                                  90.0f * static_cast<float>(s) / static_cast<float>(segments);
            const float radians = degrees * 3.14159265f / 180.0f;
            shape.setPoint(index++, {cx[corner] + radius * std::cos(radians),
                                     cy[corner] + radius * std::sin(radians)});
        }
    }
    shape.setFillColor(fill);
    shape.setOutlineColor(outline);
    shape.setOutlineThickness(outlineThickness);
    target.draw(shape);
}

void drawCircle(sf::RenderTarget& target, float x, float y, float radius,
                sf::Color colour, float scaleY = 1.0f) {
    sf::CircleShape shape(radius);
    shape.setOrigin({radius, radius});
    shape.setScale({1.0f, scaleY});
    shape.setPosition({x, y});
    shape.setFillColor(colour);
    target.draw(shape);
}
}

ParticipantMenu::ParticipantMenu()
    : window(sf::VideoMode(sf::Vector2u(1280u, 900u)), "Animal Agent | Companion Simulation") {
    window.setFramerateLimit(60);

    if (!font.openFromFile("Data/Fonts/UIFont.ttf") &&
        !font.openFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf") &&
        !font.openFromFile("C:/Windows/Fonts/arial.ttf")) {
        throw std::runtime_error("Menu font unavailable. Package Data/Fonts/UIFont.ttf with the application.");
    }
}

ParticipantMenu::Choice ParticipantMenu::run() {
    while (window.isOpen()) {
        while (const auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
                return Choice::Exit;
            }

            if (const auto* click = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (click->button != sf::Mouse::Button::Left) continue;
                const sf::Vector2f point = window.mapPixelToCoords(click->position);

                if (modeCardRect(0).contains(point)) {
                    window.close();
                    return Choice::GeneralInteraction;
                }
                if (modeCardRect(1).contains(point)) {
                    window.close();
                    return Choice::BelievabilityStudy;
                }
                if (kQuitRect.contains(point)) {
                    window.close();
                    return Choice::Exit;
                }
            }
        }

        draw();
    }

    return Choice::Exit;
}

void ParticipantMenu::draw() {
    window.clear(kBackground);

    auto centred = [&](const std::string& value, float centreX, float y,
                       unsigned int size, sf::Color colour) {
        sf::Text label(font, value, size);
        label.setFillColor(colour);
        const sf::FloatRect bounds = label.getLocalBounds();
        label.setPosition({centreX - bounds.size.x * 0.5f - bounds.position.x,
                           y - bounds.position.y});
        window.draw(label);
    };

    auto drawModeIllustration = [&](int index, float x, float y, float w, float h) {
        auto rect = [&](float rx, float ry, float rw, float rh, sf::Color colour) {
            sf::RectangleShape shape({rw, rh});
            shape.setPosition({rx, ry});
            shape.setFillColor(colour);
            window.draw(shape);
        };

        if (index == 0) {
            // Free Play: a sunny yard with a tree, flowers, a ball, and a simple companion silhouette.
            rect(x, y, w, h * 0.62f, sf::Color(158, 205, 232));
            drawCircle(window, x + w - 62.0f, y + 44.0f, 36.0f, sf::Color(255, 236, 190, 150));
            drawCircle(window, x + w - 62.0f, y + 44.0f, 22.0f, sf::Color(255, 218, 140));
            drawCircle(window, x + 96.0f, y + 40.0f, 17.0f, sf::Color(255, 255, 255, 230));
            drawCircle(window, x + 116.0f, y + 34.0f, 22.0f, sf::Color(255, 255, 255, 230));
            drawCircle(window, x + 140.0f, y + 41.0f, 16.0f, sf::Color(255, 255, 255, 230));
            rect(x, y + h * 0.62f, w, h * 0.38f, sf::Color(141, 156, 80));
            drawCircle(window, x + w * 0.30f, y + h * 0.62f, w * 0.26f, sf::Color(150, 166, 88), 0.22f);
            drawCircle(window, x + w * 0.76f, y + h * 0.62f, w * 0.22f, sf::Color(131, 146, 76), 0.20f);

            rect(x + 50.0f, y + h * 0.40f, 12.0f, 52.0f, sf::Color(121, 82, 64));
            drawCircle(window, x + 56.0f, y + h * 0.35f, 28.0f, kGreen);
            drawCircle(window, x + 40.0f, y + h * 0.45f, 20.0f, sf::Color(104, 138, 84));
            drawCircle(window, x + 74.0f, y + h * 0.45f, 20.0f, sf::Color(104, 138, 84));

            for (int i = 0; i < 7; ++i) {
                const float fx = x + 150.0f + static_cast<float>(i) * 44.0f;
                const float fy = y + h * (0.74f + 0.07f * static_cast<float>((i * 5) % 3));
                drawCircle(window, fx, fy, 3.0f,
                           i % 2 == 0 ? sf::Color(255, 244, 214) : sf::Color(240, 176, 176));
            }

            drawCircle(window, x + w * 0.70f, y + h * 0.86f, 12.0f, sf::Color(204, 62, 56));
            drawCircle(window, x + w * 0.70f - 4.0f, y + h * 0.86f - 4.0f, 3.0f,
                       sf::Color(255, 220, 215));

            // A small friendly dog-like silhouette, drawn from simple shapes.
            drawCircle(window, x + w * 0.46f, y + h * 0.90f, 25.0f, sf::Color(255, 250, 240));
            drawCircle(window, x + w * 0.46f + 21.0f, y + h * 0.90f - 20.0f, 16.0f,
                       sf::Color(255, 250, 240));
            drawCircle(window, x + w * 0.46f + 10.0f, y + h * 0.90f - 32.0f, 7.0f,
                       sf::Color(121, 82, 64));
            drawCircle(window, x + w * 0.46f + 27.0f, y + h * 0.90f - 23.0f, 2.5f, kInk);
        } else {
            // Research Session: clipboard and completed steps.
            rect(x, y, w, h, sf::Color(250, 236, 212));
            const float midX = x + w * 0.5f;
            drawRoundedBox(window, midX - 100.0f, y + 14.0f, 200.0f, h - 28.0f, 12.0f,
                           sf::Color(196, 150, 112));
            drawRoundedBox(window, midX - 88.0f, y + 30.0f, 176.0f, h - 52.0f, 8.0f, kPanel);
            drawRoundedBox(window, midX - 26.0f, y + 6.0f, 52.0f, 20.0f, 7.0f,
                           sf::Color(150, 106, 74));

            for (int row = 0; row < 3; ++row) {
                const float cy = y + 58.0f + static_cast<float>(row) * 30.0f;
                drawCircle(window, midX - 62.0f, cy, 10.0f, kGreen);
                sf::RectangleShape tickA({3.0f, 7.0f});
                tickA.setOrigin({1.5f, 7.0f});
                tickA.setPosition({midX - 66.0f, cy + 1.0f});
                tickA.setRotation(sf::degrees(-45.0f));
                tickA.setFillColor(sf::Color::White);
                window.draw(tickA);
                sf::RectangleShape tickB({3.0f, 12.0f});
                tickB.setOrigin({1.5f, 12.0f});
                tickB.setPosition({midX - 66.0f, cy + 1.0f});
                tickB.setRotation(sf::degrees(45.0f));
                tickB.setFillColor(sf::Color::White);
                window.draw(tickB);
                drawRoundedBox(window, midX - 42.0f, cy - 4.0f,
                               108.0f - 22.0f * static_cast<float>(row), 8.0f, 4.0f,
                               sf::Color(228, 204, 174));
            }

            for (int dot = 0; dot < 5; ++dot) {
                drawCircle(window, midX - 56.0f + static_cast<float>(dot) * 28.0f,
                           y + h - 38.0f, 8.0f,
                           dot < 4 ? kAccent : sf::Color(238, 224, 204));
            }
            drawCircle(window, x + 42.0f, y + 40.0f, 5.0f, sf::Color(246, 200, 150));
            drawCircle(window, x + w - 40.0f, y + 70.0f, 4.0f, sf::Color(246, 200, 150));
            drawCircle(window, x + 58.0f, y + h - 34.0f, 4.0f, sf::Color(246, 200, 150));
            drawCircle(window, x + w - 56.0f, y + h - 40.0f, 5.0f, sf::Color(246, 200, 150));
        }

        drawRoundedBox(window, x + 4.0f, y + 4.0f, w - 8.0f, h - 8.0f, 8.0f,
                       sf::Color::Transparent, sf::Color(196, 150, 112), 10.0f);
    };

    centred("Welcome to the yard", 640.0f, 62.0f, 40, kInk);
    centred("Choose how you would like to begin.", 640.0f, 124.0f, 18, kMuted);

    const sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    struct CardText {
        const char* title;
        const char* line1;
        const char* line2;
        const char* line3;
        const char* chips[3];
        const char* action;
    };
    const CardText cards[2] = {
        {"Free Play",
         "Wander the yard at your own pace.",
         "Play ball, feed, praise and chat with",
         "your companion. No tasks, no timer.",
         {"Free roam", "No questionnaire", "Any time"},
         "Start Free Play"},
        {"Research Session",
         "A short guided session of 12 prompts,",
         "then optional free time and a quick",
         "questionnaire. About 10 minutes each.",
         {"12 guided steps", "Questionnaire", "Both companions"},
         "Start Research Session"},
    };

    for (int i = 0; i < 2; ++i) {
        const sf::FloatRect bounds = modeCardRect(i);
        const bool hovered = bounds.contains(mouse);
        const float lift = hovered ? 5.0f : 0.0f;
        const float x = bounds.position.x;
        const float y = bounds.position.y - lift;
        const float w = bounds.size.x;
        const float h = bounds.size.y;

        drawRoundedBox(window, x, y + 16.0f + lift, w, h, 24.0f, sf::Color(88, 56, 40, 24));
        drawRoundedBox(window, x, y + 9.0f + lift * 0.6f, w, h, 24.0f, kShadowSoft);
        drawRoundedBox(window, x, y, w, h, 24.0f, kPanel,
                       hovered ? kAccent : kBorder, hovered ? 3.0f : 2.0f);

        drawModeIllustration(i, x + 24.0f, y + 24.0f, w - 48.0f, 190.0f);

        auto leftText = [&](const std::string& value, float tx, float ty,
                            unsigned int size, sf::Color colour) {
            sf::Text label(font, value, size);
            label.setFillColor(colour);
            const sf::FloatRect textBounds = label.getLocalBounds();
            label.setPosition({tx - textBounds.position.x, ty - textBounds.position.y});
            window.draw(label);
        };

        leftText(cards[i].title, x + 30.0f, y + 236.0f, 30, kInk);
        leftText(cards[i].line1, x + 30.0f, y + 284.0f, 16, kMuted);
        leftText(cards[i].line2, x + 30.0f, y + 308.0f, 16, kMuted);
        leftText(cards[i].line3, x + 30.0f, y + 332.0f, 16, kMuted);

        float chipX = x + 30.0f;
        for (int c = 0; c < 3; ++c) {
            sf::Text chip(font, cards[i].chips[c], 13);
            const sf::FloatRect chipBounds = chip.getLocalBounds();
            const float chipW = chipBounds.size.x + 24.0f;
            drawRoundedBox(window, chipX, y + 376.0f, chipW, 26.0f, 13.0f, kAccentSoft);
            chip.setFillColor(kAccentDark);
            chip.setPosition({chipX + 12.0f - chipBounds.position.x,
                              y + 381.0f - chipBounds.position.y});
            window.draw(chip);
            chipX += chipW + 8.0f;
        }

        const float bx = x + 60.0f;
        const float by = y + h - 74.0f;
        const float bw = w - 120.0f;
        const float bh = 50.0f;
        drawRoundedBox(window, bx, by + 3.0f, bw, bh, 16.0f, kShadowSoft);
        drawRoundedBox(window, bx, by, bw, bh, 16.0f,
                       hovered ? kAccent : kAccentSoft, kAccent, 2.0f);
        centred(cards[i].action, bx + bw * 0.5f, by + 13.0f, 18,
                hovered ? sf::Color::White : kAccentDark);
    }

    centred("You can return to this menu from General Interaction when you are not in the middle of a study.",
            640.0f, 722.0f, 13, kMuted);

    const sf::Vector2f quitMouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    const bool quitHovered = kQuitRect.contains(quitMouse);
    drawRoundedBox(window, 570.0f, 773.0f, 140.0f, 44.0f, 12.0f, kShadowSoft);
    drawRoundedBox(window, 570.0f, 770.0f, 140.0f, 44.0f, 12.0f,
                   quitHovered ? kAccent : kPanel, kBorder, 1.5f);
    centred("Quit", 640.0f, 782.0f, 17, quitHovered ? sf::Color::White : kInk);

    window.display();
}
