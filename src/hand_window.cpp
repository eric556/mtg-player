#include "hand_window.hpp"
#include "card.hpp"
#include <algorithm>
#include <cstdio>
#include <iostream>

// CLAY_IMPLEMENTATION is now in hand_window_layout.cpp
#include <clay.h>

// ── Virtual coordinate space ───────────────────────────────────────────────
static constexpr float WIN_W = 900.f;
static constexpr float WIN_H = 720.f;

// Move card piles down to avoid being covered by the Clay control bar
static constexpr float DECK_CX  =  65.f, DECK_CY  = 160.f;
static constexpr float GY_CX    = 835.f, GY_CY    = 160.f;
static constexpr float EXILE_CX = 720.f, EXILE_CY = 160.f;
static constexpr float HAND_CY = 590.f;

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

static const sf::Font* g_measureFont = nullptr;

static Clay_Dimensions MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
    if (!g_measureFont) return { 0, 0 };
    sf::Text sfText(*g_measureFont, std::string(text.chars, text.length), config->fontSize);
    auto bounds = sfText.getLocalBounds();
    return { bounds.size.x, (float)config->fontSize };
}

HandWindow::HandWindow(GameState& gs) : state_(gs) {
    window.create(sf::VideoMode({static_cast<unsigned>(WIN_W), static_cast<unsigned>(WIN_H)}),
                  "MTG Sim — Hand & Controls  [private]");
    window.setFramerateLimit(60);
    updateView(window, WIN_W, WIN_H);
    font_loaded_ = tryLoadFont(font_);
    if (font_loaded_) g_measureFont = &font_;

    clay_renderer_ = std::make_unique<ClaySFMLRenderer>(window, font_);
    setupClay();

    deck_rect_  = sf::FloatRect({DECK_CX  - CARD_W/2.f, DECK_CY  - CARD_H/2.f}, {CARD_W, CARD_H});
    gy_rect_    = sf::FloatRect({GY_CX    - CARD_W/2.f, GY_CY    - CARD_H/2.f}, {CARD_W, CARD_H});
    exile_rect_ = sf::FloatRect({EXILE_CX - CARD_W/2.f, EXILE_CY - CARD_H/2.f}, {CARD_W, CARD_H});
    pile_viewer_.overlay = sf::FloatRect({80.f, 90.f}, {740.f, 540.f});
}

void HandWindow::setupClay() {
    uint64_t clayMemorySize = Clay_MinMemorySize();
    Clay_Arena clayMemory = Clay_CreateArenaWithCapacityAndMemory(clayMemorySize, malloc(clayMemorySize));
    
    Clay_ErrorHandler errorHandler = {
        .errorHandlerFunction = [](Clay_ErrorData errorData) {
            std::cerr << "Clay Error: " << errorData.errorText.chars << std::endl;
        }
    };

    Clay_Initialize(clayMemory, { WIN_W, WIN_H }, errorHandler);
    Clay_SetMeasureTextFunction(MeasureText, nullptr);
}

// createClayLayout() is now in hand_window_layout.cpp

sf::Vector2f HandWindow::handCardCenter(int idx) const {
    int   count   = static_cast<int>(state_.hand.size());
    if (count == 0) return {WIN_W / 2.f, HAND_CY};
    float slot_w  = std::min(CARD_W + 10.f, (WIN_W - 40.f) / static_cast<float>(count));
    float total_w = static_cast<float>(count) * slot_w;
    float start_x = (WIN_W - total_w) / 2.f + slot_w / 2.f;
    return {start_x + idx * slot_w, HAND_CY};
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
    if (pile_viewer_.visible) { 
        if (pile_viewer_.handleClick(p)) return; 
    }
    
    if (deck_rect_.contains(p))  { state_.drawCard(); return; }
    if (gy_rect_.contains(p))    { pile_viewer_.show("GRAVEYARD", state_.graveyard); return; }
    if (exile_rect_.contains(p)) { pile_viewer_.show("EXILE", state_.exile); return; }

    int idx = handCardAt(p);
    if (idx >= 0) {
        if (selected_hand_idx_ == idx) {
            state_.hand[idx].selected = false;
            selected_hand_idx_ = -1;
        } else {
            if (selected_hand_idx_ >= 0 && selected_hand_idx_ < static_cast<int>(state_.hand.size()))
                state_.hand[selected_hand_idx_].selected = false;
            selected_hand_idx_ = idx;
            state_.hand[idx].selected = true;
        }
        return;
    }
}

void HandWindow::handleEvent(const sf::Event& e) {
    if (const auto* mb = e.getIf<sf::Event::MouseButtonPressed>()) {
        auto p = window.mapPixelToCoords(mb->position);
        Clay_SetPointerState({p.x, p.y}, mb->button == sf::Mouse::Button::Left);
        onMousePress(p);
    } else if (const auto* mm = e.getIf<sf::Event::MouseMoved>()) {
        auto p = window.mapPixelToCoords(mm->position);
        Clay_SetPointerState({p.x, p.y}, sf::Mouse::isButtonPressed(sf::Mouse::Button::Left));
    } else if (const auto* mbr = e.getIf<sf::Event::MouseButtonReleased>()) {
        auto p = window.mapPixelToCoords(mbr->position);
        Clay_SetPointerState({p.x, p.y}, false);
    } else if (e.is<sf::Event::Resized>()) {
        updateView(window, WIN_W, WIN_H);
        Clay_SetLayoutDimensions({WIN_W, WIN_H});
    }
}

void HandWindow::drawPileStack(sf::Vector2f center, int count, const std::string& label, sf::Color back_col) {
    if (count > 0) {
        const int shadow_n = std::min(count - 1, 4);
        sf::RectangleShape shadow({CARD_W, CARD_H});
        shadow.setOrigin({CARD_W / 2.f, CARD_H / 2.f});
        for (int s = shadow_n; s >= 1; --s) {
            shadow.setPosition({center.x - s * 2.f, center.y - s * 2.f});
            sf::Color sc = back_col; sc.a = 160;
            shadow.setFillColor(sc);
            window.draw(shadow);
        }
    }
    sf::RectangleShape top({CARD_W, CARD_H});
    top.setOrigin({CARD_W / 2.f, CARD_H / 2.f});
    top.setPosition(center);
    top.setFillColor(count > 0 ? back_col : sf::Color(50, 50, 50));
    top.setOutlineColor(sf::Color(120, 120, 160));
    top.setOutlineThickness(1.5f);
    window.draw(top);
    if (!font_loaded_) return;
    sf::Text cnt(font_, std::to_string(count), 14);
    cnt.setFillColor(sf::Color::White);
    sf::FloatRect lb = cnt.getLocalBounds();
    cnt.setOrigin({lb.position.x + lb.size.x / 2.f, lb.position.y + lb.size.y / 2.f});
    cnt.setPosition(center);
    window.draw(cnt);
}

void HandWindow::render() {
    if (!window.isOpen()) return;
    window.clear(sf::Color(25, 25, 35));
    const sf::Font* fp = font_loaded_ ? &font_ : nullptr;
    drawPileStack({DECK_CX, DECK_CY}, static_cast<int>(state_.deck.size()), "DECK", sf::Color(30, 30, 110));
    drawPileStack({GY_CX, GY_CY}, static_cast<int>(state_.graveyard.size()), "GRAVEYARD", sf::Color(80, 40, 40));
    drawPileStack({EXILE_CX, EXILE_CY}, static_cast<int>(state_.exile.size()), "EXILE", sf::Color(80, 60, 20));
    for (int i = 0; i < static_cast<int>(state_.hand.size()); ++i)
        drawCard(window, state_.hand[i], fp, handCardCenter(i));
    
    createClayLayout();
    pile_viewer_.draw(window, fp);
    window.display();
}
