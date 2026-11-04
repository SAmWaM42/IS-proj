#pragma once

#include <SFML/Graphics.hpp>

class ParticipantMenu {
public:
    // The visual labels are Free Play and Research Session; these choices map
    // onto the existing application flow without changing its study logic.
    enum class Choice { GeneralInteraction, BelievabilityStudy, Exit };

    ParticipantMenu();
    Choice run();

private:
    sf::RenderWindow window;
    sf::Font font;

    void draw();
};
