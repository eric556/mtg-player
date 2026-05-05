#include "hand_window.hpp"
#include "card.hpp"
#include <algorithm>
#include <cstdio>

// ── Virtual coordinate space ───────────────────────────────────────────────
// All positions are in this space; sf::View maps it to whatever the actual
// window size is, letterboxing to preserve aspect ratio.

static constexpr float WIN_W = 900.f;
static constexpr float WIN_H = 720.f;

// Pile centres
static constexpr float DECK_CX  =  65.f, DECK_CY  = 120.f;
static constexpr float GY_CX    = 835.f, GY_CY    = 120.f;
static constexpr float EXILE_CX = 720.f, EXILE_CY = 120.f;

// Hand card row
static constexpr float HAND_CY = 590.f;

// ── Letterbox view helper ──────────────────────────────────────────────────

static void updateView(sf::RenderWindow& win, float vw, float vh)
{
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

// ── Button ─────────────────────────────────────────────────────────────────

void Button::draw(sf::RenderTarget& target, const sf::Font* font,
                  sf::Color fill) const
{
    sf::RectangleShape bg({bounds.size.x, bounds.size.y});
    bg.setPosition(bounds.position);
    bg.setFillColor(enabled ? fill : sf::Color(45, 45, 45));
    bg.setOutlineColor(sf::Color(150, 150, 150));
    bg.setOutlineThickness(1.f);
    target.draw(bg);

    if (font) {
        sf::Text t(*font, label, 12);
        t.setFillColor(enabled ? sf::Color::White : sf::Color(90, 90, 90));
        sf::FloatRect lb = t.getLocalBounds();
        t.setOrigin({lb.position.x + lb.size.x / 2.f,
                     lb.position.y + lb.size.y / 2.f});
        t.setPosition({bounds.position.x + bounds.size.x / 2.f,
                       bounds.position.y + bounds.size.y / 2.f});
        target.draw(t);
    }
}

// ── HandWindow ─────────────────────────────────────────────────────────────

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

static Button makeBtn(float x, float y, float w, float h, const char* lbl)
{
    Button b;
    b.bounds = sf::FloatRect({x, y}, {w, h});
    b.label  = lbl;
    return b;
}

HandWindow::HandWindow(GameState& gs)
    : state_(gs)
{
    window.create(sf::VideoMode({static_cast<unsigned>(WIN_W),
                                 static_cast<unsigned>(WIN_H)}),
                  "MTG Sim — Hand & Controls  [private]");
    window.setFramerateLimit(60);
    updateView(window, WIN_W, WIN_H);

    font_loaded_ = tryLoadFont(font_);
    if (!font_loaded_)
        fprintf(stderr, "[hand] Warning: no system font found.\n");

    // ── Buttons ───────────────────────────────────────────────────────────
    btn_draw_       = makeBtn( 15.f, 248.f,  80.f, 26.f, "Draw");
    btn_shuffle_    = makeBtn(105.f, 248.f,  80.f, 26.f, "Shuffle");
    btn_play_       = makeBtn(358.f, 228.f, 184.f, 32.f, "▶  Play Card");

    btn_life_minus_ = makeBtn(358.f,  58.f,  30.f, 30.f, "−");
    btn_life_plus_  = makeBtn(512.f,  58.f,  30.f, 30.f, "+");

    btn_d6_         = makeBtn(368.f, 138.f,  45.f, 24.f, "d6");
    btn_d8_         = makeBtn(420.f, 138.f,  45.f, 24.f, "d8");
    btn_d20_        = makeBtn(472.f, 138.f,  50.f, 24.f, "d20");

    // ── Pile click regions ────────────────────────────────────────────────
    deck_rect_  = sf::FloatRect({DECK_CX  - CARD_W/2.f, DECK_CY  - CARD_H/2.f}, {CARD_W, CARD_H});
    gy_rect_    = sf::FloatRect({GY_CX    - CARD_W/2.f, GY_CY    - CARD_H/2.f}, {CARD_W, CARD_H});
    exile_rect_ = sf::FloatRect({EXILE_CX - CARD_W/2.f, EXILE_CY - CARD_H/2.f}, {CARD_W, CARD_H});

    // ── Pile viewer overlay — fits inside the 900×720 virtual space ───────
    pile_viewer_.overlay = sf::FloatRect({80.f, 90.f}, {740.f, 540.f});
}

sf::Vector2f HandWindow::handCardCenter(int idx) const
{
    int   count   = static_cast<int>(state_.hand.size());
    if (count == 0) return {WIN_W / 2.f, HAND_CY};
    float slot_w  = std::min(CARD_W + 10.f, (WIN_W - 40.f) / static_cast<float>(count));
    float total_w = static_cast<float>(count) * slot_w;
    float start_x = (WIN_W - total_w) / 2.f + slot_w / 2.f;
    return {start_x + idx * slot_w, HAND_CY};
}

int HandWindow::handCardAt(sf::Vector2f p) const
{
    for (int i = 0; i < static_cast<int>(state_.hand.size()); ++i) {
        sf::Vector2f c = handCardCenter(i);
        sf::FloatRect r({c.x - CARD_W / 2.f, c.y - CARD_H / 2.f}, {CARD_W, CARD_H});
        if (r.contains(p)) return i;
    }
    return -1;
}

void HandWindow::onMousePress(sf::Vector2f p)
{
    if (pile_viewer_.visible) { pile_viewer_.handleClick(p); return; }

    // ── Hand cards ────────────────────────────────────────────────────────
    {
        int idx = handCardAt(p);
        if (idx >= 0) {
            if (selected_hand_idx_ == idx) {
                state_.hand[idx].selected = false;
                selected_hand_idx_ = -1;
            } else {
                if (selected_hand_idx_ >= 0 &&
                    selected_hand_idx_ < static_cast<int>(state_.hand.size()))
                    state_.hand[selected_hand_idx_].selected = false;
                selected_hand_idx_ = idx;
                state_.hand[idx].selected = true;
            }
            return;
        }
    }

    // ── Buttons ───────────────────────────────────────────────────────────
    if (btn_draw_.contains(p))    { state_.drawCard();    return; }
    if (btn_shuffle_.contains(p)) { state_.shuffleDeck(); return; }

    if (btn_play_.contains(p) && btn_play_.enabled && selected_hand_idx_ >= 0) {
        float off = static_cast<float>(state_.battlefield.size()) * 115.f;
        float px  = 220.f + std::fmod(off, 820.f);
        float py  = 420.f + (static_cast<int>(off / 820.f) % 2 == 0 ? 0.f : 100.f);
        state_.playCard(selected_hand_idx_, {px, py});
        selected_hand_idx_ = -1;
        return;
    }

    if (btn_life_minus_.contains(p)) { state_.life_total--; return; }
    if (btn_life_plus_.contains(p))  { state_.life_total++; return; }
    if (btn_d6_.contains(p))         { state_.rollDice(6);  return; }
    if (btn_d8_.contains(p))         { state_.rollDice(8);  return; }
    if (btn_d20_.contains(p))        { state_.rollDice(20); return; }

    // ── Pile clicks ───────────────────────────────────────────────────────
    if (gy_rect_.contains(p) && !state_.graveyard.empty()) {
        pile_viewer_.show("Graveyard", state_.graveyard);
        return;
    }
    if (exile_rect_.contains(p) && !state_.exile.empty()) {
        pile_viewer_.show("Exile", state_.exile);
        return;
    }
}

void HandWindow::handleEvent(const sf::Event& e)
{
    if (const auto* mb = e.getIf<sf::Event::MouseButtonPressed>()) {
        auto p = window.mapPixelToCoords(mb->position);
        onMousePress(p);
    } else if (e.is<sf::Event::Resized>()) {
        updateView(window, WIN_W, WIN_H);
    }
}

// ── Rendering helpers ──────────────────────────────────────────────────────

void HandWindow::drawPileStack(sf::Vector2f center, int count,
                                const std::string& label, sf::Color back_col)
{
    if (count > 0) {
        const int shadow_n = std::min(count - 1, 4);
        sf::RectangleShape shadow({CARD_W, CARD_H});
        shadow.setOrigin({CARD_W / 2.f, CARD_H / 2.f});
        shadow.setOutlineThickness(0.f);
        for (int s = shadow_n; s >= 1; --s) {
            shadow.setPosition({center.x - s * 2.f, center.y - s * 2.f});
            sf::Color sc = back_col;
            sc.a = 160;
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

    {
        sf::Text cnt(font_, std::to_string(count), 14);
        cnt.setFillColor(sf::Color::White);
        sf::FloatRect lb = cnt.getLocalBounds();
        cnt.setOrigin({lb.position.x + lb.size.x / 2.f,
                       lb.position.y + lb.size.y / 2.f});
        cnt.setPosition(center);
        window.draw(cnt);
    }
    {
        sf::Text lbl(font_, label, 11);
        lbl.setFillColor(sf::Color(200, 200, 200));
        sf::FloatRect llb = lbl.getLocalBounds();
        lbl.setOrigin({llb.position.x + llb.size.x / 2.f,
                       llb.position.y + llb.size.y});
        lbl.setPosition({center.x, center.y - CARD_H / 2.f - 4.f});
        window.draw(lbl);
    }
}

static void drawLabel(sf::RenderTarget& t, const sf::Font* f,
                      const std::string& s, float x, float y,
                      unsigned size = 11,
                      sf::Color col = sf::Color(160, 160, 160))
{
    if (!f) return;
    sf::Text txt(*f, s, size);
    txt.setFillColor(col);
    txt.setPosition({x, y});
    t.draw(txt);
}

void HandWindow::render()
{
    if (!window.isOpen()) return;

    window.clear(sf::Color(25, 25, 35));

    const sf::Font* fp = font_loaded_ ? &font_ : nullptr;

    // ── Piles ─────────────────────────────────────────────────────────────
    drawPileStack({DECK_CX, DECK_CY},
                  static_cast<int>(state_.deck.size()),
                  "DECK", sf::Color(30, 30, 110));

    drawPileStack({GY_CX, GY_CY},
                  static_cast<int>(state_.graveyard.size()),
                  "GRAVEYARD", sf::Color(80, 40, 40));
    if (!state_.graveyard.empty())
        drawLabel(window, fp, "(click to view)",
                  GY_CX - 40.f, GY_CY + CARD_H / 2.f + 4.f);

    drawPileStack({EXILE_CX, EXILE_CY},
                  static_cast<int>(state_.exile.size()),
                  "EXILE", sf::Color(80, 60, 20));
    if (!state_.exile.empty())
        drawLabel(window, fp, "(click to view)",
                  EXILE_CX - 36.f, EXILE_CY + CARD_H / 2.f + 4.f);

    // ── Deck buttons ──────────────────────────────────────────────────────
    btn_draw_.draw(window, fp);
    btn_shuffle_.draw(window, fp);

    // ── Life total ────────────────────────────────────────────────────────
    drawLabel(window, fp, "LIFE", 432.f, 36.f, 11);
    if (fp) {
        sf::Text life(*fp, std::to_string(state_.life_total), 40);
        life.setFillColor(state_.life_total <= 5
                          ? sf::Color(220, 60, 60) : sf::Color(220, 220, 220));
        sf::FloatRect lb = life.getLocalBounds();
        life.setOrigin({lb.position.x + lb.size.x / 2.f,
                        lb.position.y + lb.size.y / 2.f});
        life.setPosition({450.f, 78.f});
        window.draw(life);
    }
    btn_life_minus_.draw(window, fp, sf::Color(120, 50, 50));
    btn_life_plus_.draw(window, fp,  sf::Color(50, 110, 50));

    // ── Dice ──────────────────────────────────────────────────────────────
    drawLabel(window, fp, "ROLL DICE", 405.f, 118.f, 11);
    btn_d6_.draw(window, fp,  sf::Color(60, 60, 100));
    btn_d8_.draw(window, fp,  sf::Color(60, 60, 100));
    btn_d20_.draw(window, fp, sf::Color(60, 60, 100));

    if (state_.dice_result > 0 && fp) {
        sf::Text res(*fp, std::to_string(state_.dice_result), 30);
        res.setFillColor(sf::Color(220, 200, 80));
        sf::FloatRect lb = res.getLocalBounds();
        res.setOrigin({lb.position.x + lb.size.x / 2.f,
                       lb.position.y + lb.size.y / 2.f});
        res.setPosition({450.f, 185.f});
        window.draw(res);
    }

    // ── Play button ───────────────────────────────────────────────────────
    btn_play_.enabled = (selected_hand_idx_ >= 0);
    btn_play_.draw(window, fp,
                   btn_play_.enabled ? sf::Color(60, 120, 60) : sf::Color(45, 45, 45));

    // ── Separator ─────────────────────────────────────────────────────────
    {
        sf::RectangleShape sep({WIN_W - 20.f, 1.f});
        sep.setPosition({10.f, 290.f});
        sep.setFillColor(sf::Color(60, 60, 70));
        window.draw(sep);
    }
    drawLabel(window, fp, "HAND", WIN_W / 2.f - 18.f, 298.f, 12,
              sf::Color(180, 180, 140));

    // ── Hand cards ────────────────────────────────────────────────────────
    for (int i = 0; i < static_cast<int>(state_.hand.size()); ++i)
        drawCard(window, state_.hand[i], fp, handCardCenter(i));

    // ── Pile viewer overlay ───────────────────────────────────────────────
    pile_viewer_.draw(window, fp);

    // ── Private watermark ─────────────────────────────────────────────────
    drawLabel(window, fp, "PRIVATE — do not share this window",
              6.f, WIN_H - 18.f, 11, sf::Color(120, 60, 60));

    window.display();
}
