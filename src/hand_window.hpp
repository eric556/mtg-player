#pragma once
#include "game_state.hpp"
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

// ── Simple clickable button ────────────────────────────────────────────────

struct Button {
    sf::FloatRect bounds;
    std::string   label;
    bool          enabled = true;

    bool contains(sf::Vector2f p) const { return enabled && bounds.contains(p); }

    // font may be nullptr — button is drawn without label text in that case.
    void draw(sf::RenderTarget& target, const sf::Font* font,
              sf::Color fill = sf::Color(70, 70, 80)) const;
};

// ── Graveyard / exile pile list overlay ───────────────────────────────────

struct PileViewer {
    bool                     visible = false;
    std::string              title;
    std::vector<std::string> names;

    void show(const std::string& t, const std::vector<Card>& pile);
    void hide()                         { visible = false; }
    bool handleClick(sf::Vector2f p);   // returns true if click was consumed
    void draw(sf::RenderTarget& target, const sf::Font* font) const;

private:
    // Overlay occupies a fixed screen rect; clicking anywhere dismisses it.
    static constexpr float OX = 80.f, OY = 80.f, OW = 620.f, OH = 490.f;
};

// ── Hand window (private — never share this) ───────────────────────────────

class HandWindow {
public:
    sf::RenderWindow window;

    explicit HandWindow(GameState& gs);

    void handleEvent(const sf::Event& e);
    void render();

private:
    GameState&              state_;
    sf::Font font_;
    bool     font_loaded_ = false;
    int                     selected_hand_idx_ = -1;
    PileViewer              pile_viewer_;

    // Buttons
    Button btn_draw_, btn_shuffle_, btn_play_;
    Button btn_life_plus_, btn_life_minus_;
    Button btn_d6_, btn_d8_, btn_d20_;

    // Clickable regions for pile stacks (opening PileViewer).
    sf::FloatRect deck_rect_, gy_rect_, exile_rect_;

    // Center position of the idx-th hand card in window space.
    sf::Vector2f handCardCenter(int idx) const;
    // Returns index of the hand card under p, or -1.
    int handCardAt(sf::Vector2f p) const;

    void onMousePress(sf::Vector2f p);

    // Draw the back-face stack representing a pile (deck / GY / exile).
    void drawPileStack(sf::Vector2f center, int count,
                       const std::string& label, sf::Color back_col);
};
