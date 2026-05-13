// playmat_draw.cpp — Frame draw-list construction and execution for PlaymatWindow.
// Contains: drawPileStack (file-local helper), buildDrawList, executeDrawList,
//           drawAltPreview.

#include "playmat_window.hpp"
#include "playmat_defs.hpp"
#include "window_utils.hpp"

// ── Pile stack visual helper ──────────────────────────────────────────────

static void drawPileStack(sf::RenderTarget& win, const sf::Font* font,
                          sf::Vector2f center, int count,
                          const std::string& label, sf::Color back_col)
{
    if (count > 0) {
        sf::RectangleShape shadow({CARD_W, CARD_H});
        shadow.setOrigin({CARD_W / 2.f, CARD_H / 2.f});
        for (int s = std::min(count - 1, 3); s >= 1; --s) {
            shadow.setPosition({center.x - s * 2.f, center.y - s * 2.f});
            sf::Color sc = back_col; sc.a = 140;
            shadow.setFillColor(sc);
            win.draw(shadow);
        }
    }
    sf::RectangleShape top({CARD_W, CARD_H});
    top.setOrigin({CARD_W / 2.f, CARD_H / 2.f});
    top.setPosition(center);
    top.setFillColor(count > 0 ? back_col : sf::Color(50, 50, 50, 120));
    top.setOutlineColor(sf::Color(180, 180, 180, 210));
    top.setOutlineThickness(1.5f);
    win.draw(top);
    if (!font) return;
    sf::Text cnt(*font, std::to_string(count), 14);
    cnt.setFillColor(sf::Color::White);
    sf::FloatRect lb = cnt.getLocalBounds();
    cnt.setOrigin({std::round(lb.position.x + lb.size.x / 2.f),
                   std::round(lb.position.y + lb.size.y / 2.f)});
    cnt.setPosition({std::round(center.x), std::round(center.y)});
    win.draw(cnt);
    sf::Text lbl(*font, label, 11);
    lbl.setFillColor(sf::Color(210, 210, 210, 220));
    sf::FloatRect llb = lbl.getLocalBounds();
    lbl.setOrigin({std::round(llb.position.x + llb.size.x / 2.f),
                   std::round(llb.position.y + llb.size.y)});
    lbl.setPosition({std::round(center.x), std::round(center.y - CARD_H / 2.f - 4.f)});
    win.draw(lbl);
}

// ── Alt-preview (hold Alt to magnify hovered card) ────────────────────────

void PlaymatWindow::drawAltPreview(const DrawCtx& dc) const {
    if (!alt_card_) return;

    constexpr float PS = 4.0f;
    float pw = CARD_W * PS * dc.sx;
    float ph = CARD_H * PS * dc.sy;

    sf::Vector2f mp = { alt_mouse_window_.x * dc.sx, alt_mouse_window_.y * dc.sy };
    sf::Vector2f pp = mp + sf::Vector2f(20.f * dc.sx, 20.f * dc.sy);

    if (pp.x + pw > dc.tw) pp.x = mp.x - pw - 20.f * dc.sx;
    pp = fitToWindow(pp, {pw, ph}, {dc.tw, dc.th});

    Card preview    = *alt_card_;
    preview.position  = pp + sf::Vector2f(pw / 2.f, ph / 2.f);
    preview.scale     = std::min(dc.sx, dc.sy) * PS;
    preview.tapped    = false;
    preview.face_down = false;
    preview.draw(dc.target, dc.font);
}

// ── Frame draw-list construction ──────────────────────────────────────────

void PlaymatWindow::buildDrawList() {
    frame_cmds_.clear();

    // ── stream-safe: game board ───────────────────────────────────────────

    // Battlefield cards (skip those currently flying cross-window).
    frame_cmds_.push_back({[&](const DrawCtx& dc) {
        for (const auto& card : state_.battlefield) {
            if (card.is_flying_cross_window) continue;
            Card d = card;
            d.position = {card.position.x * dc.sx, card.position.y * dc.sy};
            d.draw(dc.target, dc.font);
        }
    }, true});

    // GY and exile pile stacks (right-anchored).
    frame_cmds_.push_back({[&](const DrawCtx& dc) {
        float tgy_x    = dc.tw - (w_ - gy_ctr_.x);
        float tgy_y    = gy_ctr_.y * dc.sy;
        float texile_x = dc.tw - (w_ - exile_ctr_.x);
        float texile_y = exile_ctr_.y * dc.sy;
        drawPileStack(dc.target, dc.font, {tgy_x, tgy_y},
                      (int)state_.graveyard.size(), "GY",    sf::Color(110, 45,  45,  210));
        drawPileStack(dc.target, dc.font, {texile_x, texile_y},
                      (int)state_.exile.size(),     "EXILE", sf::Color(110, 80,  25,  210));
    }, true});

    // Command zone (left-anchored).
    frame_cmds_.push_back({[&](const DrawCtx& dc) {
        if (!state_.commander_mode) return;
        float cmd_cx = PM_CMD_CX * dc.sx;
        float cmd_cy = PM_CMD_CY * dc.sy;
        if (state_.command_zone.empty()) {
            sf::RectangleShape outline({CARD_W, CARD_H});
            outline.setOrigin({CARD_W / 2.f, CARD_H / 2.f});
            outline.setPosition({cmd_cx, cmd_cy});
            outline.setFillColor(sf::Color(0, 0, 0, 0));
            outline.setOutlineColor(sf::Color(150, 100, 200, 80));
            outline.setOutlineThickness(1.5f);
            dc.target.draw(outline);
            if (dc.font) {
                sf::Text lbl(*dc.font, "CMD", 11);
                lbl.setFillColor(sf::Color(150, 100, 200, 130));
                sf::FloatRect lb = lbl.getLocalBounds();
                lbl.setOrigin({lb.position.x + lb.size.x / 2.f,
                               lb.position.y + lb.size.y});
                lbl.setPosition({cmd_cx, cmd_cy - CARD_H / 2.f - 4.f});
                dc.target.draw(lbl);
            }
        } else {
            for (int i = 0; i < (int)state_.command_zone.size(); ++i) {
                Card d = state_.command_zone[i];
                d.position = {cmd_cx + i * (CARD_W + 10.f), cmd_cy};
                d.draw(dc.target, dc.font);
            }
        }
    }, true});

    // "Click to view" hint text under GY / exile stacks.
    frame_cmds_.push_back({[&](const DrawCtx& dc) {
        if (!dc.font) return;
        float tgy_x    = dc.tw - (w_ - gy_ctr_.x);
        float tgy_y    = gy_ctr_.y * dc.sy;
        float texile_x = dc.tw - (w_ - exile_ctr_.x);
        float texile_y = exile_ctr_.y * dc.sy;
        auto hint = [&](sf::Vector2f ctr, bool show) {
            if (!show) return;
            sf::Text h(*dc.font, "click to view", 10);
            h.setFillColor(sf::Color(200, 200, 200, 180));
            sf::FloatRect lb = h.getLocalBounds();
            h.setOrigin({std::round(lb.position.x + lb.size.x / 2.f),
                         std::round(lb.position.y)});
            h.setPosition({std::round(ctr.x), std::round(ctr.y + CARD_H / 2.f + 4.f)});
            dc.target.draw(h);
        };
        hint({tgy_x, tgy_y},       !state_.graveyard.empty());
        hint({texile_x, texile_y}, !state_.exile.empty());
    }, true});

    // ── stream-safe: UI overlays (pile viewers + alt preview) ─────────────

    frame_cmds_.push_back({[&](const DrawCtx& dc) {
        auto drawScaledViewer = [&](const PileViewer& viewer) {
            if (!viewer.visible) return;
            PileViewer v = viewer;
            v.overlay.position.x *= dc.sx;
            v.overlay.position.y *= dc.sy;
            v.overlay.size.x     *= dc.sx;
            v.overlay.size.y     *= dc.sy;
            v.draw(dc.target, dc.font);
        };
        drawScaledViewer(gy_viewer_);
        drawScaledViewer(exile_viewer_);
        drawScaledViewer(cmd_viewer_);
    }, true});

    frame_cmds_.push_back({[&](const DrawCtx& dc) {
        drawAltPreview(dc);
    }, true});

    // ── window-only: flying cross-window card ghosts ──────────────────────

    frame_cmds_.push_back({[&](const DrawCtx& dc) {
        auto renderFlyingZone = [&](const std::vector<Card>& zone) {
            for (const auto& card : zone) {
                if (!card.is_flying_cross_window) continue;
                float t = std::min(1.f, card.anim_timer / 0.6f);
                sf::Vector2i cur = {
                    (int)(card.start_desktop_pos.x
                          + (card.end_desktop_pos.x - card.start_desktop_pos.x) * t),
                    (int)(card.start_desktop_pos.y
                          + (card.end_desktop_pos.y - card.start_desktop_pos.y) * t)
                };
                Card ghost  = card;
                ghost.position = window.mapPixelToCoords(cur - window.getPosition());
                ghost.draw(dc.target, dc.font);
            }
        };
        renderFlyingZone(state_.hand);
        renderFlyingZone(state_.graveyard);
        renderFlyingZone(state_.exile);
        renderFlyingZone(state_.deck);
        renderFlyingZone(state_.command_zone);
    }, false});

    // ── window-only: context menus + chrome label ─────────────────────────

    frame_cmds_.push_back({[&](const DrawCtx& dc) {
        ctx_menu_.draw(dc.target, dc.font);
        z_menu_.draw(dc.target, dc.font);
        cmd_ctx_menu_.draw(dc.target, dc.font);
        gy_bulk_menu_.draw(dc.target, dc.font);
        cmd_viewer_ctx_.draw(dc.target, dc.font);
    }, false});

    frame_cmds_.push_back({[&](const DrawCtx& dc) {
        if (!dc.font) return;
        sf::Text label(*dc.font, "PLAYMAT - share this window on stream", 11);
        label.setFillColor(sf::Color(180, 220, 180, 150));
        label.setPosition({6.f, 4.f});
        dc.target.draw(label);
    }, false});
}

// ── Draw-list execution ───────────────────────────────────────────────────

void PlaymatWindow::executeDrawList(sf::RenderTarget& target, bool stream) const {
    const sf::Font* fp = font_loaded_ ? &font_ : nullptr;
    sf::Vector2f vs = target.getView().getSize();
    DrawCtx dc{ target, fp, vs.x / w_, vs.y / h_, vs.x, vs.y };
    for (const auto& cmd : frame_cmds_)
        if (!stream || cmd.stream_safe)
            cmd.fn(dc);
}
