#pragma once
#include "game_state.hpp"
#include "zone.hpp"
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

// ── Simple clickable button (Pure SFML) ────────────────────────────────────

struct Button {
    sf::FloatRect bounds;
    std::string   label;
    bool          enabled = true;
    bool          hovered = false;

    bool contains(sf::Vector2f p) const { return enabled && bounds.contains(p); }
    void draw(sf::RenderTarget& target, const sf::Font* font) const;
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

    // UI Buttons
    Button btn_draw_, btn_shuffle_, btn_play_;

    // Layout configuration (Virtual space)
    static constexpr float WIN_W = 900.f;
    static constexpr float WIN_H = 720.f;

    sf::Vector2f handCardCenter(int idx) const;
    int          handCardAt(sf::Vector2f p) const;
    void         onMousePress(sf::Vector2f p);
    void         onMouseMove(sf::Vector2f p);
    void         drawPileStack(sf::RenderTarget& target, sf::Vector2f center, int count,
                               const std::string& label, sf::Color back_col);
    void         updateButtonLayout();
};
