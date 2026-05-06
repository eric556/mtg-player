#pragma once
#include "game_state.hpp"
#include "ui.hpp"
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

class PlaymatWindow;

// -- Hand window (private - never share this) -------------------------------

class HandWindow {
public:
    sf::RenderWindow window;

    explicit HandWindow(GameState& gs);

    void setPlaymatWindow(PlaymatWindow* pw) { playmat_win_ = pw; }

    void handleEvent(const sf::Event& e);
    void render();

    sf::Vector2i getPileDesktopPos(Zone z) const;

private:
    GameState& state_;
    PlaymatWindow* playmat_win_ = nullptr;
    sf::Font   font_;
    bool       font_loaded_ = false;
    int        selected_hand_idx_ = -1;
    PileViewer  pile_viewer_;
    ContextMenu ctx_menu_;   // right-click on hand cards

    // UI Buttons
    Button btn_draw_, btn_shuffle_, btn_reset_, btn_search_, btn_play_, btn_view_top_;

    // Live window dimensions (updated by reflow on every resize).
    float w_ = 900.f, h_ = 720.f;
    float ui_scale_ = 1.0f;

    // Pile center positions - recomputed by reflow().
    sf::Vector2f pos_deck_, pos_gy_, pos_exile_, pos_cmd_;

    sf::Vector2f handCardCenter(int idx) const;
    int          handCardAt(sf::Vector2f p) const;
    void         reflow(sf::Vector2u size);   // recalculates all positions
    void         onMousePress(sf::Vector2f p);
    void         onMouseRightClick(sf::Vector2f p);
    void         onMouseMove(sf::Vector2f p);
    void         applyHandContextAction(int item);
    void         moveCardWithAnim(Zone from, int idx, Zone to, DeckPos dp = DeckPos::TOP);
    void         drawPileStack(sf::RenderTarget& target, sf::Vector2f center, int count,
                               const std::string& label, sf::Color back_col);
    void         drawAltPreview(sf::RenderTarget& target, const sf::Font* font, sf::Vector2f mouse_pos) const;
};
