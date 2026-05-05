#include "hand_window.hpp"
#include "card.hpp"
#include <algorithm>
#include <cstdio>

// ── Button Implementation ──────────────────────────────────────────────────

void Button::draw(sf::RenderTarget& target, const sf::Font* font) const {
    sf::RectangleShape shape(bounds.size);
    shape.setPosition(bounds.position);
    
    sf::Color fill = enabled ? (hovered ? sf::Color(100, 100, 110) : sf::Color(70, 70, 80)) 
                             : sf::Color(40, 40, 45);
    shape.setFillColor(fill);
    shape.setOutlineColor(sf::Color(120, 120, 130));
    shape.setOutlineThickness(1.f);
    target.draw(shape);

    if (font && !label.empty()) {
        sf::Text text(*font, label, 14);
        text.setFillColor(enabled ? sf::Color::White : sf::Color(100, 100, 100));
        sf::FloatRect lb = text.getLocalBounds();
        text.setOrigin({lb.position.x + lb.size.x / 2.f, lb.position.y + lb.size.y / 2.f});
        text.setPosition(bounds.position + bounds.size / 2.f);
        target.draw(text);
    }
}

// ── HandWindow Implementation ──────────────────────────────────────────────

static void updateView(sf::RenderWindow& win, float vw, float vh) {
    auto ws = win.getSize();
    float winRatio  = static_cast<float>(ws.x) / static_cast<float>(ws.y);
    float virtRatio = vw / vh;
    sf::FloatRect vp;
    if (winRatio > virtRatio) {
        float s = virtRatio / winRatio;
        vp = sf::FloatRect({(1.f - s) / 2.f, 0.f}, {s, 1.f});
    } else {
        float s = winRatio / virtRatio;
        vp = sf::FloatRect({0.f, (1.f - s) / 2.f}, {1.f, s});
    }
    sf::View view(sf::FloatRect({0.f, 0.f}, {vw, vh}));
    view.setViewport(vp);
    win.setView(view);
}

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
                  "MTG Sim — Hand & Controls  [private]", sf::State::Windowed);
    window.setFramerateLimit(60);
    updateView(window, WIN_W, WIN_H);
    font_loaded_ = tryLoadFont(font_);

    // Initial button setup
    btn_draw_.label = "Draw";
    btn_shuffle_.label = "Shuffle";
    btn_play_.label = "Play Card";
    updateButtonLayout();

    pile_viewer_.overlay = sf::FloatRect({80.f, 90.f}, {740.f, 540.f});
}

void HandWindow::updateButtonLayout() {
    float x = 20.f;
    float y = 10.f;
    float h = 35.f;
    
    btn_draw_.bounds    = sf::FloatRect({x, y}, {80.f, h});
    btn_shuffle_.bounds = sf::FloatRect({x + 90.f, y}, {80.f, h});
    btn_play_.bounds    = sf::FloatRect({WIN_W - 140.f, y}, {120.f, h});
}

sf::Vector2f HandWindow::handCardCenter(int idx) const {
    int   count   = static_cast<int>(state_.hand.size());
    if (count == 0) return {WIN_W / 2.f, 590.f};
    float slot_w  = std::min(CARD_W + 10.f, (WIN_W - 40.f) / static_cast<float>(count));
    float total_w = static_cast<float>(count) * slot_w;
    float start_x = (WIN_W - total_w) / 2.f + slot_w / 2.f;
    return {start_x + idx * slot_w, 590.f};
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
    if (pile_viewer_.visible) { if (pile_viewer_.handleClick(p)) return; }

    if (btn_draw_.contains(p))    { state_.drawCard(); return; }
    if (btn_shuffle_.contains(p)) { state_.shuffleDeck(); return; }
    if (btn_play_.contains(p)) {
        if (selected_hand_idx_ >= 0) {
            float off = static_cast<float>(state_.battlefield.size()) * 115.f;
            float px  = 220.f + std::fmod(off, 820.f);
            float py  = 420.f + (static_cast<int>(off / 820.f) % 2 == 0 ? 0.f : 100.f);
            state_.playCard(selected_hand_idx_, {px, py});
            selected_hand_idx_ = -1;
        }
        return;
    }

    // Pile clicks
    auto checkPile = [&](sf::Vector2f center, const char* zone) {
        sf::FloatRect r({center.x - CARD_W/2.f, center.y - CARD_H/2.f}, {CARD_W, CARD_H});
        return r.contains(p);
    };

    if (checkPile({65.f, 160.f}, "DECK")) { state_.drawCard(); return; }
    if (checkPile({835.f, 160.f}, "GY"))   { pile_viewer_.show("GRAVEYARD", state_.graveyard); return; }
    if (checkPile({720.f, 160.f}, "EXILE")) { pile_viewer_.show("EXILE", state_.exile); return; }

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

void HandWindow::onMouseMove(sf::Vector2f p) {
    btn_draw_.hovered = btn_draw_.contains(p);
    btn_shuffle_.hovered = btn_shuffle_.contains(p);
    btn_play_.hovered = btn_play_.contains(p);
}

void HandWindow::handleEvent(const sf::Event& e) {
    if (const auto* mb = e.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button == sf::Mouse::Button::Left)
            onMousePress(window.mapPixelToCoords(mb->position));
    } else if (const auto* mm = e.getIf<sf::Event::MouseMoved>()) {
        auto p = window.mapPixelToCoords(mm->position);
        onMouseMove(p);
        pile_viewer_.handleMouseMove(p);
    } else if (const auto* mws = e.getIf<sf::Event::MouseWheelScrolled>()) {
        if (mws->wheel == sf::Mouse::Wheel::Vertical) {
            pile_viewer_.handleScroll(mws->delta);
        }
    } else if (e.is<sf::Event::Resized>()) {
        updateView(window, WIN_W, WIN_H);
    }
}

void HandWindow::drawPileStack(sf::RenderTarget& target, sf::Vector2f center, int count, const std::string& label, sf::Color back_col) {
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
    cnt.setOrigin({lb.position.x + lb.size.x / 2.f, lb.position.y + lb.size.y / 2.f});
    cnt.setPosition(center);
    target.draw(cnt);
}

void HandWindow::render() {
    if (!window.isOpen()) return;
    window.clear(sf::Color(25, 25, 35));
    updateView(window, WIN_W, WIN_H);
    
    const sf::Font* fp = font_loaded_ ? &font_ : nullptr;
    
    // UI
    btn_draw_.draw(window, fp);
    btn_shuffle_.draw(window, fp);
    btn_play_.enabled = (selected_hand_idx_ >= 0);
    btn_play_.draw(window, fp);

    // Piles
    drawPileStack(window, {65.f, 160.f}, (int)state_.deck.size(), "DECK", sf::Color(30, 30, 110));
    drawPileStack(window, {835.f, 160.f}, (int)state_.graveyard.size(), "GRAVEYARD", sf::Color(80, 40, 40));
    drawPileStack(window, {720.f, 160.f}, (int)state_.exile.size(), "EXILE", sf::Color(80, 60, 20));

    // Hand
    for (int i = 0; i < (int)state_.hand.size(); ++i)
        drawCard(window, state_.hand[i], fp, handCardCenter(i));
    
    // Preview selected card
    if (selected_hand_idx_ >= 0 && selected_hand_idx_ < (int)state_.hand.size()) {
        Card preview = state_.hand[selected_hand_idx_];
        preview.scale = 2.5f;
        preview.selected = false; // outline is not needed for the preview
        drawCard(window, preview, fp, {WIN_W / 2.f, 350.f});
    }

    if (pile_viewer_.visible) pile_viewer_.draw(window, fp);
    
    window.display();
}
