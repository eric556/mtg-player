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
    std::vector<const Card*> cards; // Store pointers to cards for art access

    float scroll_offset = 0.f;
    int   hovered_idx   = -1;

    // Screen-space (virtual coords) rect for the overlay panel.
    sf::FloatRect overlay{sf::Vector2f{80.f, 80.f}, sf::Vector2f{620.f, 540.f}};

    void show(const std::string& t, const std::vector<Card>& pile);
    void hide();
    bool handleClick(sf::Vector2f p);
    void handleScroll(float delta);
    void handleMouseMove(sf::Vector2f p);
    void draw(sf::RenderTarget& target, const sf::Font* font) const;

private:
    void drawScrollbar(sf::RenderTarget& target, float total_height) const;
};
