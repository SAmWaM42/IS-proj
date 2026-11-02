#include "participant-menu.h"

#include <stdexcept>

namespace {
const sf::Color background(250, 241, 225);
const sf::Color panel(255, 250, 240);
const sf::Color ink(84, 58, 44);
const sf::Color muted(150, 118, 96);
const sf::Color accent(196, 112, 84);
}

ParticipantMenu::ParticipantMenu()
    : window(sf::VideoMode(sf::Vector2u(900u, 620u)), "Animal Agent") {
    window.setFramerateLimit(60);
    fontLoaded = font.openFromFile("Data/Fonts/UIFont.ttf") ||
                 font.openFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    if (!fontLoaded) {
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
                const auto point = window.mapPixelToCoords(click->position);
                if (point.y >= 270.0f && point.y < 340.0f) {
                    window.close();
                    return Choice::GeneralInteraction;
                }
                if (point.y >= 360.0f && point.y < 430.0f) {
                    window.close();
                    return Choice::BelievabilityStudy;
                }
                if (point.y >= 450.0f && point.y < 520.0f) {
                    window.close();
                    return Choice::Exit;
                }
            }
        }
        draw();
    }
    return Choice::Exit;
}

void ParticipantMenu::drawOption(const sf::String& label, float y, bool hovered) {
    sf::RectangleShape option(sf::Vector2f(520.0f, 58.0f));
    option.setPosition({190.0f, y});
    option.setFillColor(hovered ? sf::Color(247, 218, 192) : panel);
    option.setOutlineColor(sf::Color(228, 204, 174));
    option.setOutlineThickness(1.0f);
    window.draw(option);

    sf::Text text(font, label, 22);
    text.setFillColor(ink);
    const auto bounds = text.getLocalBounds();
    text.setPosition({450.0f - bounds.size.x * 0.5f, y + 13.0f});
    window.draw(text);
}

void ParticipantMenu::draw() {
    window.clear(background);

    sf::Text title(font, "Animal Agent", 38);
    title.setFillColor(ink);
    const auto titleBounds = title.getLocalBounds();
    title.setPosition({450.0f - titleBounds.size.x * 0.5f, 105.0f});
    window.draw(title);

    sf::Text subtitle(font, "Choose how you would like to continue", 17);
    subtitle.setFillColor(muted);
    const auto subtitleBounds = subtitle.getLocalBounds();
    subtitle.setPosition({450.0f - subtitleBounds.size.x * 0.5f, 175.0f});
    window.draw(subtitle);

    const auto mouse = sf::Mouse::getPosition(window);
    const auto point = window.mapPixelToCoords(mouse);
    drawOption("General Interaction", 270.0f, point.y >= 270.0f && point.y < 340.0f);
    drawOption("Believability Study", 360.0f, point.y >= 360.0f && point.y < 430.0f);
    drawOption("Exit", 450.0f, point.y >= 450.0f && point.y < 520.0f);

    sf::Text note(font, "Menu placeholder - visual design can be replaced independently", 12);
    note.setFillColor(muted);
    note.setPosition({190.0f, 565.0f});
    window.draw(note);

    window.display();
}
