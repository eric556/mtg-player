#include "playmat_window.hpp"
#include "card.hpp"
#include <array>

// ── ContextMenu constants ──────────────────────────────────────────────────

static constexpr float MENU_W = 190.f;
static constexpr float ITEM_H = 23.f;

static const std::array<const char*, 5> CTX_ITEMS = {
    "Flip face-down / up",
    "Send to graveyard",
    "Send to exile",
    "Add counter",
    "Remove counter"
};

// ── ContextMenu ────────────────────────────────────────────────────────────

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

// ── PlaymatWindow ──────────────────────────────────────────────────────────

static bool tryLoadFont(sf::Font& font)
{
    for (const char* path : {
            "C:/Windows/Fonts/arial.ttf",
            "C:/Windows/Fonts/calibri.ttf",
            "C:/Windows/Fonts/segoeui.ttf",
            "C:/Windows/Fonts/verdana.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/System/Library/Fonts/Helvetica.ttc"}) {
        if (font.openFromFile(path)) return true;
    }
    return false;
}

PlaymatWindow::PlaymatWindow(GameState& gs)
    : state_(gs)
{
    window.create(sf::VideoMode({1280u, 800u}),
                  "MTG Sim — Playmat  [share this window]");
    window.setFramerateLimit(60);

    font_loaded_ = tryLoadFont(font_);
    if (!font_loaded_)
        fprintf(stderr, "[playmat] Warning: no system font found; card names hidden.\n");
}

int PlaymatWindow::cardAt(sf::Vector2f p) const
{
    for (int i = static_cast<int>(state_.battlefield.size()) - 1; i >= 0; --i) {
        if (cardContains(state_.battlefield[i], p)) return i;
    }
    return -1;
}

void PlaymatWindow::onMousePress(sf::Vector2f p, sf::Mouse::Button btn)
{
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

        int idx = cardAt(p);
        if (idx >= 0) {
            if (idx == last_click_idx_ &&
                dbl_click_clock_.getElapsedTime().asSeconds() < 0.4f) {
                state_.battlefield[idx].tapped = !state_.battlefield[idx].tapped;
                last_click_idx_ = -1;
                dragged_idx_    = -1;
            } else {
                last_click_idx_ = idx;
                dbl_click_clock_.restart();
                dragged_idx_ = idx;
                drag_offset_ = p - state_.battlefield[idx].position;
            }
        } else {
            last_click_idx_ = -1;
        }
    }
}

void PlaymatWindow::onMouseMove(sf::Vector2f p)
{
    if (dragged_idx_ >= 0 &&
        dragged_idx_ < static_cast<int>(state_.battlefield.size()))
        state_.battlefield[dragged_idx_].position = p - drag_offset_;
}

void PlaymatWindow::onMouseRelease()
{
    dragged_idx_ = -1;
}

void PlaymatWindow::applyContextAction(int item)
{
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

void PlaymatWindow::handleEvent(const sf::Event& e)
{
    if (const auto* mb = e.getIf<sf::Event::MouseButtonPressed>()) {
        auto p = window.mapPixelToCoords(mb->position);
        onMousePress(p, mb->button);
    } else if (const auto* mm = e.getIf<sf::Event::MouseMoved>()) {
        auto p = window.mapPixelToCoords(mm->position);
        onMouseMove(p);
    } else if (e.is<sf::Event::MouseButtonReleased>()) {
        onMouseRelease();
    }
}

void PlaymatWindow::render()
{
    if (!window.isOpen()) return;

    window.clear(sf::Color(34, 85, 34));   // felt green

    const sf::Font* fp = font_loaded_ ? &font_ : nullptr;

    if (fp) {
        sf::Text label(*fp, "PLAYMAT — share this window on stream", 11);
        label.setFillColor(sf::Color(180, 220, 180, 160));
        label.setPosition({6.f, 4.f});
        window.draw(label);
    }

    for (const auto& card : state_.battlefield)
        drawCard(window, card, fp);

    ctx_menu_.draw(window, fp);

    window.display();
}
