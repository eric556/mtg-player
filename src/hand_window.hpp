#pragma once
#include "game_state.hpp"
#include "zone.hpp"
#include "clay_sfml_renderer.hpp"
#include <SFML/Graphics.hpp>
extern "C" {
    #include <clay.h>
}
#include <string>
#include <vector>

// ── Hand window (private — never share this) ───────────────────────────────

class HandWindow {
public:
    sf::RenderWindow window;

    explicit HandWindow(GameState& gs);

    void handleEvent(const sf::Event& e);
    void render();

private:
    GameState& state_;
    sf::Font   font_;
    bool       font_loaded_ = false;
    int        selected_hand_idx_ = -1;
    PileViewer pile_viewer_;
    
    std::unique_ptr<ClaySFMLRenderer> clay_renderer_;

    // Clickable regions for pile stacks.
    sf::FloatRect deck_rect_, gy_rect_, exile_rect_;

    sf::Vector2f handCardCenter(int idx) const;
    int          handCardAt(sf::Vector2f p) const;
    void         onMousePress(sf::Vector2f p);
    void         drawPileStack(sf::Vector2f center, int count,
                               const std::string& label, sf::Color back_col);
    
    void setupClay();
    void createClayLayout();
};
