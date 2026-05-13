#pragma once
#include "game_state.hpp"
#include "ui.hpp"
#include "vcam_wrapper.hpp"
#include <SFML/Graphics.hpp>
#include <functional>
#include <vector>

// -- Per-target rendering context -------------------------------------------
// Built once per executeDrawList call; passed to every DrawCmd function.

struct DrawCtx {
    sf::RenderTarget& target;
    const sf::Font*   font;
    float sx, sy;   // window-logical → target-logical scale
    float tw, th;   // target logical dimensions
};

// A single draw operation and whether it should appear on the stream.
struct DrawCmd {
    std::function<void(const DrawCtx&)> fn;
    bool stream_safe = true;
};

// -- Playmat window (the public, stream-shared window) ----------------------

class PlaymatWindow {
public:
    sf::RenderWindow window;

    explicit PlaymatWindow(GameState& gs);

    void handleEvent(const sf::Event& e);
    void render();

private:
    GameState& state_;
    sf::Font   font_;
    bool       font_loaded_ = false;

    // Virtual Camera — stream_rt_ renders at a fixed resolution independent of window
    static constexpr unsigned STREAM_W = 1920;
    static constexpr unsigned STREAM_H = 1080;
    VCamWrapper       vcam_;
    sf::RenderTexture stream_rt_;
    sf::Clock         vcam_timer_;
    bool              vcam_enabled_ = true;

    ContextMenu ctx_menu_;        // right-click on battlefield card: zone actions
    ContextMenu z_menu_;          // shift+right-click on battlefield: z-depth
    ContextMenu cmd_ctx_menu_;    // right-click on command zone card (inline)
    ContextMenu gy_bulk_menu_;    // right-click inside GY viewer: bulk actions
    ContextMenu cmd_viewer_ctx_;  // right-click on card in commander viewer

    // Pile viewers.
    PileViewer gy_viewer_;
    PileViewer exile_viewer_;
    PileViewer cmd_viewer_;       // opened by left-clicking the command zone area
    sf::FloatRect gy_rect_;
    sf::FloatRect exile_rect_;

    // Live window dimensions and right-anchored pile centers - updated by reflow().
    float        w_ = 1280.f, h_ = 800.f;
    float        ui_scale_ = 1.0f;
    sf::Vector2f gy_ctr_, exile_ctr_;

    sf::Clock    frame_clock_;
    int          dragged_idx_    = -1;
    sf::Vector2f drag_offset_;
    int          last_click_idx_ = -1;
    sf::Clock    dbl_click_clock_;

    // Draw command list — rebuilt every frame in buildDrawList().
    std::vector<DrawCmd> frame_cmds_;

    // Alt-preview state — resolved once in tick(), consumed by DrawCmd.
    const Card*  alt_card_         = nullptr;
    sf::Vector2f alt_mouse_window_;

    void reflow(sf::Vector2u size);
    void tick(float dt);
    void buildDrawList();
    void executeDrawList(sf::RenderTarget& target, bool stream) const;
    void drawAltPreview(const DrawCtx& dc) const;
    int  cardAt(sf::Vector2f p) const;

    void onMousePress(sf::Vector2f p, sf::Mouse::Button btn, bool shift);
    void onMouseMove(sf::Vector2f p);
    void onMouseRelease();
    void onMouseScroll(sf::Vector2f p, float delta);
    void applyContextAction(int item);
    void applyZAction(int item);
    void applyCmdContextAction(int item);
    void applyGYBulkAction(int item);
    void applyCmdViewerCtxAction(int item);
    int  cmdCardAt(sf::Vector2f p) const;
};
