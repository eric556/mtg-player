#include "hand_window.hpp"
#include "window_utils.hpp"
#include "card.hpp"
#include "game_action.hpp"
#include <algorithm>
#include <cstdio>

// -- HandWindow Implementation ----------------------------------------------

static bool tryLoadFont(sf::Font& font) {
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

HandWindow::HandWindow(GameState& gs) : state_(gs) {
    window.create(sf::VideoMode({900, 720}),
                  "MTG Sim - Hand & Controls  [private]", sf::State::Windowed);
    window.setFramerateLimit(60);
    font_loaded_ = tryLoadFont(font_);

    btn_draw_.label         = "Draw";
    btn_shuffle_.label      = "Shuffle";
    btn_reset_.label        = "Reset Deck";
    btn_search_.label       = "Search Deck";
    btn_view_top_.label     = "View Top";
    btn_play_.label         = "Play Card";
    btn_create_token_.label = "Create Token";

    reflow(window.getSize());
    }

    void HandWindow::reflow(sf::Vector2u size) {
    w_ = static_cast<float>(size.x) / ui_scale_;
    h_ = static_cast<float>(size.y) / ui_scale_;

    // Buttons - left-anchored except Create Token + Play which are right-anchored
    constexpr float bx = 20.f, by = 10.f, bh = 35.f;
    btn_draw_.bounds          = {{bx,         by}, {80.f,  bh}};
    btn_shuffle_.bounds       = {{bx + 90.f,  by}, {80.f,  bh}};
    btn_reset_.bounds         = {{bx + 180.f, by}, {100.f, bh}};
    btn_search_.bounds        = {{bx + 290.f, by}, {100.f, bh}};
    btn_view_top_.bounds      = {{bx + 400.f, by}, {85.f,  bh}};
    btn_create_token_.bounds  = {{w_ - 275.f, by}, {120.f, bh}};
    btn_play_.bounds          = {{w_ - 140.f, by}, {120.f, bh}};

    // Pile centers - deck left-anchored, others right-anchored
    pos_deck_  = {65.f,       160.f};
    pos_gy_    = {w_ - 65.f,  160.f};
    pos_exile_ = {w_ - 180.f, 160.f};
    pos_cmd_   = {w_ - 295.f, 160.f};

    // Pile viewer: 50% width, full height with 40px padding
    float pv_w = w_ * 0.5f;
    float pv_h = h_ - 80.f;
    float pv_x = (w_ - pv_w) / 2.f;
    float pv_y = 40.f;
    pile_viewer_.overlay = {{pv_x, pv_y}, {pv_w, pv_h}};

    state_.pos_zone_deck = pos_deck_;
    state_.pos_zone_hand = {w_ / 2.f, h_ - 130.f};

    updateView(window, ui_scale_);
}

sf::Vector2f HandWindow::handCardCenter(int idx) const {
    int   count   = static_cast<int>(state_.hand.size());
    float hand_y  = h_ - 130.f;
    if (count == 0) return {w_ / 2.f, hand_y};
    float slot_w  = std::min(CARD_W + 10.f, (w_ - 40.f) / static_cast<float>(count));
    float total_w = static_cast<float>(count) * slot_w;
    float start_x = (w_ - total_w) / 2.f + slot_w / 2.f;
    return {start_x + idx * slot_w, hand_y};
}

int HandWindow::handCardAt(sf::Vector2f p) const {
    for (int i = 0; i < static_cast<int>(state_.hand.size()); ++i) {
        sf::Vector2f c = handCardCenter(i);
        sf::FloatRect r({c.x - CARD_W / 2.f, c.y - CARD_H / 2.f}, {CARD_W, CARD_H});
        if (r.contains(p)) return i;
    }
    return -1;
}

void HandWindow::onMousePress(sf::Vector2f p) {
    // -- Token dialog intercepts ALL clicks when open ----------------------
    if (token_dialog_open_) {
        // A click outside the dialog box cancels it.
        constexpr float dw = 360.f, dh = 110.f;
        sf::FloatRect dlg({(w_ - dw) / 2.f, (h_ - dh) / 2.f}, {dw, dh});
        if (!dlg.contains(p)) {
            token_dialog_open_ = false;
            token_input_.clear();
        }
        return;
    }

    // -- PileViewer actions -----------------------------------------------
    if (pile_viewer_.visible) {
        auto act = pile_viewer_.handleClick(p);
        if (act.valid) {
            sf::Vector2i src = window.getPosition() + sf::Vector2i(window.mapCoordsToPixel(pos_deck_));
            sf::Vector2i dst = state_.zoneDesktopCenter(act.to);
            state_.history.push(std::make_unique<MoveCardAction>(act.from, act.index, act.to, act.deck_pos), state_);
            if (Card* c = state_.lastInZone(act.to, act.deck_pos)) {
                if (act.to == Zone::BATTLEFIELD) {
                    c->target_position       = {640.f, 400.f};
                    c->position              = pos_deck_;
                }
                c->is_flying_cross_window = true;
                c->start_desktop_pos      = src;
                c->end_desktop_pos        = dst;
                c->anim_timer             = 0.f;
                c->is_animating           = false;
            }
        }
        return;
    }

    // -- Hand context menu ------------------------------------------------
    if (ctx_menu_.visible) {
        int item = ctx_menu_.hitTest(p);
        if (item >= 0) applyHandContextAction(item);
        ctx_menu_.hide();
        return;
    }

    if (btn_draw_.contains(p))    { state_.history.push(std::make_unique<DrawCardAction>(), state_); return; }
    if (btn_shuffle_.contains(p)) { state_.shuffleDeck(); return; }
    if (btn_reset_.contains(p))   { state_.resetAll(); return; }
    if (btn_search_.contains(p))  { pile_viewer_.show("DECK", state_.deck, Zone::DECK); return; }
    if (btn_view_top_.contains(p)) { state_.deck_top_visible = !state_.deck_top_visible; return; }
    if (btn_create_token_.contains(p)) {
        token_dialog_open_ = true;
        token_input_.clear();
        return;
    }
    if (btn_play_.contains(p)) {
        if (selected_hand_idx_ >= 0) {
            float off = static_cast<float>(state_.battlefield.size()) * 115.f;
            float px  = 220.f + std::fmod(off, 820.f);
            float py  = 420.f + (static_cast<int>(off / 820.f) % 2 == 0 ? 0.f : 100.f);
            // Capture the card's screen position BEFORE the move shifts the hand layout.
            sf::Vector2f card_center = handCardCenter(selected_hand_idx_);
            auto act = std::make_unique<MoveCardAction>(Zone::HAND, selected_hand_idx_, Zone::BATTLEFIELD);
            state_.history.push(std::move(act), state_);
            if (Card* c = state_.lastInZone(Zone::BATTLEFIELD)) {
                c->target_position = {px, py};
                c->position        = card_center;
                if (state_.hand_window_ptr && state_.playmat_window_ptr) {
                    c->start_desktop_pos      = state_.hand_window_ptr->getPosition()
                        + state_.hand_window_ptr->mapCoordsToPixel(card_center);
                    sf::Vector2u psz = state_.playmat_window_ptr->getSize();
                    c->end_desktop_pos        = state_.playmat_window_ptr->getPosition()
                        + sf::Vector2i(psz.x / 2, psz.y / 2);
                    c->is_flying_cross_window = true;
                    c->is_animating           = false;
                } else {
                    c->is_animating = true;
                }
                c->anim_timer = 0.f;
            }
            selected_hand_idx_ = -1;
        }
        return;
    }

    // Pile clicks
    auto hitPile = [&](sf::Vector2f center) {
        return sf::FloatRect({center.x - CARD_W/2.f, center.y - CARD_H/2.f}, {CARD_W, CARD_H}).contains(p);
    };

    if (hitPile(pos_deck_))  { state_.history.push(std::make_unique<DrawCardAction>(), state_); return; }
    if (hitPile(pos_gy_))    { pile_viewer_.show("GRAVEYARD",    state_.graveyard,    Zone::GRAVEYARD);    return; }
    if (hitPile(pos_exile_)) { pile_viewer_.show("EXILE",        state_.exile,        Zone::EXILE);        return; }
    if (state_.commander_mode && hitPile(pos_cmd_))
        { pile_viewer_.show("COMMAND ZONE", state_.command_zone, Zone::COMMAND_ZONE); return; }

    int idx = handCardAt(p);
    if (idx >= 0) {
        if (selected_hand_idx_ == idx) {
            state_.hand[idx].selected = false;
            selected_hand_idx_ = -1;
        } else {
            if (selected_hand_idx_ >= 0) state_.hand[selected_hand_idx_].selected = false;
            selected_hand_idx_ = idx;
            state_.hand[idx].selected = true;
        }
    }
}

void HandWindow::onMouseRightClick(sf::Vector2f p) {
    if (pile_viewer_.visible) return;
    if (token_dialog_open_) { token_dialog_open_ = false; token_input_.clear(); return; }
    int idx = handCardAt(p);
    if (idx >= 0) {
        ctx_menu_.show(p, idx, {
            "Send to graveyard",
            "Send to exile",
            "To top of deck",
            "To bottom of deck",
        }, {w_, h_});
    }
}

void HandWindow::applyHandContextAction(int item) {
    int idx = ctx_menu_.target_idx;
    if (idx < 0 || idx >= (int)state_.hand.size()) return;

    sf::Vector2i src = window.getPosition() + sf::Vector2i(window.mapCoordsToPixel(handCardCenter(idx)));
    auto fly = [&](Card& c, Zone to) {
        c.is_flying_cross_window = true;
        c.start_desktop_pos      = src;
        c.end_desktop_pos        = state_.zoneDesktopCenter(to);
        c.anim_timer             = 0.f;
        c.is_animating           = false;
    };

    switch (item) {
        case 0: state_.history.push(std::make_unique<MoveCardAction>(Zone::HAND, idx, Zone::GRAVEYARD), state_);
                if (auto* c = state_.lastInZone(Zone::GRAVEYARD)) fly(*c, Zone::GRAVEYARD); break;
        case 1: state_.history.push(std::make_unique<MoveCardAction>(Zone::HAND, idx, Zone::EXILE), state_);
                if (auto* c = state_.lastInZone(Zone::EXILE))     fly(*c, Zone::EXILE);     break;
        case 2: state_.history.push(std::make_unique<MoveCardAction>(Zone::HAND, idx, Zone::DECK, DeckPos::TOP), state_);
                if (auto* c = state_.lastInZone(Zone::DECK, DeckPos::TOP))    fly(*c, Zone::DECK); break;
        case 3: state_.history.push(std::make_unique<MoveCardAction>(Zone::HAND, idx, Zone::DECK, DeckPos::BOTTOM), state_);
                if (auto* c = state_.lastInZone(Zone::DECK, DeckPos::BOTTOM)) fly(*c, Zone::DECK); break;
    }
    selected_hand_idx_ = -1;
}

void HandWindow::onMouseMove(sf::Vector2f p) {
    btn_draw_.hovered         = btn_draw_.contains(p);
    btn_shuffle_.hovered      = btn_shuffle_.contains(p);
    btn_reset_.hovered        = btn_reset_.contains(p);
    btn_search_.hovered       = btn_search_.contains(p);
    btn_view_top_.hovered     = btn_view_top_.contains(p);
    btn_create_token_.hovered = btn_create_token_.contains(p);
    btn_play_.hovered         = btn_play_.contains(p);
}

void HandWindow::handleEvent(const sf::Event& e) {
    if (const auto* mb = e.getIf<sf::Event::MouseButtonPressed>()) {
        auto p = window.mapPixelToCoords(mb->position);
        if (mb->button == sf::Mouse::Button::Left)
            onMousePress(p);
        else if (mb->button == sf::Mouse::Button::Right)
            onMouseRightClick(p);
    } else if (const auto* mm = e.getIf<sf::Event::MouseMoved>()) {
        auto p = window.mapPixelToCoords(mm->position);
        onMouseMove(p);
        pile_viewer_.handleMouseMove(p);
    } else if (const auto* mws = e.getIf<sf::Event::MouseWheelScrolled>()) {
        if (mws->wheel == sf::Mouse::Wheel::Vertical)
            pile_viewer_.handleScroll(mws->delta);
    } else if (const auto* te = e.getIf<sf::Event::TextEntered>()) {
        if (token_dialog_open_) {
            uint32_t c = te->unicode;
            if (c == '\b' || c == 8) {  // backspace
                if (!token_input_.empty()) token_input_.pop_back();
            } else if (c >= 32 && c < 127) {  // printable ASCII
                if (token_input_.size() < 64)
                    token_input_ += static_cast<char>(c);
            }
        }
    } else if (const auto* kp = e.getIf<sf::Event::KeyPressed>()) {
        if (token_dialog_open_) {
            if (kp->code == sf::Keyboard::Key::Enter) {
                if (!token_input_.empty())
                    state_.createToken(token_input_);
                token_dialog_open_ = false;
                token_input_.clear();
            } else if (kp->code == sf::Keyboard::Key::Escape) {
                token_dialog_open_ = false;
                token_input_.clear();
            }
            return;  // swallow all keys while dialog is open
        }

        bool ctrl = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) ||
                    sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
        if (ctrl) {
            if (kp->code == sf::Keyboard::Key::Z) {
                state_.history.undo(state_);
            } else if (kp->code == sf::Keyboard::Key::Y) {
                state_.history.redo(state_);
            } else if (kp->code == sf::Keyboard::Key::Equal || kp->code == sf::Keyboard::Key::Add) {
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

void HandWindow::drawPileStack(sf::RenderTarget& target, sf::Vector2f center, int count,
                               const std::string& label, sf::Color back_col) {
    sf::RectangleShape top({CARD_W, CARD_H});
    top.setOrigin({CARD_W / 2.f, CARD_H / 2.f});
    top.setPosition(center);
    top.setFillColor(count > 0 ? back_col : sf::Color(50, 50, 50));
    top.setOutlineColor(sf::Color(120, 120, 160));
    top.setOutlineThickness(1.5f);
    target.draw(top);
    if (!font_loaded_) return;
    sf::Text cnt(font_, std::to_string(count), 14);
    cnt.setFillColor(sf::Color::White);
    sf::FloatRect lb = cnt.getLocalBounds();
    cnt.setOrigin({std::round(lb.position.x + lb.size.x / 2.f), std::round(lb.position.y + lb.size.y / 2.f)});
    cnt.setPosition({std::round(center.x), std::round(center.y)});
    target.draw(cnt);
}

void HandWindow::drawAltPreview(sf::RenderTarget& target, const sf::Font* font, sf::Vector2f mouse_pos) const {
    if (!sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LAlt) &&
        !sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RAlt)) return;

    const Card* target_card = nullptr;

    if (pile_viewer_.visible) {
        if (pile_viewer_.hovered_idx >= 0 && pile_viewer_.hovered_idx < (int)pile_viewer_.entries.size())
            target_card = pile_viewer_.entries[pile_viewer_.hovered_idx].card;
    } else {
        int hidx = handCardAt(mouse_pos);
        if (hidx >= 0) target_card = &state_.hand[hidx];
        else {
            auto hitPile = [&](sf::Vector2f center, const std::vector<Card>& pile) -> const Card* {
                if (sf::FloatRect({center.x - CARD_W/2.f, center.y - CARD_H/2.f}, {CARD_W, CARD_H}).contains(mouse_pos))
                    return (pile.empty() || (&pile == &state_.deck && !state_.deck_top_visible)) ? nullptr : &pile.back();
                return nullptr;
            };
            if (auto* c = hitPile(pos_deck_, state_.deck)) target_card = c;
            else if (auto* c = hitPile(pos_gy_, state_.graveyard)) target_card = c;
            else if (auto* c = hitPile(pos_exile_, state_.exile)) target_card = c;
            else if (state_.commander_mode) {
                if (auto* c = hitPile(pos_cmd_, state_.command_zone)) target_card = c;
            }
        }
    }

    if (target_card) {
        constexpr float PS = 4.0f;
        float pw = CARD_W * PS, ph = CARD_H * PS;
        sf::Vector2f pp = mouse_pos + sf::Vector2f(20.f, 20.f);

        // Try right side first, then flip to left if off-screen
        if (pp.x + pw > w_) pp.x = mouse_pos.x - pw - 20.f;
        
        // Final clamp to window bounds using utility
        pp = fitToWindow(pp, {pw, ph}, {w_, h_});

        Card preview = *target_card;
        preview.position  = pp + sf::Vector2f(pw / 2.f, ph / 2.f);
        preview.scale     = PS;
        preview.tapped    = false;
        preview.face_down = false;
        preview.draw(target, font);
    }
}

void HandWindow::render() {
    if (!window.isOpen()) return;
    window.clear(sf::Color(25, 25, 35));

    const sf::Font* fp = font_loaded_ ? &font_ : nullptr;

    btn_draw_.draw(window, fp);
    btn_shuffle_.draw(window, fp);
    btn_reset_.draw(window, fp);
    btn_search_.draw(window, fp);
    
    // Toggle label for View Top button
    btn_view_top_.label = state_.deck_top_visible ? "Hide Top" : "View Top";
    btn_view_top_.draw(window, fp);

    btn_create_token_.draw(window, fp);

    btn_play_.enabled = (selected_hand_idx_ >= 0);
    btn_play_.draw(window, fp);

    // Piles
    drawPileStack(window, pos_deck_,  (int)state_.deck.size(),         "DECK",  sf::Color(30,  30, 110));
    drawPileStack(window, pos_gy_,    (int)state_.graveyard.size(),    "GRAVE", sf::Color(80,  40,  40));
    drawPileStack(window, pos_exile_, (int)state_.exile.size(),        "EXILE", sf::Color(80,  60,  20));
    if (state_.commander_mode)
        drawPileStack(window, pos_cmd_, (int)state_.command_zone.size(), "CMD", sf::Color(70, 30, 100));

    static sf::Clock frame_clock;
    float dt = frame_clock.restart().asSeconds();

    // Hand cards (skip any that are mid-flight — rendered by the fly pass below)
    for (int i = 0; i < (int)state_.hand.size(); ++i) {
        if (state_.hand[i].is_flying_cross_window) continue;
        sf::Vector2f center = handCardCenter(i);
        if (state_.hand[i].is_animating) {
            state_.hand[i].anim_timer += dt;
            constexpr float anim_duration = 0.3f;
            if (state_.hand[i].anim_timer > anim_duration) {
                state_.hand[i].is_animating = false;
            } else {
                float t    = state_.hand[i].anim_timer / anim_duration;
                float ease = 1.0f - std::pow(1.0f - t, 3.0f);
                center = pos_deck_ + (center - pos_deck_) * ease;
            }
        }
        state_.hand[i].draw(window, fp, center);
    }

    // Preview selected card (centered, upper half)
    if (selected_hand_idx_ >= 0 && selected_hand_idx_ < (int)state_.hand.size()) {
        Card preview = state_.hand[selected_hand_idx_];
        preview.scale    = 2.5f;
        preview.selected = false;
        preview.draw(window, fp, {w_ / 2.f, h_ * 0.42f});
    }

    // Flying cards from all zones rendered in this window's viewport
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
            ghost.draw(window, fp);
        }
    };
    renderFlyingZone(state_.battlefield);
    renderFlyingZone(state_.hand);
    renderFlyingZone(state_.graveyard);
    renderFlyingZone(state_.exile);
    renderFlyingZone(state_.deck);
    renderFlyingZone(state_.command_zone);

    if (pile_viewer_.visible) pile_viewer_.draw(window, fp);
    ctx_menu_.draw(window, fp);

    drawAltPreview(window, fp, window.mapPixelToCoords(sf::Mouse::getPosition(window)));

    // Token creation dialog drawn on top of everything
    drawTokenDialog(window);

    window.display();
}

void HandWindow::drawTokenDialog(sf::RenderTarget& target) const {
    if (!token_dialog_open_) return;

    const sf::Font* fp = font_loaded_ ? &font_ : nullptr;

    // Semi-transparent overlay to dim the rest of the UI
    sf::RectangleShape overlay({w_, h_});
    overlay.setFillColor(sf::Color(0, 0, 0, 140));
    target.draw(overlay);

    // Dialog box
    constexpr float dw = 360.f, dh = 110.f;
    sf::Vector2f dlg_pos = {(w_ - dw) / 2.f, (h_ - dh) / 2.f};
    sf::RectangleShape dlg({dw, dh});
    dlg.setPosition(dlg_pos);
    dlg.setFillColor(sf::Color(40, 40, 55));
    dlg.setOutlineColor(sf::Color(140, 180, 255));
    dlg.setOutlineThickness(2.f);
    target.draw(dlg);

    if (!fp) return;

    // Title
    sf::Text title(*fp, "Create Token - type name, press Enter", 13);
    title.setFillColor(sf::Color(200, 220, 255));
    title.setPosition({dlg_pos.x + 10.f, dlg_pos.y + 10.f});
    target.draw(title);

    // Input field background
    sf::RectangleShape field({dw - 20.f, 32.f});
    field.setPosition({dlg_pos.x + 10.f, dlg_pos.y + 38.f});
    field.setFillColor(sf::Color(20, 20, 30));
    field.setOutlineColor(sf::Color(100, 140, 220));
    field.setOutlineThickness(1.5f);
    target.draw(field);

    // Input text with a simple cursor indicator
    std::string display = token_input_ + '|';
    sf::Text input(*fp, display, 14);
    input.setFillColor(sf::Color::White);
    input.setPosition({dlg_pos.x + 15.f, dlg_pos.y + 44.f});
    target.draw(input);

    // Hint line
    sf::Text hint(*fp, "Esc to cancel", 11);
    hint.setFillColor(sf::Color(160, 160, 180));
    hint.setPosition({dlg_pos.x + 10.f, dlg_pos.y + 84.f});
    target.draw(hint);
}
