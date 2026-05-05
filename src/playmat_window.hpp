#pragma once
#include "game_state.hpp"
#include <SFML/Graphics.hpp>

// ── Context menu shown on right-click ──────────────────────────────────────

struct ContextMenu {
    bool         visible    = false;
    sf::Vector2f pos;
    int          target_idx = -1;

    void show(sf::Vector2f p, int idx) { pos = p; target_idx = idx; visible = true; }
    void hide()                        { visible = false; target_idx = -1; }

    int  hitTest(sf::Vector2f p) const;
    // font may be nullptr — menu is not drawn in that case.
    void draw(sf::RenderTarget& target, const sf::Font* font) const;
};

// ── Playmat window (the public, stream-shared window) ──────────────────────

class PlaymatWindow {
public:
    sf::RenderWindow window;

    explicit PlaymatWindow(GameState& gs);

    void handleEvent(const sf::Event& e);
    void render();

private:
    GameState&              state_;
    sf::Font font_;
    bool     font_loaded_ = false;
    ContextMenu             ctx_menu_;

    int          dragged_idx_    = -1;
    sf::Vector2f drag_offset_;

    int          last_click_idx_ = -1;
    sf::Clock    dbl_click_clock_;

    int cardAt(sf::Vector2f p) const;

    void onMousePress(sf::Vector2f p, sf::Mouse::Button btn);
    void onMouseMove(sf::Vector2f p);
    void onMouseRelease();
    void applyContextAction(int item);
};
