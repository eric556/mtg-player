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

// ── ContextMenu ────────────────────────────────────────────────────────────

static constexpr float MENU_W = 190.f;
static constexpr float ITEM_H = 23.f;

static const std::array<const char*, 5> CTX_ITEMS = {
    "Flip face-down / up",
    "Send to graveyard",
    "Send to exile",
    "Add counter",
    "Remove counter"
};

int ContextMenu::hitTest(sf::Vector2f p) const
{
    if (!visible) return -1;
    for (int i = 0; i < static_cast<int>(CTX_ITEMS.size()); ++i) {
        sf::FloatRect row({pos.x, pos.y + i * ITEM_H}, {MENU_W, ITEM_H});
        if (row.contains(p)) return i;
    }
    return -1;
}

void ContextMenu::draw(sf::RenderTarget& target, const sf::Font* font) const
{
    if (!visible || !font) return;

    const float total_h = CTX_ITEMS.size() * ITEM_H;
    sf::RectangleShape bg({MENU_W, total_h});
    bg.setPosition(pos);
    bg.setFillColor(sf::Color(30, 30, 30, 235));
    bg.setOutlineColor(sf::Color(180, 180, 180));
    bg.setOutlineThickness(1.f);
    target.draw(bg);

    for (int i = 0; i < static_cast<int>(CTX_ITEMS.size()); ++i) {
        sf::Text t(*font, CTX_ITEMS[i], 13);
        t.setFillColor(sf::Color(230, 230, 230));
        t.setPosition({pos.x + 8.f, pos.y + i * ITEM_H + 4.f});
        target.draw(t);
    }
}

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
    cnt.setOrigin({lb.position.x + lb.size.x / 2.f, lb.position.y + lb.size.y / 2.f});
    cnt.setPosition(center);
    win.draw(cnt);
    sf::Text lbl(*font, label, 11);
    lbl.setFillColor(sf::Color(210, 210, 210, 220));
    sf::FloatRect llb = lbl.getLocalBounds();
    lbl.setOrigin({llb.position.x + llb.size.x / 2.f, llb.position.y + llb.size.y});
    lbl.setPosition({center.x, center.y - CARD_H / 2.f - 4.f});
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
        if (cardContains(state_.battlefield[i], p)) return i;
    return -1;
}

void PlaymatWindow::onMousePress(sf::Vector2f p, sf::Mouse::Button btn) {
    if (gy_viewer_.visible)    { gy_viewer_.handleClick(p);    return; }
    if (exile_viewer_.visible) { exile_viewer_.handleClick(p); return; }
    if (btn == sf::Mouse::Button::Right) {
        if (ctx_menu_.visible) { ctx_menu_.hide(); return; }
        int idx = cardAt(p);
        if (idx >= 0) ctx_menu_.show(p, idx);
        return;
    }
    if (btn == sf::Mouse::Button::Left) {
        if (ctx_menu_.visible) {
            int item = ctx_menu_.hitTest(p);
            if (item >= 0) applyContextAction(item);
            ctx_menu_.hide();
            return;
        }
        if (gy_rect_.contains(p) && !state_.graveyard.empty()) { gy_viewer_.show("Graveyard", state_.graveyard); return; }
        if (exile_rect_.contains(p) && !state_.exile.empty()) { exile_viewer_.show("Exile", state_.exile); return; }
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
        case 0: state_.battlefield[idx].face_down = !state_.battlefield[idx].face_down; break;
        case 1: state_.sendToGraveyard(idx); break;
        case 2: state_.sendToExile(idx);     break;
        case 3: state_.battlefield[idx].counters++; break;
        case 4: state_.battlefield[idx].counters--; break;
    }
}

void PlaymatWindow::handleEvent(const sf::Event& e) {
    if (const auto* mb = e.getIf<sf::Event::MouseButtonPressed>()) {
        auto p = window.mapPixelToCoords(mb->position);
        onMousePress(p, mb->button);
    } else if (const auto* mm = e.getIf<sf::Event::MouseMoved>()) {
        auto p = window.mapPixelToCoords(mm->position);
        onMouseMove(p);
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
        if (card.is_animating) {
            card.anim_timer += dt;
            float stay_duration = 1.5f;
            float anim_duration = 0.4f;

            if (card.anim_timer > stay_duration) {
                float t = (card.anim_timer - stay_duration) / anim_duration;
                if (t >= 1.0f) {
                    t = 1.0f;
                    card.is_animating = false;
                }
                
                // ease out cubic for smoother settling
                float ease = 1.0f - std::pow(1.0f - t, 3.0f);
                
                sf::Vector2f start_pos = {PLAYMAT_W / 2.f, PLAYMAT_H / 2.f};
                card.position = start_pos + (card.target_position - start_pos) * ease;
                card.scale = 2.5f + (1.0f - 2.5f) * ease;
            }
        }
        drawCard(window, card, fp);
    }
    drawPileStack(window, fp, {PM_GY_CX, PM_GY_CY}, static_cast<int>(state_.graveyard.size()), "GY", sf::Color(110, 45, 45, 210));
    drawPileStack(window, fp, {PM_EXILE_CX, PM_EXILE_CY}, static_cast<int>(state_.exile.size()), "EXILE", sf::Color(110, 80, 25, 210));
    if (fp) {
        auto hint = [&](float cx, float cy, bool show) {
            if (!show) return;
            sf::Text h(*fp, "click to view", 10);
            h.setFillColor(sf::Color(200, 200, 200, 180));
            sf::FloatRect lb = h.getLocalBounds();
            h.setOrigin({lb.position.x + lb.size.x / 2.f, lb.position.y});
            h.setPosition({cx, cy + CARD_H / 2.f + 4.f});
            window.draw(h);
        };
        hint(PM_GY_CX, PM_GY_CY, !state_.graveyard.empty());
        hint(PM_EXILE_CX, PM_EXILE_CY, !state_.exile.empty());
    }
    ctx_menu_.draw(window, fp);
    gy_viewer_.draw(window, fp);
    exile_viewer_.draw(window, fp);
    window.display();
}
