#pragma once
#include "game_state.hpp"
#include "zone.hpp"
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

// ── Simple clickable button ────────────────────────────────────────────────

struct Button {
    sf::FloatRect bounds;
    std::string   label;
    bool          enabled = true;

    bool contains(sf::Vector2f p) const { return enabled && bounds.contains(p); }
    void draw(sf::RenderTarget& target, const sf::Font* font,
              sf::Color fill = sf::Color(70, 70, 80)) const;
};

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

    // Buttons
    Button btn_draw_, btn_shuffle_, btn_play_;
    Button btn_life_plus_, btn_life_minus_;
    Button btn_d6_, btn_d8_, btn_d20_;

    // Clickable regions for pile stacks.
    sf::FloatRect deck_rect_, gy_rect_, exile_rect_;

    sf::Vector2f handCardCenter(int idx) const;
    int          handCardAt(sf::Vector2f p) const;
    void         onMousePress(sf::Vector2f p);
    void         drawPileStack(sf::Vector2f center, int count,
                               const std::string& label, sf::Color back_col);
};
