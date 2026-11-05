#pragma once

#include <algorithm>

#include <SFML/Graphics.hpp>

// The application UI is authored in a 1280x900 design space. Fit that space
// uniformly inside the resizable window instead of stretching it independently
// along the x and y axes. Extra space is letterboxed.
namespace ui_layout {
inline constexpr sf::Vector2f kDesignSize{1280.0f, 900.0f};

inline sf::View makeLetterboxedView(sf::Vector2u windowSize) {
    sf::View view(sf::FloatRect({0.0f, 0.0f}, kDesignSize));
    if (windowSize.x == 0 || windowSize.y == 0) return view;

    const float scale = std::min(
        static_cast<float>(windowSize.x) / kDesignSize.x,
        static_cast<float>(windowSize.y) / kDesignSize.y);
    const float viewportWidth = (kDesignSize.x * scale) / static_cast<float>(windowSize.x);
    const float viewportHeight = (kDesignSize.y * scale) / static_cast<float>(windowSize.y);
    view.setViewport(sf::FloatRect(
        {(1.0f - viewportWidth) * 0.5f, (1.0f - viewportHeight) * 0.5f},
        {viewportWidth, viewportHeight}));
    return view;
}

inline sf::FloatRect designRectViewport(const sf::RenderWindow& window, sf::FloatRect designRect) {
    const sf::FloatRect base = makeLetterboxedView(window.getSize()).getViewport();
    return sf::FloatRect(
        {base.position.x + (designRect.position.x / kDesignSize.x) * base.size.x,
         base.position.y + (designRect.position.y / kDesignSize.y) * base.size.y},
        {(designRect.size.x / kDesignSize.x) * base.size.x,
         (designRect.size.y / kDesignSize.y) * base.size.y});
}

inline void applyLetterboxedView(sf::RenderWindow& window) {
    window.setView(makeLetterboxedView(window.getSize()));
}
} // namespace ui_layout
