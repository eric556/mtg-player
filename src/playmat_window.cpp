// playmat_window.cpp — Core setup, layout, input dispatch, and render loop.
// Companion files (same class, split TUs):
//   playmat_tick.cpp  — tick() animation + alt-preview
//   playmat_draw.cpp  — buildDrawList / executeDrawList / drawAltPreview
//   playmat_menus.cpp — applyContextAction / applyZAction / etc.

#include "playmat_window.hpp"
#include "playmat_defs.hpp"
#include "game_action.hpp"
#include "window_utils.hpp"
#include "card.hpp"

// ── Font loading ──────────────────────────────────────────────────────────

static bool tryLoadFont(sf::Font& font)
{
    for (const char* path : {
            "C:/Windows/Fonts/arial.ttf", "C:/Windows/Fonts/calibri.ttf",
            "C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/verdana.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/System/Library/Fonts/Helvetica.ttc"}) {
        if (font.openFromFile(path)) return true;
    }
    return false;
}

// ── Constructor ───────────────────────────────────────────────────────────

PlaymatWindow::PlaymatWindow(GameState& gs) : state_(gs) {
    window.create(sf::VideoMode({1280, 800}), "MTG Sim - Playmat  [share this window]");
    window.setFramerateLimit(60);
    font_loaded_ = tryLoadFont(font_);
    reflow(window.getSize());

    if (vcam_enabled_) {
        if (!stream_rt_.resize({STREAM_W, STREAM_H}) || !vcam_.start(STREAM_W, STREAM_H))
            vcam_enabled_ = false;
    }
}

// ── Layout ────────────────────────────────────────────────────────────────

void PlaymatWindow::reflow(sf::Vector2u size) {
    if (size.x == 0 || size.y == 0) return;

    float new_w = static_cast<float>(size.x) / ui_scale_;
    float new_h = static_cast<float>(size.y) / ui_scale_;

    // Remap battlefield card positions proportionally to the new window size.
    if (w_ > 0.f && h_ > 0.f) {
        float sx = new_w / w_, sy = new_h / h_;
        for (auto& card : state_.battlefield)
            if (!card.is_animating && !card.is_flying_cross_window)
                card.position = {card.position.x * sx, card.position.y * sy};
    }

    w_ = new_w;
    h_ = new_h;

    // GY and exile anchored to top-right.
    gy_ctr_    = {w_ - 90.f,  100.f};
    exile_ctr_ = {w_ - 205.f, 100.f};

    gy_rect_    = {{gy_ctr_.x    - CARD_W/2.f, gy_ctr_.y    - CARD_H/2.f}, {CARD_W, CARD_H}};
    exile_rect_ = {{exile_ctr_.x - CARD_W/2.f, exile_ctr_.y - CARD_H/2.f}, {CARD_W, CARD_H}};

    // Pile viewer: 50% width, full height with 40px padding.
    float pv_w = w_ * 0.5f;
    float pv_h = h_ - 80.f;
    float pv_x = (w_ - pv_w) / 2.f;
    float pv_y = 40.f;
    sf::FloatRect pv_overlay = {{pv_x, pv_y}, {pv_w, pv_h}};
    gy_viewer_.overlay    = pv_overlay;
    exile_viewer_.overlay = pv_overlay;
    cmd_viewer_.overlay   = pv_overlay;

    state_.pos_zone_bf    = {w_ / 2.f, h_ / 2.f};
    state_.pos_zone_gy    = gy_ctr_;
    state_.pos_zone_exile = exile_ctr_;
    state_.pos_zone_cmd   = {PM_CMD_CX, PM_CMD_CY};

    updateView(window, ui_scale_);
}

// ── Hit tests ─────────────────────────────────────────────────────────────

int PlaymatWindow::cardAt(sf::Vector2f p) const {
    for (int i = static_cast<int>(state_.battlefield.size()) - 1; i >= 0; --i)
        if (state_.battlefield[i].contains(p)) return i;
    return -1;
}

int PlaymatWindow::cmdCardAt(sf::Vector2f p) const {
    for (int i = (int)state_.command_zone.size() - 1; i >= 0; --i) {
        float cx = PM_CMD_CX + i * (CARD_W + 10.f);
        sf::FloatRect bounds({cx - CARD_W / 2.f, PM_CMD_CY - CARD_H / 2.f}, {CARD_W, CARD_H});
        if (bounds.contains(p)) return i;
    }
    return -1;
}

// ── Mouse input handlers ──────────────────────────────────────────────────

void PlaymatWindow::onMousePress(sf::Vector2f p, sf::Mouse::Button btn, bool shift) {
    // Helper: handle a PileViewer card-move click and launch the cross-window fly.
    auto handlePileViewerAction = [&](PileViewer& viewer, sf::Vector2f pile_center) {
        auto act = viewer.handleClick(p);
        if (!act.valid) return;
        sf::Vector2i src = window.getPosition()
                         + sf::Vector2i(window.mapCoordsToPixel(pile_center));
        sf::Vector2i dst = state_.zoneDesktopCenter(act.to);
        state_.history.push(
            std::make_unique<MoveCardAction>(act.from, act.index, act.to, act.deck_pos), state_);
        if (Card* c = state_.lastInZone(act.to, act.deck_pos)) {
            if (act.to == Zone::BATTLEFIELD) {
                c->target_position = {w_ / 2.f, h_ / 2.f};
                c->position        = pile_center;
            }
            c->is_flying_cross_window = true;
            c->start_desktop_pos      = src;
            c->end_desktop_pos        = dst;
            c->anim_timer             = 0.f;
            c->is_animating           = false;
        }
    };

    // Floating viewer context menus take highest priority.
    if (gy_bulk_menu_.visible) {
        if (btn == sf::Mouse::Button::Left) {
            int item = gy_bulk_menu_.hitTest(p);
            if (item >= 0) applyGYBulkAction(item);
            gy_bulk_menu_.hide();
        }
        return;
    }
    if (cmd_viewer_ctx_.visible) {
        if (btn == sf::Mouse::Button::Left) {
            int item = cmd_viewer_ctx_.hitTest(p);
            if (item >= 0) applyCmdViewerCtxAction(item);
            cmd_viewer_ctx_.hide();
        }
        return;
    }

    // Active pile viewers intercept all input.
    if (gy_viewer_.visible) {
        if (btn == sf::Mouse::Button::Right)
            gy_bulk_menu_.show(p, -1, GY_BULK_ITEMS, {w_, h_});
        else
            handlePileViewerAction(gy_viewer_, gy_ctr_);
        return;
    }
    if (exile_viewer_.visible) {
        if (btn != sf::Mouse::Button::Right)
            handlePileViewerAction(exile_viewer_, exile_ctr_);
        else
            exile_viewer_.hide();
        return;
    }
    if (cmd_viewer_.visible) {
        if (btn == sf::Mouse::Button::Right) {
            int cidx = cmd_viewer_.handleRightClick(p);
            if (cidx >= 0)
                cmd_viewer_ctx_.show(p, cidx, CMD_VIEWER_CTX_ITEMS, {w_, h_});
            else
                cmd_viewer_.hide();
        } else {
            handlePileViewerAction(cmd_viewer_, {PM_CMD_CX, PM_CMD_CY});
        }
        return;
    }

    // Right-click: open context menus on the battlefield / piles.
    if (btn == sf::Mouse::Button::Right) {
        ctx_menu_.hide(); z_menu_.hide(); cmd_ctx_menu_.hide();
        if (gy_rect_.contains(p) && !state_.graveyard.empty()) {
            gy_bulk_menu_.show(p, -1, GY_BULK_ITEMS, {w_, h_});
            return;
        }
        int cidx = cmdCardAt(p);
        if (cidx >= 0) { cmd_ctx_menu_.show(p, cidx, CMD_ITEMS, {w_, h_}); return; }
        int idx = cardAt(p);
        if (idx < 0) return;
        if (shift) {
            z_menu_.show(p, idx, Z_ITEMS, {w_, h_});
        } else {
            auto items = CTX_ITEMS;
            if (state_.commander_mode && state_.battlefield[idx].is_commander)
                items.push_back("Return to command zone");
            ctx_menu_.show(p, idx, items, {w_, h_});
        }
        return;
    }

    // Left-click: dismiss menus, open viewers, drag/double-tap cards.
    if (btn == sf::Mouse::Button::Left) {
        if (ctx_menu_.visible) {
            int item = ctx_menu_.hitTest(p);
            if (item >= 0) applyContextAction(item);
            ctx_menu_.hide(); return;
        }
        if (z_menu_.visible) {
            int item = z_menu_.hitTest(p);
            if (item >= 0) applyZAction(item);
            z_menu_.hide(); return;
        }
        if (cmd_ctx_menu_.visible) {
            int item = cmd_ctx_menu_.hitTest(p);
            if (item >= 0) applyCmdContextAction(item);
            cmd_ctx_menu_.hide(); return;
        }
        if (gy_rect_.contains(p) && !state_.graveyard.empty()) {
            gy_viewer_.show("Graveyard", state_.graveyard, Zone::GRAVEYARD);
            return;
        }
        if (exile_rect_.contains(p) && !state_.exile.empty()) {
            exile_viewer_.show("Exile", state_.exile, Zone::EXILE);
            return;
        }
        if (state_.commander_mode && !state_.command_zone.empty() && cmdCardAt(p) >= 0) {
            cmd_viewer_.show("Command Zone", state_.command_zone, Zone::COMMAND_ZONE);
            return;
        }
        int idx = cardAt(p);
        if (idx >= 0) {
            if (idx == last_click_idx_ && dbl_click_clock_.getElapsedTime().asSeconds() < 0.4f) {
                bool new_tapped = !state_.battlefield[idx].tapped;
                state_.history.push(std::make_unique<TapAction>(idx, new_tapped), state_);
                last_click_idx_ = -1; dragged_idx_ = -1;
            } else {
                last_click_idx_ = idx; dbl_click_clock_.restart();
                dragged_idx_    = idx; drag_offset_ = p - state_.battlefield[idx].position;
            }
        } else {
            last_click_idx_ = -1;
        }
    }
}

void PlaymatWindow::onMouseMove(sf::Vector2f p) {
    mouse_pos_ = p;
    if (dragged_idx_ >= 0 && dragged_idx_ < static_cast<int>(state_.battlefield.size()))
        state_.battlefield[dragged_idx_].position = p - drag_offset_;
    gy_viewer_.handleMouseMove(p);
    exile_viewer_.handleMouseMove(p);
}

void PlaymatWindow::onMouseRelease() { dragged_idx_ = -1; }

void PlaymatWindow::onMouseScroll(sf::Vector2f p, float delta) {
    if (gy_viewer_.visible)    { gy_viewer_.handleScroll(delta);    return; }
    if (exile_viewer_.visible) { exile_viewer_.handleScroll(delta); return; }
    int idx = cardAt(p);
    if (idx >= 0) {
        state_.battlefield[idx].scale *= (delta > 0) ? 1.1f : 0.9f;
        state_.battlefield[idx].scale  = std::max(0.2f, std::min(5.0f, state_.battlefield[idx].scale));
    }
}

// ── Event dispatch ────────────────────────────────────────────────────────

void PlaymatWindow::handleEvent(const sf::Event& e) {
    if (const auto* mb = e.getIf<sf::Event::MouseButtonPressed>()) {
        auto  p     = window.mapPixelToCoords(mb->position);
        bool  shift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift)
                   || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
        onMousePress(p, mb->button, shift);
    } else if (const auto* mm = e.getIf<sf::Event::MouseMoved>()) {
        onMouseMove(window.mapPixelToCoords(mm->position));
    } else if (const auto* ms = e.getIf<sf::Event::MouseWheelScrolled>()) {
        auto p = window.mapPixelToCoords(ms->position);
        if (ms->wheel == sf::Mouse::Wheel::Vertical) onMouseScroll(p, ms->delta);
    } else if (e.is<sf::Event::MouseButtonReleased>()) {
        onMouseRelease();
    } else if (const auto* kp = e.getIf<sf::Event::KeyPressed>()) {
        bool ctrl = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) ||
                    sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
        if (ctrl) {
            if (kp->code == sf::Keyboard::Key::Equal || kp->code == sf::Keyboard::Key::Add) {
                ui_scale_ = std::min(3.0f, ui_scale_ * 1.1f);
                reflow(window.getSize());
            } else if (kp->code == sf::Keyboard::Key::Hyphen || kp->code == sf::Keyboard::Key::Subtract) {
                ui_scale_ = std::max(0.5f, ui_scale_ / 1.1f);
                reflow(window.getSize());
            } else if (kp->code == sf::Keyboard::Key::Num0 || kp->code == sf::Keyboard::Key::Numpad0) {
                ui_scale_ = 1.0f;
                reflow(window.getSize());
            } else if (kp->code == sf::Keyboard::Key::Z) {
                state_.history.undo(state_);
            } else if (kp->code == sf::Keyboard::Key::Y) {
                state_.history.redo(state_);
            }
        } else {
            onHoverKey(kp->code);
        }
    } else if (const auto* rs = e.getIf<sf::Event::Resized>()) {
        reflow(rs->size);
    }
}

// ── Hover-key actions (G/E/T/X on hovered battlefield card) ──────────────

void PlaymatWindow::onHoverKey(sf::Keyboard::Key key) {
    if (gy_viewer_.visible || exile_viewer_.visible || cmd_viewer_.visible) return;
    if (ctx_menu_.visible || z_menu_.visible || cmd_ctx_menu_.visible) return;

    int idx = cardAt(mouse_pos_);
    if (idx < 0) return;

    sf::Vector2i src = window.getPosition()
                     + sf::Vector2i(window.mapCoordsToPixel(state_.battlefield[idx].position));
    auto flyTo = [&](Card& c, Zone to) {
        c.is_flying_cross_window = true;
        c.start_desktop_pos      = src;
        c.end_desktop_pos        = state_.zoneDesktopCenter(to);
        c.anim_timer             = 0.f;
        c.is_animating           = false;
    };

    switch (key) {
        case sf::Keyboard::Key::G:
            state_.history.push(
                std::make_unique<MoveCardAction>(Zone::BATTLEFIELD, idx, Zone::GRAVEYARD), state_);
            if (auto* c = state_.lastInZone(Zone::GRAVEYARD)) flyTo(*c, Zone::GRAVEYARD);
            break;
        case sf::Keyboard::Key::E:
            state_.history.push(
                std::make_unique<MoveCardAction>(Zone::BATTLEFIELD, idx, Zone::EXILE), state_);
            if (auto* c = state_.lastInZone(Zone::EXILE)) flyTo(*c, Zone::EXILE);
            break;
        case sf::Keyboard::Key::T: {
            bool new_tapped = !state_.battlefield[idx].tapped;
            state_.history.push(std::make_unique<TapAction>(idx, new_tapped), state_);
            break;
        }
        case sf::Keyboard::Key::X: {
            Card copy               = state_.battlefield[idx];
            copy.is_token           = true;
            copy.is_commander       = false;
            copy.tapped             = false;
            copy.selected           = false;
            copy.face_down          = false;
            copy.counters           = 0;
            copy.position           = {w_ / 2.f, h_ / 2.f};
            copy.is_animating       = false;
            copy.is_flying_cross_window = false;
            copy.anim_timer         = 0.f;
            state_.battlefield.push_back(copy);
            break;
        }
        default: break;
    }
}

// ── Render loop ───────────────────────────────────────────────────────────

void PlaymatWindow::render() {
    if (!window.isOpen()) return;

    float dt = frame_clock_.restart().asSeconds();
    tick(dt);
    buildDrawList();

    window.clear(sf::Color(34, 85, 34));
    executeDrawList(window, false);
    window.display();

    if (vcam_enabled_ && vcam_timer_.getElapsedTime().asSeconds() >= (1.f / 60.f)) {
        vcam_timer_.restart();
        stream_rt_.clear(sf::Color(34, 85, 34));
        stream_rt_.setView(sf::View(sf::FloatRect({0.f, 0.f},
            {static_cast<float>(STREAM_W), static_cast<float>(STREAM_H)})));
        executeDrawList(stream_rt_, true);
        stream_rt_.display();
        vcam_.pushFrame(stream_rt_.getTexture().copyToImage());
    }
}
