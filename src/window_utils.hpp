#pragma once
#include <SFML/Graphics.hpp>

// Sets the view to match the actual window pixel size (1:1 mapping).
// Call on construction and on every Resized event.
inline void updateView(sf::RenderWindow& win, float scale = 1.0f) {
    auto sz = win.getSize();
    win.setView(sf::View(sf::FloatRect(
        {0.f, 0.f},
        {static_cast<float>(sz.x) / scale, static_cast<float>(sz.y) / scale})));
}
