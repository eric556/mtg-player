#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

// -- Button ----------------------------------------------------------------

struct Button {
    sf::FloatRect bounds;
    std::string   label;
    bool          enabled = true;
    bool          hovered = false;

    bool contains(sf::Vector2f p) const { return enabled && bounds.contains(p); }
    void draw(sf::RenderTarget& target, const sf::Font* font) const;
};

// -- ContextMenu -----------------------------------------------------------

struct ContextMenu {
    bool         visible    = false;
    sf::Vector2f pos;
    int          target_idx = -1;
    std::vector<std::string> items;

    void show(sf::Vector2f p, int idx, const std::vector<std::string>& options, sf::Vector2f window_size);
    void hide() { visible = false; target_idx = -1; items.clear(); }
    int  hitTest(sf::Vector2f p) const;
    void draw(sf::RenderTarget& target, const sf::Font* font) const;
};

