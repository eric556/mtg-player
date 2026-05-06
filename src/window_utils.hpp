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

// Clamps a rectangle to stay within the window bounds.
// Returns a new position for the rectangle of size 'rect_size' starting at 'pos'.
inline sf::Vector2f fitToWindow(sf::Vector2f pos, sf::Vector2f rect_size, sf::Vector2f window_size, float margin = 5.f) {
    sf::Vector2f out = pos;
    if (out.x + rect_size.x > window_size.x - margin) out.x = window_size.x - rect_size.x - margin;
    if (out.y + rect_size.y > window_size.y - margin) out.y = window_size.y - rect_size.y - margin;
    if (out.x < margin) out.x = margin;
    if (out.y < margin) out.y = margin;
    return out;
}

// Draws a card that is flying in desktop space.
inline void drawFlyingCard(sf::RenderWindow& window, const Card& card, const sf::Font* font) {
    float t = card.anim_timer / 0.6f;
    if (t > 1.0f) t = 1.0f;
    sf::Vector2i cur = {
        (int)(card.start_desktop_pos.x + (card.end_desktop_pos.x - card.start_desktop_pos.x) * t),
        (int)(card.start_desktop_pos.y + (card.end_desktop_pos.y - card.start_desktop_pos.y) * t)
    };
    Card cross = card;
    cross.position = window.mapPixelToCoords(cur - window.getPosition());
    cross.draw(window, font);
}
