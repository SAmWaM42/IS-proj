// Stand-alone preview of the questionnaire window (for tuning the layout without running the simulation).
//   F1 = consent and background   F2 = rating of Companion A   F3 = rating of Companion B   F4 = comparison
// Finished questionnaires are printed and written to questionnaire-demo-<id>.json. Esc quits.

#include <fstream>
#include <iostream>
#include <optional>

#include <SFML/Graphics.hpp>

#include "questionnaire.h"

int main() {
    sf::RenderWindow window(sf::VideoMode(sf::Vector2u(1280u, 900u)), "Questionnaire preview");
    window.setFramerateLimit(60);
    sf::Font font;
    if (!font.openFromFile("Data/Fonts/UIFont.ttf") &&
        !font.openFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")) {
        std::cerr << "Could not load a font.\n";
        return 1;
    }

    questionnaire::View view(window, font);
    const nlohmann::json context = {{"run_id", "demo-run"}, {"participant_code", "P00"}, {"session_index", 1}};
    view.begin(questionnaire::makeCompanionRating('A', 42), context, 42);
    sf::Clock clock;

    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            } else if (const auto* text = event->getIf<sf::Event::TextEntered>()) {
                view.handleText(text->unicode);
            } else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Escape) window.close();
                else if (key->code == sf::Keyboard::Key::F1) view.begin(questionnaire::makeConsentAndBackground(), context, 1);
                else if (key->code == sf::Keyboard::Key::F2) view.begin(questionnaire::makeCompanionRating('A', 42), context, 42);
                else if (key->code == sf::Keyboard::Key::F3) view.begin(questionnaire::makeCompanionRating('B', 43), context, 43);
                else if (key->code == sf::Keyboard::Key::F4) view.begin(questionnaire::makeComparison(), context, 1);
                else view.handleKey(key->code);
            } else if (const auto* mouse = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mouse->button == sf::Mouse::Button::Left)
                    view.handleClick(window.mapPixelToCoords(mouse->position));
            } else if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>()) {
                view.handleScroll(wheel->delta);
            }
        }
        view.update(clock.restart().asSeconds());
        if (view.finished()) {
            const nlohmann::json result = view.result();
            std::cout << result.dump(2) << std::endl;
            std::ofstream out("questionnaire-demo-" + result.value("questionnaire_id", std::string("result")) + ".json");
            out << result.dump(2) << '\n';
            view.begin(questionnaire::makeCompanionRating('A', 42), context, 42);   // start over for another preview
        }
        window.clear(sf::Color(250, 241, 225));
        view.draw();
        window.display();
    }
    return 0;
}
