#pragma once
#include "card.hpp"
#include <string>
#include <vector>

enum class Zone { DECK, HAND, BATTLEFIELD, GRAVEYARD, EXILE };

// Scrollable overlay that lists the contents of a graveyard or exile pile.
// Each window sets its own `overlay` bounds to match its virtual coordinate space.
struct PileViewer {
    bool                     visible = false;
    std::string              title;
    std::vector<std::string> names;

    // Screen-space (virtual coords) rect for the overlay panel.
    // Set this in the owning window's constructor before first use.
    sf::FloatRect overlay{sf::Vector2f{80.f, 80.f}, sf::Vector2f{620.f, 490.f}};

    void show(const std::string& t, const std::vector<Card>& pile);
    void hide()                    { visible = false; }
    bool handleClick(sf::Vector2f p);   // returns true, dismisses on any click
    void draw(sf::RenderTarget& target, const sf::Font* font) const;
};
