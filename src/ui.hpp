#pragma once
#include "card.hpp"     // Zone, DeckPos, Card all live here now
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
    void       handleScroll(float delta);
    void       handleMouseMove(sf::Vector2f p);
    void       draw(sf::RenderTarget& target, const sf::Font* font) const;

private:
    void drawScrollbar(sf::RenderTarget& target, float total_height) const;

    // Returns button hit rects and their destination zones for the hovered row.
    struct ActionBtn { sf::FloatRect rect; Zone to; DeckPos deck_pos; };
    std::vector<ActionBtn> actionButtons(float btn_x, float row_y) const;
};
