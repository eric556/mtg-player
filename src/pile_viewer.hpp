#pragma once
#include "card.hpp"     // Zone, DeckPos, Card, CARD_W, CARD_H all live here
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

// -- PileAction ------------------------------------------------------------
// Returned by PileViewer::handleClick(). `valid` is false if the click
// closed the viewer without choosing a destination.

struct PileAction {
    bool    valid    = false;
    int     index    = -1;
    Zone    from     = Zone::GRAVEYARD;
    Zone    to       = Zone::HAND;
    DeckPos deck_pos = DeckPos::TOP;
};

// -- PileViewer ------------------------------------------------------------

struct PileViewer {
    bool                     visible      = false;
    std::string              title;

    struct Entry { const Card* card; int original_idx; };
    std::vector<Entry>       entries;

    Zone                     current_zone = Zone::GRAVEYARD;

    float scroll_offset = 0.f;
    int   hovered_idx   = -1;

    sf::FloatRect overlay{sf::Vector2f{80.f, 80.f}, sf::Vector2f{620.f, 540.f}};

    void       show(const std::string& t, const std::vector<Card>& pile, Zone z);
    void       hide();
    PileAction handleClick(sf::Vector2f p);
    int        handleRightClick(sf::Vector2f p);  // returns hovered card idx, -1 if none; does NOT close
    void       handleScroll(float delta);
    void       handleMouseMove(sf::Vector2f p);
    void       draw(sf::RenderTarget& target, const sf::Font* font) const;

private:
    void drawScrollbar(sf::RenderTarget& target, float total_height) const;

    // Returns button hit rects and their destination zones for the hovered row.
    struct ActionBtn { sf::FloatRect rect; Zone to; DeckPos deck_pos; };
    std::vector<ActionBtn> actionButtons(float btn_x, float row_y) const;
};
