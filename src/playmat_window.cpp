#include "playmat_window.hpp"
#include "card.hpp"
#include <array>

// ── Virtual coordinate space ───────────────────────────────────────────────

static constexpr float PLAYMAT_W = 1280.f;
static constexpr float PLAYMAT_H = 800.f;

// GY and exile piles sit in the top-right corner of the playmat.
static constexpr float PM_GY_CX    = 1190.f, PM_GY_CY    = 100.f;
static constexpr float PM_EXILE_CX = 1075.f, PM_EXILE_CY = 100.f;

// ── Letterbox view helper ──────────────────────────────────────────────────

static void updateView(sf::RenderWindow& win)
{
    auto sz = win.getSize();
    win.setView(sf::View(sf::FloatRect(
        {0.f, 0.f},
        {static_cast<float>(sz.x), static_cast<float>(sz.y)})));
}

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

// ── Pile stack helper ──────────────────────────────────────────────────────

static void drawPileStack(sf::RenderWindow& win, const sf::Font* font,
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

// ── PlaymatWindow ──────────────────────────────────────────────────────────

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

PlaymatWindow::PlaymatWindow(GameState& gs) : state_(gs) {
    window.create(sf::VideoMode({static_cast<unsigned>(PLAYMAT_W), static_cast<unsigned>(PLAYMAT_H)}),
                  "MTG Sim — Playmat  [share this window]");
    window.setFramerateLimit(60);
    updateView(window);
    font_loaded_ = tryLoadFont(font_);
    gy_rect_    = sf::FloatRect({PM_GY_CX - CARD_W/2.f, PM_GY_CY - CARD_H/2.f}, {CARD_W, CARD_H});
    exile_rect_ = sf::FloatRect({PM_EXILE_CX - CARD_W/2.f, PM_EXILE_CY - CARD_H/2.f}, {CARD_W, CARD_H});
    gy_viewer_.overlay    = sf::FloatRect({200.f, 100.f}, {880.f, 600.f});
    exile_viewer_.overlay = sf::FloatRect({200.f, 100.f}, {880.f, 600.f});
}

int PlaymatWindow::cardAt(sf::Vector2f p) const {
    for (int i = static_cast<int>(state_.battlefield.size()) - 1; i >= 0; --i)
        if (state_.battlefield[i].contains(p)) return i;
    return -1;
}

void PlaymatWindow::onMousePress(sf::Vector2f p, sf::Mouse::Button btn, bool shift) {
    // ── PileViewer actions ─────────────────────────────────────────────
    if (gy_viewer_.visible) {
        auto act = gy_viewer_.handleClick(p);
        if (act.valid) {
            state_.moveCard(act.from, act.index, act.to, act.deck_pos);
            if (act.to == Zone::BATTLEFIELD && !state_.battlefield.empty())
                state_.battlefield.back().position = {PLAYMAT_W / 2.f, PLAYMAT_H / 2.f};
        }
        return;
    }
    if (exile_viewer_.visible) {
        auto act = exile_viewer_.handleClick(p);
        if (act.valid) {
            state_.moveCard(act.from, act.index, act.to, act.deck_pos);
            if (act.to == Zone::BATTLEFIELD && !state_.battlefield.empty())
                state_.battlefield.back().position = {PLAYMAT_W / 2.f, PLAYMAT_H / 2.f};
        }
        return;
    }
    if (btn == sf::Mouse::Button::Right) {
        ctx_menu_.hide(); z_menu_.hide();
        int idx = cardAt(p);
        if (idx < 0) return;
        if (shift)
            z_menu_.show(p, idx, Z_ITEMS);     // Shift+RClick → z-depth menu
        else
            ctx_menu_.show(p, idx, CTX_ITEMS); // RClick → zone/action menu
        return;
    }
    if (btn == sf::Mouse::Button::Left) {
        // Close any open menu on left-click, applying the selected item first.
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
        if (gy_rect_.contains(p) && !state_.graveyard.empty()) { gy_viewer_.show("Graveyard", state_.graveyard, Zone::GRAVEYARD); return; }
        if (exile_rect_.contains(p) && !state_.exile.empty()) { exile_viewer_.show("Exile", state_.exile, Zone::EXILE); return; }
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
    switch (item) {
        case CTX_FLIP:       state_.battlefield[idx].face_down = !state_.battlefield[idx].face_down; break;
        case CTX_TO_HAND:    state_.moveCard(Zone::BATTLEFIELD, idx, Zone::HAND);                    break;
        case CTX_TO_GY:      state_.moveCard(Zone::BATTLEFIELD, idx, Zone::GRAVEYARD);               break;
        case CTX_TO_EXILE:   state_.moveCard(Zone::BATTLEFIELD, idx, Zone::EXILE);                   break;
        case CTX_TO_DECK_TOP: state_.moveCard(Zone::BATTLEFIELD, idx, Zone::DECK, DeckPos::TOP);     break;
        case CTX_TO_DECK_BOT: state_.moveCard(Zone::BATTLEFIELD, idx, Zone::DECK, DeckPos::BOTTOM);  break;
        case CTX_ADD_CTR:    state_.battlefield[idx].counters++;                                     break;
        case CTX_REM_CTR:    state_.battlefield[idx].counters--;                                     break;
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
    } else if (e.is<sf::Event::Resized>()) {
        updateView(window);
    }
}

void PlaymatWindow::render() {
    if (!window.isOpen()) return;
    window.clear(sf::Color(34, 85, 34));
    const sf::Font* fp = font_loaded_ ? &font_ : nullptr;
    if (fp) {
        sf::Text label(*fp, "PLAYMAT — share this window on stream", 11);
        label.setFillColor(sf::Color(180, 220, 180, 150));
        label.setPosition({6.f, 4.f});
        window.draw(label);
    }
    static sf::Clock frame_clock;
    float dt = frame_clock.restart().asSeconds();
    
    for (auto& card : state_.battlefield) {
        if (card.is_flying_cross_window) {
            card.anim_timer += dt;
            float duration = 0.6f; // Quick fly across screen
            if (card.anim_timer >= duration) {
                card.is_flying_cross_window = false;
                card.rotation = 0.f;
                // Transition to Phase 2 of old animation (hang in center)
                card.is_animating = true;
                card.anim_timer = 0.25f; // Starts right at the hang phase
            } else {
                float t = card.anim_timer / duration;
                sf::Vector2i current_global = {
                    (int)(card.start_desktop_pos.x + (card.end_desktop_pos.x - card.start_desktop_pos.x) * t),
                    (int)(card.start_desktop_pos.y + (card.end_desktop_pos.y - card.start_desktop_pos.y) * t)
                };
                
                sf::Vector2i local_pixel = current_global - window.getPosition();
                card.position = window.mapPixelToCoords(local_pixel);
                
                float dx = (float)(card.end_desktop_pos.x - card.start_desktop_pos.x);
                float dy = (float)(card.end_desktop_pos.y - card.start_desktop_pos.y);
                float angle = std::atan2(dy, dx) * 180.f / 3.14159265f;
                card.rotation = angle + 90.f; // point top of card in direction of travel
            }
        } else if (card.is_animating) {
            card.anim_timer += dt;
            
            float fly_in_duration = 0.25f;
            float hang_duration = 1.0f;
            float settle_duration = 0.4f;
            
            sf::Vector2f center = {PLAYMAT_W / 2.f, PLAYMAT_H / 2.f};
            sf::Vector2f bottom = {PLAYMAT_W / 2.f, PLAYMAT_H + 300.f};

            if (card.anim_timer <= fly_in_duration) {
                float t = card.anim_timer / fly_in_duration;
                float ease = 1.0f - std::pow(1.0f - t, 3.0f);
                card.position = bottom + (center - bottom) * ease;
                card.scale = 4.0f + (2.5f - 4.0f) * ease;
            }
            else if (card.anim_timer <= fly_in_duration + hang_duration) {
                card.position = center;
                card.scale = 2.5f;
            }
            else {
                float t = (card.anim_timer - fly_in_duration - hang_duration) / settle_duration;
                if (t >= 1.0f) {
                    t = 1.0f;
                    card.is_animating = false;
                }
                float ease = 1.0f - std::pow(1.0f - t, 3.0f);
                card.position = center + (card.target_position - center) * ease;
                card.scale = 2.5f + (1.0f - 2.5f) * ease;
            }
        }
        card.draw(window, fp);
    }
    drawPileStack(window, fp, {PM_GY_CX, PM_GY_CY}, static_cast<int>(state_.graveyard.size()), "GY", sf::Color(110, 45, 45, 210));
    drawPileStack(window, fp, {PM_EXILE_CX, PM_EXILE_CY}, static_cast<int>(state_.exile.size()), "EXILE", sf::Color(110, 80, 25, 210));
    if (fp) {
        auto hint = [&](float cx, float cy, bool show) {
            if (!show) return;
            sf::Text h(*fp, "click to view", 10);
            h.setFillColor(sf::Color(200, 200, 200, 180));
            sf::FloatRect lb = h.getLocalBounds();
            h.setOrigin({std::round(lb.position.x + lb.size.x / 2.f), std::round(lb.position.y)});
            h.setPosition({std::round(cx), std::round(cy + CARD_H / 2.f + 4.f)});
            window.draw(h);
        };
        hint(PM_GY_CX, PM_GY_CY, !state_.graveyard.empty());
        hint(PM_EXILE_CX, PM_EXILE_CY, !state_.exile.empty());
    }
    ctx_menu_.draw(window, fp);
    z_menu_.draw(window, fp);
    gy_viewer_.draw(window, fp);
    exile_viewer_.draw(window, fp);
    window.display();
}
