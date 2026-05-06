#include "hand_window.hpp"
#include "playmat_window.hpp"
#include "window_utils.hpp"
#include "card.hpp"
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

    btn_draw_.label    = "Draw";
    btn_shuffle_.label = "Shuffle";
    btn_reset_.label   = "Reset Deck";
    btn_search_.label  = "Search Deck";
    btn_view_top_.label = "View Top";
    btn_play_.label    = "Play Card";

    reflow(window.getSize());
    }

    void HandWindow::reflow(sf::Vector2u size) {
    w_ = static_cast<float>(size.x) / ui_scale_;
    h_ = static_cast<float>(size.y) / ui_scale_;

    // Buttons - left-anchored except Play which is right-anchored
    constexpr float bx = 20.f, by = 10.f, bh = 35.f;
    btn_draw_.bounds     = {{bx,         by}, {80.f,  bh}};
    btn_shuffle_.bounds  = {{bx + 90.f,  by}, {80.f,  bh}};
    btn_reset_.bounds    = {{bx + 180.f, by}, {100.f, bh}};
    btn_search_.bounds   = {{bx + 290.f, by}, {100.f, bh}};
    btn_view_top_.bounds = {{bx + 400.f, by}, {85.f,  bh}};
    btn_play_.bounds     = {{w_ - 140.f, by}, {120.f, bh}};

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

void HandWindow::moveCardWithAnim(Zone from, int idx, Zone to, DeckPos dp)
{
    sf::Vector2i start_pix, end_pix;
    
    // Get start position based on source zone
    if (from == Zone::HAND) {
        start_pix = window.getPosition() + window.mapCoordsToPixel(handCardCenter(idx));
    } else {
        start_pix = getPileDesktopPos(from);
    }
    
    // Get end position based on destination zone
    // If it's a zone on the Playmat, ask the Playmat window
    if ((to == Zone::GRAVEYARD || to == Zone::EXILE || to == Zone::BATTLEFIELD) && playmat_win_) {
        end_pix = playmat_win_->getPileDesktopPos(to);
    } else {
        end_pix = getPileDesktopPos(to);
    }
    
    state_.moveCardAnimated(from, idx, to, start_pix, end_pix, dp);
}

void HandWindow::onMousePress(sf::Vector2f p) {
    // -- PileViewer actions -----------------------------------------------
    if (pile_viewer_.visible) {
        auto act = pile_viewer_.handleClick(p);
        if (act.valid) {
            moveCardWithAnim(act.from, act.index, act.to, act.deck_pos);
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

    if (btn_draw_.contains(p))    { state_.drawCard(); return; }
    if (btn_shuffle_.contains(p)) { state_.shuffleDeck(); return; }
    if (btn_reset_.contains(p))   { state_.resetAll(); return; }
    if (btn_search_.contains(p))  { pile_viewer_.show("DECK", state_.deck, Zone::DECK); return; }
    if (btn_view_top_.contains(p)) { state_.deck_top_visible = !state_.deck_top_visible; return; }
    if (btn_play_.contains(p)) {
        if (selected_hand_idx_ >= 0) {
            float off = static_cast<float>(state_.battlefield.size()) * 115.f;
            float px  = 220.f + std::fmod(off, 820.f);
            float py  = 420.f + (static_cast<int>(off / 820.f) % 2 == 0 ? 0.f : 100.f);
            state_.playCard(selected_hand_idx_, {px, py}, handCardCenter(selected_hand_idx_));
            selected_hand_idx_ = -1;
        }
        return;
    }

    // Pile clicks
    auto hitPile = [&](sf::Vector2f center) {
        return sf::FloatRect({center.x - CARD_W/2.f, center.y - CARD_H/2.f}, {CARD_W, CARD_H}).contains(p);
    };

    if (hitPile(pos_deck_))  { state_.drawCard(); return; }
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
    switch (item) {
        case 0: moveCardWithAnim(Zone::HAND, idx, Zone::GRAVEYARD);              break;
        case 1: moveCardWithAnim(Zone::HAND, idx, Zone::EXILE);                  break;
        case 2: moveCardWithAnim(Zone::HAND, idx, Zone::DECK, DeckPos::TOP);    break;
        case 3: moveCardWithAnim(Zone::HAND, idx, Zone::DECK, DeckPos::BOTTOM); break;
    }
    selected_hand_idx_ = -1;
}

void HandWindow::onMouseMove(sf::Vector2f p) {
    btn_draw_.hovered    = btn_draw_.contains(p);
    btn_shuffle_.hovered = btn_shuffle_.contains(p);
    btn_reset_.hovered   = btn_reset_.contains(p);
    btn_search_.hovered  = btn_search_.contains(p);
    btn_view_top_.hovered = btn_view_top_.contains(p);
    btn_play_.hovered    = btn_play_.contains(p);
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

    // Hand cards
    for (int i = 0; i < (int)state_.hand.size(); ++i) {
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

    // Flying animations
    for (const auto& card : state_.flying_cards) {
        drawFlyingCard(window, card, fp);
    }

    // Cross-window flying cards (legacy battlefield phase 1)
    for (auto& card : state_.battlefield) {
        if (card.is_flying_cross_window) {
            drawFlyingCard(window, card, fp);
        }
    }

    if (pile_viewer_.visible) pile_viewer_.draw(window, fp);
    ctx_menu_.draw(window, fp);

    drawAltPreview(window, fp, window.mapPixelToCoords(sf::Mouse::getPosition(window)));

    window.display();
}

sf::Vector2i HandWindow::getPileDesktopPos(Zone z) const {
    sf::Vector2f virtual_pos;
    switch (z) {
        case Zone::DECK:         virtual_pos = pos_deck_;  break;
        case Zone::GRAVEYARD:    virtual_pos = pos_gy_;    break;
        case Zone::EXILE:        virtual_pos = pos_exile_; break;
        case Zone::COMMAND_ZONE: virtual_pos = pos_cmd_;   break;
        case Zone::HAND:         virtual_pos = {w_ / 2.f, h_ - 130.f}; break;
        default:                 virtual_pos = {w_ / 2.f, h_ / 2.f};   break;
    }
    return window.getPosition() + window.mapCoordsToPixel(virtual_pos);
}
