#include "ui.hpp"
#include "window_utils.hpp"
#include <cmath>

// -- Button -----------------------------------------------------------------

void Button::draw(sf::RenderTarget& target, const sf::Font* font) const {
    sf::RectangleShape shape(bounds.size);
    shape.setPosition(bounds.position);
    sf::Color fill = enabled ? (hovered ? sf::Color(100, 100, 110) : sf::Color(70, 70, 80))
                             : sf::Color(40, 40, 45);
    shape.setFillColor(fill);
    shape.setOutlineColor(sf::Color(120, 120, 130));
    shape.setOutlineThickness(1.f);
    target.draw(shape);

    if (font && !label.empty()) {
        sf::Text text(*font, label, 14);
        text.setFillColor(enabled ? sf::Color::White : sf::Color(100, 100, 100));
        sf::FloatRect lb = text.getLocalBounds();
        text.setOrigin({std::round(lb.position.x + lb.size.x / 2.f),
                        std::round(lb.position.y + lb.size.y / 2.f)});
        text.setPosition({std::round(bounds.position.x + bounds.size.x / 2.f),
                          std::round(bounds.position.y + bounds.size.y / 2.f)});
        target.draw(text);
    }
}

// -- ContextMenu -------------------------------------------------------------

static constexpr float ITEM_H = 23.f;
static constexpr float MENU_W = 200.f;

void ContextMenu::show(sf::Vector2f p, int idx, const std::vector<std::string>& options, sf::Vector2f window_size)
{
    items = options;
    target_idx = idx;
    visible = true;
    
    float total_h = items.size() * ITEM_H;
    pos = fitToWindow(p, {MENU_W, total_h}, window_size);
}

int ContextMenu::hitTest(sf::Vector2f p) const
{
    if (!visible) return -1;
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        sf::FloatRect row({pos.x, pos.y + i * ITEM_H}, {MENU_W, ITEM_H});
        if (row.contains(p)) return i;
    }
    return -1;
}

void ContextMenu::draw(sf::RenderTarget& target, const sf::Font* font) const
{
    if (!visible || !font) return;
    const float total_h = items.size() * ITEM_H;
    sf::RectangleShape bg({MENU_W, total_h});
    bg.setPosition(pos);
    bg.setFillColor(sf::Color(30, 30, 30, 235));
    bg.setOutlineColor(sf::Color(180, 180, 180));
    bg.setOutlineThickness(1.f);
    target.draw(bg);
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        sf::Text t(*font, items[i], 13);
        t.setFillColor(sf::Color(230, 230, 230));
        t.setPosition({std::round(pos.x + 8.f), std::round(pos.y + i * ITEM_H + 4.f)});
        target.draw(t);
    }
}

