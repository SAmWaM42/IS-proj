#pragma once

#include <SFML/Graphics.hpp>

class ParticipantMenu {
public:
    enum class Choice { GeneralInteraction, BelievabilityStudy, Exit };

    ParticipantMenu();
    Choice run();

private:
    sf::RenderWindow window;
    sf::Font font;
    bool fontLoaded = false;

    void draw();
    void drawOption(const sf::String& label, float y, bool hovered);
};
