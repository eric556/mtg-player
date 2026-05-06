#pragma once
#include "game_state.hpp"
#include "ui.hpp"
#include <SFML/Graphics.hpp>

class HandWindow;

// -- Playmat window (the public, stream-shared window) ----------------------

class PlaymatWindow {
public:
    sf::RenderWindow window;

    explicit PlaymatWindow(GameState& gs);

    void setHandWindow(HandWindow* hw) { hand_win_ = hw; }

    void handleEvent(const sf::Event& e);
    void render();

    sf::Vector2i getPileDesktopPos(Zone z) const;

private:
    GameState& state_;
    HandWindow* hand_win_ = nullptr;
    sf::Font   font_;
    bool       font_loaded_ = false;
    ContextMenu ctx_menu_;     // right-click on battlefield: zone actions
    ContextMenu z_menu_;       // shift+right-click on battlefield: z-depth
    ContextMenu cmd_ctx_menu_; // right-click on command zone card

    // Graveyard and exile viewers visible on the shared window.
    PileViewer gy_viewer_;
    PileViewer exile_viewer_;
    sf::FloatRect gy_rect_;
    sf::FloatRect exile_rect_;

    // Live window dimensions and right-anchored pile centers - updated by reflow().
    float        w_ = 1280.f, h_ = 800.f;
    float        ui_scale_ = 1.0f;
    sf::Vector2f gy_ctr_, exile_ctr_;

    int          dragged_idx_    = -1;
    sf::Vector2f drag_offset_;
    int          last_click_idx_ = -1;
    sf::Clock    dbl_click_clock_;

    void reflow(sf::Vector2u size);
    int  cardAt(sf::Vector2f p) const;

    void onMousePress(sf::Vector2f p, sf::Mouse::Button btn, bool shift);
    void onMouseMove(sf::Vector2f p);
    void onMouseRelease();
    void onMouseScroll(sf::Vector2f p, float delta);
    void applyContextAction(int item);
    void applyZAction(int item);
    void applyCmdContextAction(int item);
    void moveCardWithAnim(Zone from, int idx, Zone to, DeckPos dp = DeckPos::TOP);
    int  cmdCardAt(sf::Vector2f p) const;
    void drawAltPreview(sf::RenderTarget& target, const sf::Font* font, sf::Vector2f mouse_pos) const;
};
