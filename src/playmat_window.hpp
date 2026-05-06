#pragma once
#include "game_state.hpp"
#include "ui.hpp"
#include "vcam/vcam.hpp"
#include <SFML/Graphics.hpp>
#include <memory>

// -- Playmat window (the public, stream-shared window) ----------------------

class PlaymatWindow {
public:
    sf::RenderWindow window;

    explicit PlaymatWindow(GameState& gs);

    void handleEvent(const sf::Event& e);
    void render();

    // Attach a Vcam implementation. Takes ownership. Call before the game loop.
    // Pass nullptr to detach (stops and releases the current vcam).
    void setVcam(std::unique_ptr<Vcam> vcam, int fps = 30);

private:
    GameState& state_;
    sf::Font   font_;
    bool       font_loaded_ = false;
    ContextMenu ctx_menu_;
    ContextMenu z_menu_;
    ContextMenu cmd_ctx_menu_;

    PileViewer gy_viewer_;
    PileViewer exile_viewer_;
    sf::FloatRect gy_rect_;
    sf::FloatRect exile_rect_;

    float        w_ = 1280.f, h_ = 800.f;
    float        ui_scale_ = 1.0f;
    sf::Vector2f gy_ctr_, exile_ctr_;

    int          dragged_idx_    = -1;
    sf::Vector2f drag_offset_;
    int          last_click_idx_ = -1;
    sf::Clock    dbl_click_clock_;

    // Virtual camera
    std::unique_ptr<Vcam>    vcam_;
    sf::Clock                vcam_clock_;
    float                    vcam_interval_ = 1.f / 30.f;
    std::vector<std::uint8_t> vcam_buf_;

    void vcamCaptureFrame();

    void reflow(sf::Vector2u size);
    int  cardAt(sf::Vector2f p) const;

    void onMousePress(sf::Vector2f p, sf::Mouse::Button btn, bool shift);
    void onMouseMove(sf::Vector2f p);
    void onMouseRelease();
    void onMouseScroll(sf::Vector2f p, float delta);
    void applyContextAction(int item);
    void applyZAction(int item);
    void applyCmdContextAction(int item);
    int  cmdCardAt(sf::Vector2f p) const;
    void drawAltPreview(sf::RenderTarget& target, const sf::Font* font, sf::Vector2f mouse_pos) const;
};
