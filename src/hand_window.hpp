#pragma once
#include "game_state.hpp"
#include "ui.hpp"
#include <SFML/Graphics.hpp>
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
    PileViewer  pile_viewer_;
    ContextMenu ctx_menu_;   // right-click on hand cards

    // UI Buttons
    Button btn_draw_, btn_shuffle_, btn_reset_, btn_search_, btn_play_;

    // Live window dimensions (updated by reflow on every resize).
    float w_ = 900.f, h_ = 720.f;

    // Pile center positions — recomputed by reflow().
    sf::Vector2f pos_deck_, pos_gy_, pos_exile_, pos_cmd_;

    sf::Vector2f handCardCenter(int idx) const;
    int          handCardAt(sf::Vector2f p) const;
    void         reflow(sf::Vector2u size);   // recalculates all positions
    void         onMousePress(sf::Vector2f p);
    void         onMouseRightClick(sf::Vector2f p);
    void         onMouseMove(sf::Vector2f p);
    void         applyHandContextAction(int item);
    void         drawPileStack(sf::RenderTarget& target, sf::Vector2f center, int count,
                               const std::string& label, sf::Color back_col);
};
