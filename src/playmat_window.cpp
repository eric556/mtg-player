#include "playmat_window.hpp"
#include "window_utils.hpp"
#include "card.hpp"
#include <array>

// Command zone sits in the top-left corner — left-anchored, so position is fixed.
static constexpr float PM_CMD_CX = 65.f, PM_CMD_CY = 100.f;

// Main right-click menu (zone actions + card state)
enum CtxItem {
    CTX_FLIP = 0, CTX_TO_HAND, CTX_TO_GY, CTX_TO_EXILE,
    CTX_TO_DECK_TOP, CTX_TO_DECK_BOT,
    CTX_ADD_CTR, CTX_REM_CTR,
};
static const std::vector<std::string> CTX_ITEMS = {
    "Flip face-down / up",
    "Return to hand",
    "Send to graveyard",
    "Send to exile",
    "To top of deck",
    "To bottom of deck",
    "Add counter",
    "Remove counter",
};

// Right-click menu on command zone cards
enum CmdCtxItem {
    CMD_TO_BF = 0, CMD_TO_HAND, CMD_TO_GY, CMD_TO_EXILE,
    CMD_TO_DECK_TOP, CMD_TO_DECK_BOT,
    CMD_ADD_CTR, CMD_REM_CTR,
};
static const std::vector<std::string> CMD_ITEMS = {
    "Play commander",
    "Return to hand",
    "Send to graveyard",
    "Send to exile",
    "To top of deck",
    "To bottom of deck",
    "Add commander tax",
    "Remove commander tax",
};

// Shift+right-click menu (z-depth ordering only)
enum ZCtxItem {
    ZTX_TOP = 0, ZTX_BOT, ZTX_UP, ZTX_DOWN,
};
static const std::vector<std::string> Z_ITEMS = {
    "Send to front",
    "Send to back",
    "Move up one",
    "Move down one",
};

// Right-click inside GY viewer: bulk zone actions
enum GYBulkItem {
    GY_BULK_TO_BF = 0, GY_BULK_TO_DECK, GY_BULK_TO_EXILE,
};
static const std::vector<std::string> GY_BULK_ITEMS = {
    "All to battlefield",
    "All to deck (top)",
    "All to exile",
};

// Right-click on a card inside the commander viewer
enum CmdViewerCtxItem {
    CMD_VC_PLAY = 0, CMD_VC_ADD_TAX,
};
static const std::vector<std::string> CMD_VIEWER_CTX_ITEMS = {
    "Play commander",
    "Add commander tax",
};

// -- Pile stack helper ------------------------------------------------------

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
    cnt.setOrigin({std::round(lb.position.x + lb.size.x / 2.f), std::round(lb.position.y + lb.size.y / 2.f)});
    cnt.setPosition({std::round(center.x), std::round(center.y)});
    win.draw(cnt);
    sf::Text lbl(*font, label, 11);
    lbl.setFillColor(sf::Color(210, 210, 210, 220));
    sf::FloatRect llb = lbl.getLocalBounds();
    lbl.setOrigin({std::round(llb.position.x + llb.size.x / 2.f), std::round(llb.position.y + llb.size.y)});
    lbl.setPosition({std::round(center.x), std::round(center.y - CARD_H / 2.f - 4.f)});
    win.draw(lbl);
}

// -- PlaymatWindow ----------------------------------------------------------

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

void PlaymatWindow::reflow(sf::Vector2u size) {
    if (size.x == 0 || size.y == 0) return;

    float new_w = static_cast<float>(size.x) / ui_scale_;
    float new_h = static_cast<float>(size.y) / ui_scale_;

    // Remap battlefield card positions proportionally to the new window size
    if (w_ > 0.f && h_ > 0.f) {
        float sx = new_w / w_, sy = new_h / h_;
        for (auto& card : state_.battlefield)
            if (!card.is_animating && !card.is_flying_cross_window)
                card.position = {card.position.x * sx, card.position.y * sy};
    }

    w_ = new_w;
    h_ = new_h;

    // GY and exile anchored to top-right
    gy_ctr_    = {w_ - 90.f,  100.f};
    exile_ctr_ = {w_ - 205.f, 100.f};

    gy_rect_    = {{gy_ctr_.x    - CARD_W/2.f, gy_ctr_.y    - CARD_H/2.f}, {CARD_W, CARD_H}};
    exile_rect_ = {{exile_ctr_.x - CARD_W/2.f, exile_ctr_.y - CARD_H/2.f}, {CARD_W, CARD_H}};

    // Pile viewer: 50% width, full height with 40px padding
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

PlaymatWindow::PlaymatWindow(GameState& gs) : state_(gs) {
    window.create(sf::VideoMode({1280, 800}), "MTG Sim - Playmat  [share this window]");
    window.setFramerateLimit(60);
    font_loaded_ = tryLoadFont(font_);
    reflow(window.getSize());

    if (vcam_enabled_) {
        if (!stream_rt_.resize({STREAM_W, STREAM_H}))
            vcam_enabled_ = false;
        else
            vcam_.start(STREAM_W, STREAM_H);
    }
}

int PlaymatWindow::cardAt(sf::Vector2f p) const {
    for (int i = static_cast<int>(state_.battlefield.size()) - 1; i >= 0; --i)
        if (state_.battlefield[i].contains(p)) return i;
    return -1;
}

void PlaymatWindow::onMousePress(sf::Vector2f p, sf::Mouse::Button btn, bool shift) {
    // -- PileViewer zone-move actions (left-click on hover buttons) -----
    auto handlePileViewerAction = [&](PileViewer& viewer, sf::Vector2f pile_center) {
        auto act = viewer.handleClick(p);
        if (!act.valid) return;
        sf::Vector2i src = window.getPosition() + sf::Vector2i(window.mapCoordsToPixel(pile_center));
        sf::Vector2i dst = state_.zoneDesktopCenter(act.to);
        state_.moveCard(act.from, act.index, act.to, act.deck_pos);
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

    // -- Floating context menus (viewer right-click menus) take priority -
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

    // -- GY viewer ----------------------------------------------------------
    if (gy_viewer_.visible) {
        if (btn == sf::Mouse::Button::Right) {
            gy_bulk_menu_.show(p, -1, GY_BULK_ITEMS, {w_, h_});
        } else {
            handlePileViewerAction(gy_viewer_, gy_ctr_);
        }
        return;
    }

    // -- Exile viewer -------------------------------------------------------
    if (exile_viewer_.visible) {
        if (btn != sf::Mouse::Button::Right)
            handlePileViewerAction(exile_viewer_, exile_ctr_);
        else
            exile_viewer_.hide();
        return;
    }

    // -- Commander viewer ---------------------------------------------------
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


    if (btn == sf::Mouse::Button::Right) {
        ctx_menu_.hide(); z_menu_.hide(); cmd_ctx_menu_.hide();
        // Right-click pile stacks directly — no need to open the viewer first.
        if (gy_rect_.contains(p) && !state_.graveyard.empty()) {
            gy_bulk_menu_.show(p, -1, GY_BULK_ITEMS, {w_, h_});
            return;
        }
        // Check command zone first (top-left, doesn't overlap battlefield).
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
        if (gy_rect_.contains(p) && !state_.graveyard.empty()) { gy_viewer_.show("Graveyard", state_.graveyard, Zone::GRAVEYARD); return; }
        if (exile_rect_.contains(p) && !state_.exile.empty()) { exile_viewer_.show("Exile", state_.exile, Zone::EXILE); return; }
        if (state_.commander_mode && !state_.command_zone.empty() && cmdCardAt(p) >= 0) {
            cmd_viewer_.show("Command Zone", state_.command_zone, Zone::COMMAND_ZONE);
            return;
        }
        int idx = cardAt(p);
        if (idx >= 0) {
            if (idx == last_click_idx_ && dbl_click_clock_.getElapsedTime().asSeconds() < 0.4f) {
                state_.battlefield[idx].tapped = !state_.battlefield[idx].tapped;
                last_click_idx_ = -1; dragged_idx_ = -1;
            } else {
                last_click_idx_ = idx; dbl_click_clock_.restart();
                dragged_idx_ = idx; drag_offset_ = p - state_.battlefield[idx].position;
            }
        } else { last_click_idx_ = -1; }
    }
}

void PlaymatWindow::onMouseMove(sf::Vector2f p) {
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
        state_.battlefield[idx].scale = std::max(0.2f, std::min(5.0f, state_.battlefield[idx].scale));
    }
}

void PlaymatWindow::applyContextAction(int item) {
    int idx = ctx_menu_.target_idx;
    if (idx < 0 || idx >= static_cast<int>(state_.battlefield.size())) return;

    sf::Vector2i src = window.getPosition() + sf::Vector2i(window.mapCoordsToPixel(state_.battlefield[idx].position));
    auto fly = [&](Card& c, Zone to) {
        c.is_flying_cross_window = true;
        c.start_desktop_pos      = src;
        c.end_desktop_pos        = state_.zoneDesktopCenter(to);
        c.anim_timer             = 0.f;
        c.is_animating           = false;
    };

    if (item == static_cast<int>(CTX_ITEMS.size())) {
        state_.moveCard(Zone::BATTLEFIELD, idx, Zone::COMMAND_ZONE);
        if (auto* c = state_.lastInZone(Zone::COMMAND_ZONE)) fly(*c, Zone::COMMAND_ZONE);
        return;
    }
    switch (item) {
        case CTX_FLIP:        state_.battlefield[idx].face_down = !state_.battlefield[idx].face_down; break;
        case CTX_TO_HAND:     state_.moveCard(Zone::BATTLEFIELD, idx, Zone::HAND);
                              if (auto* c = state_.lastInZone(Zone::HAND))     fly(*c, Zone::HAND);     break;
        case CTX_TO_GY:       state_.moveCard(Zone::BATTLEFIELD, idx, Zone::GRAVEYARD);
                              if (auto* c = state_.lastInZone(Zone::GRAVEYARD)) fly(*c, Zone::GRAVEYARD); break;
        case CTX_TO_EXILE:    state_.moveCard(Zone::BATTLEFIELD, idx, Zone::EXILE);
                              if (auto* c = state_.lastInZone(Zone::EXILE))    fly(*c, Zone::EXILE);    break;
        case CTX_TO_DECK_TOP: state_.moveCard(Zone::BATTLEFIELD, idx, Zone::DECK, DeckPos::TOP);
                              if (auto* c = state_.lastInZone(Zone::DECK, DeckPos::TOP))    fly(*c, Zone::DECK); break;
        case CTX_TO_DECK_BOT: state_.moveCard(Zone::BATTLEFIELD, idx, Zone::DECK, DeckPos::BOTTOM);
                              if (auto* c = state_.lastInZone(Zone::DECK, DeckPos::BOTTOM)) fly(*c, Zone::DECK); break;
        case CTX_ADD_CTR:     state_.battlefield[idx].counters++;                                      break;
        case CTX_REM_CTR:     state_.battlefield[idx].counters--;                                      break;
    }
}

void PlaymatWindow::applyZAction(int item) {
    int idx = z_menu_.target_idx;
    if (idx < 0 || idx >= static_cast<int>(state_.battlefield.size())) return;
    switch (item) {
        case ZTX_TOP: {
            auto c = state_.battlefield[idx];
            state_.battlefield.erase(state_.battlefield.begin() + idx);
            state_.battlefield.push_back(c);
            break;
        }
        case ZTX_BOT: {
            auto c = state_.battlefield[idx];
            state_.battlefield.erase(state_.battlefield.begin() + idx);
            state_.battlefield.insert(state_.battlefield.begin(), c);
            break;
        }
        case ZTX_UP:
            if (idx < static_cast<int>(state_.battlefield.size()) - 1)
                std::swap(state_.battlefield[idx], state_.battlefield[idx + 1]);
            break;
        case ZTX_DOWN:
            if (idx > 0)
                std::swap(state_.battlefield[idx], state_.battlefield[idx - 1]);
            break;
    }
}

int PlaymatWindow::cmdCardAt(sf::Vector2f p) const {
    for (int i = (int)state_.command_zone.size() - 1; i >= 0; --i) {
        float cx = PM_CMD_CX + i * (CARD_W + 10.f);
        sf::FloatRect bounds({cx - CARD_W / 2.f, PM_CMD_CY - CARD_H / 2.f}, {CARD_W, CARD_H});
        if (bounds.contains(p)) return i;
    }
    return -1;
}

void PlaymatWindow::applyCmdContextAction(int item) {
    int idx = cmd_ctx_menu_.target_idx;
    if (idx < 0 || idx >= (int)state_.command_zone.size()) return;

    sf::Vector2f cmd_pos = {PM_CMD_CX + idx * (CARD_W + 10.f), PM_CMD_CY};
    sf::Vector2i src = window.getPosition() + sf::Vector2i(window.mapCoordsToPixel(cmd_pos));
    auto fly = [&](Card& c, Zone to) {
        c.is_flying_cross_window = true;
        c.start_desktop_pos      = src;
        c.end_desktop_pos        = state_.zoneDesktopCenter(to);
        c.anim_timer             = 0.f;
        c.is_animating           = false;
    };

    switch (item) {
        case CMD_TO_BF: {
            state_.moveCard(Zone::COMMAND_ZONE, idx, Zone::BATTLEFIELD);
            if (auto* c = state_.lastInZone(Zone::BATTLEFIELD)) {
                c->target_position = {w_ / 2.f, h_ / 2.f};
                c->position        = cmd_pos;
                fly(*c, Zone::BATTLEFIELD);
            }
            break;
        }
        case CMD_TO_HAND:     state_.moveCard(Zone::COMMAND_ZONE, idx, Zone::HAND);
                              if (auto* c = state_.lastInZone(Zone::HAND))     fly(*c, Zone::HAND);     break;
        case CMD_TO_GY:       state_.moveCard(Zone::COMMAND_ZONE, idx, Zone::GRAVEYARD);
                              if (auto* c = state_.lastInZone(Zone::GRAVEYARD)) fly(*c, Zone::GRAVEYARD); break;
        case CMD_TO_EXILE:    state_.moveCard(Zone::COMMAND_ZONE, idx, Zone::EXILE);
                              if (auto* c = state_.lastInZone(Zone::EXILE))    fly(*c, Zone::EXILE);    break;
        case CMD_TO_DECK_TOP: state_.moveCard(Zone::COMMAND_ZONE, idx, Zone::DECK, DeckPos::TOP);
                              if (auto* c = state_.lastInZone(Zone::DECK, DeckPos::TOP))    fly(*c, Zone::DECK); break;
        case CMD_TO_DECK_BOT: state_.moveCard(Zone::COMMAND_ZONE, idx, Zone::DECK, DeckPos::BOTTOM);
                              if (auto* c = state_.lastInZone(Zone::DECK, DeckPos::BOTTOM)) fly(*c, Zone::DECK); break;
        case CMD_ADD_CTR:     state_.command_zone[idx].counters++;                                     break;
        case CMD_REM_CTR:     state_.command_zone[idx].counters--;                                     break;
    }
}

void PlaymatWindow::applyGYBulkAction(int item) {
    Zone to = Zone::HAND;
    DeckPos dp = DeckPos::TOP;
    switch (item) {
        case GY_BULK_TO_BF:   to = Zone::BATTLEFIELD; break;
        case GY_BULK_TO_DECK: to = Zone::DECK;        break;
        case GY_BULK_TO_EXILE:to = Zone::EXILE;       break;
        default: return;
    }
    while (!state_.graveyard.empty())
        state_.moveCard(Zone::GRAVEYARD, (int)state_.graveyard.size() - 1, to, dp);
    gy_viewer_.hide();
}

void PlaymatWindow::applyCmdViewerCtxAction(int item) {
    int viewer_idx = cmd_viewer_ctx_.target_idx;
    if (viewer_idx < 0 || viewer_idx >= (int)cmd_viewer_.entries.size()) return;
    int original_idx = cmd_viewer_.entries[viewer_idx].original_idx;
    if (original_idx < 0 || original_idx >= (int)state_.command_zone.size()) return;

    if (item == CMD_VC_PLAY) {
        sf::Vector2f cmd_pos = {PM_CMD_CX + original_idx * (CARD_W + 10.f), PM_CMD_CY};
        sf::Vector2i src = window.getPosition() + sf::Vector2i(window.mapCoordsToPixel(cmd_pos));
        state_.moveCard(Zone::COMMAND_ZONE, original_idx, Zone::BATTLEFIELD);
        if (auto* c = state_.lastInZone(Zone::BATTLEFIELD)) {
            c->target_position        = {w_ / 2.f, h_ / 2.f};
            c->position               = cmd_pos;
            c->is_flying_cross_window = true;
            c->start_desktop_pos      = src;
            c->end_desktop_pos        = state_.zoneDesktopCenter(Zone::BATTLEFIELD);
            c->anim_timer             = 0.f;
            c->is_animating           = false;
        }
        cmd_viewer_.hide();
    } else if (item == CMD_VC_ADD_TAX) {
        state_.command_zone[original_idx].counters++;
        // Re-open viewer so the updated counter is reflected.
        cmd_viewer_.show("Command Zone", state_.command_zone, Zone::COMMAND_ZONE);
    }
}

void PlaymatWindow::handleEvent(const sf::Event& e) {
    if (const auto* mb = e.getIf<sf::Event::MouseButtonPressed>()) {
        auto p     = window.mapPixelToCoords(mb->position);
        bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift)
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
            }
        }
    } else if (const auto* rs = e.getIf<sf::Event::Resized>()) {
        reflow(rs->size);
    }
}

void PlaymatWindow::drawAltPreview(const DrawCtx& dc) const {
    if (!alt_card_) return;

    constexpr float PS = 4.0f;
    float card_scale = std::min(dc.sx, dc.sy) * PS;
    float pw = CARD_W * PS * dc.sx;
    float ph = CARD_H * PS * dc.sy;

    sf::Vector2f mp = { alt_mouse_window_.x * dc.sx, alt_mouse_window_.y * dc.sy };
    sf::Vector2f pp = mp + sf::Vector2f(20.f * dc.sx, 20.f * dc.sy);

    if (pp.x + pw > dc.tw) pp.x = mp.x - pw - 20.f * dc.sx;
    pp = fitToWindow(pp, {pw, ph}, {dc.tw, dc.th});

    Card preview   = *alt_card_;
    preview.position  = pp + sf::Vector2f(pw / 2.f, ph / 2.f);
    preview.scale     = card_scale;
    preview.tapped    = false;
    preview.face_down = false;
    preview.draw(dc.target, dc.font);
}

void PlaymatWindow::tick(float dt) {
    auto advanceFlyZone = [&](std::vector<Card>& zone) {
        for (auto& card : zone) {
            if (!card.is_flying_cross_window) continue;
            card.anim_timer += dt;
            if (card.anim_timer >= 0.6f) {
                card.is_flying_cross_window = false;
                card.rotation = 0.f;
            }
        }
    };
    advanceFlyZone(state_.hand);
    advanceFlyZone(state_.graveyard);
    advanceFlyZone(state_.exile);
    advanceFlyZone(state_.deck);
    advanceFlyZone(state_.command_zone);

    sf::Vector2f center = {w_ / 2.f, h_ / 2.f};
    sf::Vector2f bottom = {w_ / 2.f, h_ + 300.f};

    for (auto& card : state_.battlefield) {
        if (card.is_flying_cross_window) {
            card.anim_timer += dt;
            constexpr float duration = 0.6f;
            if (card.anim_timer >= duration) {
                card.is_flying_cross_window = false;
                card.rotation = 0.f;
                card.is_animating = true;
                card.anim_timer = 0.25f;
            } else {
                float t = card.anim_timer / duration;
                sf::Vector2i cur = {
                    (int)(card.start_desktop_pos.x + (card.end_desktop_pos.x - card.start_desktop_pos.x) * t),
                    (int)(card.start_desktop_pos.y + (card.end_desktop_pos.y - card.start_desktop_pos.y) * t)
                };
                card.position = window.mapPixelToCoords(cur - window.getPosition());
                float dx = (float)(card.end_desktop_pos.x - card.start_desktop_pos.x);
                float dy = (float)(card.end_desktop_pos.y - card.start_desktop_pos.y);
                card.rotation = std::atan2(dy, dx) * 180.f / 3.14159265f + 90.f;
            }
        } else if (card.is_animating) {
            card.anim_timer += dt;
            constexpr float fly_in = 0.25f, hang = 1.0f, settle = 0.4f;
            if (card.anim_timer <= fly_in) {
                float ease = 1.0f - std::pow(1.0f - card.anim_timer / fly_in, 3.0f);
                card.position = bottom + (center - bottom) * ease;
                card.scale = 4.0f + (2.5f - 4.0f) * ease;
            } else if (card.anim_timer <= fly_in + hang) {
                card.position = center;
                card.scale = 2.5f;
            } else {
                float t = (card.anim_timer - fly_in - hang) / settle;
                if (t >= 1.0f) { t = 1.0f; card.is_animating = false; }
                float ease = 1.0f - std::pow(1.0f - t, 3.0f);
                card.position = center + (card.target_position - center) * ease;
                card.scale = 2.5f + (1.0f - 2.5f) * ease;
            }
        }
    }

    // Resolve alt-preview card (keyboard + hit-test, window coords)
    alt_card_ = nullptr;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LAlt) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RAlt))
    {
        alt_mouse_window_ = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        sf::Vector2f mp   = alt_mouse_window_;

        if (gy_viewer_.visible) {
            if (gy_viewer_.hovered_idx >= 0 && gy_viewer_.hovered_idx < (int)gy_viewer_.entries.size())
                alt_card_ = gy_viewer_.entries[gy_viewer_.hovered_idx].card;
        } else if (exile_viewer_.visible) {
            if (exile_viewer_.hovered_idx >= 0 && exile_viewer_.hovered_idx < (int)exile_viewer_.entries.size())
                alt_card_ = exile_viewer_.entries[exile_viewer_.hovered_idx].card;
        } else {
            int idx = cardAt(mp);
            if (idx >= 0) {
                alt_card_ = &state_.battlefield[idx];
            } else {
                int cidx = cmdCardAt(mp);
                if (cidx >= 0) {
                    alt_card_ = &state_.command_zone[cidx];
                } else {
                    auto hitPile = [&](sf::Vector2f ctr, const std::vector<Card>& pile) -> const Card* {
                        sf::FloatRect r({ctr.x - CARD_W/2.f, ctr.y - CARD_H/2.f}, {CARD_W, CARD_H});
                        if (r.contains(mp))
                            return (pile.empty() || (&pile == &state_.deck && !state_.deck_top_visible))
                                   ? nullptr : &pile.back();
                        return nullptr;
                    };
                    if (auto* c = hitPile(gy_ctr_,     state_.graveyard)) alt_card_ = c;
                    else if (auto* c = hitPile(exile_ctr_, state_.exile)) alt_card_ = c;
                }
            }
        }
    }
}

void PlaymatWindow::buildDrawList() {
    frame_cmds_.clear();

    // ── stream_safe: game board ────────────────────────────────────────────

    frame_cmds_.push_back({[&](const DrawCtx& dc) {
        for (const auto& card : state_.battlefield) {
            if (card.is_flying_cross_window) continue;
            Card d = card;
            d.position = {card.position.x * dc.sx, card.position.y * dc.sy};
            d.draw(dc.target, dc.font);
        }
    }, true});

    frame_cmds_.push_back({[&](const DrawCtx& dc) {
        float tgy_x    = dc.tw - (w_ - gy_ctr_.x);
        float tgy_y    = gy_ctr_.y * dc.sy;
        float texile_x = dc.tw - (w_ - exile_ctr_.x);
        float texile_y = exile_ctr_.y * dc.sy;
        drawPileStack(dc.target, dc.font, {tgy_x, tgy_y},
                      (int)state_.graveyard.size(), "GY",    sf::Color(110, 45, 45, 210));
        drawPileStack(dc.target, dc.font, {texile_x, texile_y},
                      (int)state_.exile.size(),     "EXILE", sf::Color(110, 80, 25, 210));
    }, true});

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
                lbl.setOrigin({lb.position.x + lb.size.x / 2.f, lb.position.y + lb.size.y});
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
            h.setOrigin({std::round(lb.position.x + lb.size.x / 2.f), std::round(lb.position.y)});
            h.setPosition({std::round(ctr.x), std::round(ctr.y + CARD_H / 2.f + 4.f)});
            dc.target.draw(h);
        };
        hint({tgy_x, tgy_y},       !state_.graveyard.empty());
        hint({texile_x, texile_y}, !state_.exile.empty());
    }, true});

    // ── stream_safe: game UI overlays ─────────────────────────────────────

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

    // ── window-only: chrome ────────────────────────────────────────────────

    frame_cmds_.push_back({[&](const DrawCtx& dc) {
        auto renderFlyingZone = [&](const std::vector<Card>& zone) {
            for (const auto& card : zone) {
                if (!card.is_flying_cross_window) continue;
                float t = std::min(1.f, card.anim_timer / 0.6f);
                sf::Vector2i cur = {
                    (int)(card.start_desktop_pos.x + (card.end_desktop_pos.x - card.start_desktop_pos.x) * t),
                    (int)(card.start_desktop_pos.y + (card.end_desktop_pos.y - card.start_desktop_pos.y) * t)
                };
                Card ghost = card;
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

void PlaymatWindow::executeDrawList(sf::RenderTarget& target, bool stream) const {
    const sf::Font* fp = font_loaded_ ? &font_ : nullptr;
    sf::Vector2f vs = target.getView().getSize();
    DrawCtx dc{ target, fp, vs.x / w_, vs.y / h_, vs.x, vs.y };
    for (const auto& cmd : frame_cmds_)
        if (!stream || cmd.stream_safe)
            cmd.fn(dc);
}

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
