#include "ui.hpp"
#include "window_utils.hpp"
#include <algorithm>
#include <cmath>
#include <random>

// -- Button -----------------------------------------------------------------

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
        text.setOrigin({std::round(lb.position.x + lb.size.x / 2.f),
                        std::round(lb.position.y + lb.size.y / 2.f)});
        text.setPosition({std::round(bounds.position.x + bounds.size.x / 2.f),
                          std::round(bounds.position.y + bounds.size.y / 2.f)});
        target.draw(text);
    }
}

// -- ContextMenu -------------------------------------------------------------

static constexpr float ITEM_H = 23.f;
static constexpr float MENU_W = 200.f;

void ContextMenu::show(sf::Vector2f p, int idx, const std::vector<std::string>& options, sf::Vector2f window_size)
{
    items = options;
    target_idx = idx;
    visible = true;
    
    float total_h = items.size() * ITEM_H;
    pos = fitToWindow(p, {MENU_W, total_h}, window_size);
}

int ContextMenu::hitTest(sf::Vector2f p) const
{
    if (!visible) return -1;
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        sf::FloatRect row({pos.x, pos.y + i * ITEM_H}, {MENU_W, ITEM_H});
        if (row.contains(p)) return i;
    }
    return -1;
}

void ContextMenu::draw(sf::RenderTarget& target, const sf::Font* font) const
{
    if (!visible || !font) return;
    const float total_h = items.size() * ITEM_H;
    sf::RectangleShape bg({MENU_W, total_h});
    bg.setPosition(pos);
    bg.setFillColor(sf::Color(30, 30, 30, 235));
    bg.setOutlineColor(sf::Color(180, 180, 180));
    bg.setOutlineThickness(1.f);
    target.draw(bg);
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        sf::Text t(*font, items[i], 13);
        t.setFillColor(sf::Color(230, 230, 230));
        t.setPosition({std::round(pos.x + 8.f), std::round(pos.y + i * ITEM_H + 4.f)});
        target.draw(t);
    }
}

// -- PileViewer -------------------------------------------------------------

// All destination zones in order, with labels and colours.
struct ZoneInfo { Zone zone; DeckPos deck_pos; const char* label; sf::Color color; };
static const ZoneInfo ALL_ZONES[] = {
    { Zone::HAND,         DeckPos::TOP,    "[Hand]",    {50,  50, 150} },
    { Zone::BATTLEFIELD,  DeckPos::TOP,    "[Play]",    {150, 50,  50} },
    { Zone::GRAVEYARD,    DeckPos::TOP,    "[Grave]",   {80,  30,  30} },
    { Zone::EXILE,        DeckPos::TOP,    "[Exile]",   {120, 80,   0} },
    { Zone::COMMAND_ZONE, DeckPos::TOP,    "[Cmd]",     {80,  40, 100} },
    { Zone::DECK,         DeckPos::TOP,    "[DeckT]",   {30,  80,  60} },
    { Zone::DECK,         DeckPos::BOTTOM, "[DeckB]",   {20,  60,  40} },
};

// Max 6 buttons shown at once (7 zones minus the current one, DECK counts as 1).
// 6 × (46 + 3) − 3 = 291 px — fits comfortably in btn_x = OW − 305.
static constexpr float BTN_W   = 46.f;
static constexpr float BTN_H   = 15.f;
static constexpr float BTN_PAD =  3.f;

// Build the list of action buttons for the current zone, excluding self.
std::vector<PileViewer::ActionBtn>
PileViewer::actionButtons(float btn_x, float row_y) const
{
    std::vector<ActionBtn> btns;
    float x = btn_x;
    for (const auto& zi : ALL_ZONES) {
        // Skip if this destination is the same as where we're already viewing
        // (allow both DECK^ and DECKv to show when current_zone != DECK)
        bool is_self = (zi.zone == current_zone)
                    && !(zi.zone == Zone::DECK); // always allow deck dirs if not viewing deck
        if (current_zone == Zone::DECK && zi.zone == Zone::DECK) is_self = true;
        if (is_self) continue;
        btns.push_back({ sf::FloatRect({x, row_y + 1.f}, {BTN_W, BTN_H}),
                         zi.zone, zi.deck_pos });
        x += BTN_W + BTN_PAD;
    }
    return btns;
}

void PileViewer::show(const std::string& t, const std::vector<Card>& pile, Zone z)
{
    title = t; current_zone = z;
    entries.clear();
    for (int i = 0; i < (int)pile.size(); ++i) {
        entries.push_back({ &pile[i], i });
    }

    if (z == Zone::DECK) {
        static std::mt19937 rng{ std::random_device{}() };
        std::shuffle(entries.begin(), entries.end(), rng);
    }

    scroll_offset = 0.f; hovered_idx = -1; visible = true;
}

void PileViewer::hide() { visible = false; }

PileAction PileViewer::handleClick(sf::Vector2f p)
{
    if (!visible) return {};

    if (overlay.contains(p) && hovered_idx >= 0 && hovered_idx < (int)entries.size()) {
        float list_top = overlay.position.y + 40.f;
        float row_y    = list_top + (hovered_idx * 18.f) - scroll_offset;
        float btn_x    = overlay.position.x + overlay.size.x - 305.f;

        for (const auto& btn : actionButtons(btn_x, row_y)) {
            if (btn.rect.contains(p)) {
                visible = false;
                return { true, entries[hovered_idx].original_idx, current_zone, btn.to, btn.deck_pos };
            }
        }
    }

    // Clicked outside any button -> close
    visible = false;
    return {};
}

void PileViewer::handleScroll(float delta)
{
    if (!visible) return;
    scroll_offset -= delta * 25.f;
}

void PileViewer::handleMouseMove(sf::Vector2f p)
{
    if (!visible || !overlay.contains(p)) { hovered_idx = -1; return; }
    float rel = p.y - (overlay.position.y + 40.f) + scroll_offset;
    int idx = static_cast<int>(rel / 18.f);
    hovered_idx = (idx >= 0 && idx < (int)entries.size()) ? idx : -1;
}

void PileViewer::drawScrollbar(sf::RenderTarget& target, float total_h) const
{
    const float view_h = overlay.size.y - 60.f;
    if (total_h <= view_h) return;
    float track_y = overlay.position.y + 40.f;
    float thumb_h = view_h * (view_h / total_h);
    float thumb_y = track_y + (scroll_offset / total_h) * view_h;

    sf::RectangleShape track({6.f, view_h});
    track.setPosition({overlay.position.x + overlay.size.x - 10.f, track_y});
    track.setFillColor(sf::Color(40, 40, 40));
    target.draw(track);

    sf::RectangleShape thumb({6.f, thumb_h});
    thumb.setPosition({overlay.position.x + overlay.size.x - 10.f, thumb_y});
    thumb.setFillColor(sf::Color(120, 120, 120));
    target.draw(thumb);
}

void PileViewer::draw(sf::RenderTarget& target, const sf::Font* font) const
{
    if (!visible || !font) return;

    const float OX = overlay.position.x, OY = overlay.position.y;
    const float OW = overlay.size.x,     OH = overlay.size.y;

    // Background
    sf::RectangleShape bg({OW, OH});
    bg.setPosition({OX, OY});
    bg.setFillColor(sf::Color(20, 20, 25, 245));
    bg.setOutlineColor(sf::Color(180, 180, 180));
    bg.setOutlineThickness(2.f);
    target.draw(bg);

    // Title
    sf::Text ttl(*font, title + " (" + std::to_string(entries.size()) + ")", 16);
    ttl.setFillColor(sf::Color(230, 220, 180));
    ttl.setPosition({std::round(OX + 12.f), std::round(OY + 8.f)});
    target.draw(ttl);

    float list_top = OY + 40.f;
    float list_bot = OY + OH - 30.f;
    float total_content_h = entries.size() * 18.f;

    float max_scroll = std::max(0.f, total_content_h - (list_bot - list_top));
    const_cast<PileViewer*>(this)->scroll_offset = std::clamp(scroll_offset, 0.f, max_scroll);

    for (int i = 0; i < (int)entries.size(); ++i) {
        float y = list_top + (i * 18.f) - scroll_offset;
        if (y < list_top - 18.f || y > list_bot) continue;

        bool hov = (i == hovered_idx);
        sf::Text row(*font, std::to_string(i + 1) + ". " + entries[i].card->name, 13);
        row.setFillColor(hov ? sf::Color::Cyan : sf::Color(210, 210, 200));
        row.setPosition({std::round(OX + 15.f), std::round(y)});
        target.draw(row);

        if (hov) {
            float btn_x = OX + OW - 305.f;
            auto  btns  = actionButtons(btn_x, y);
            int   bi    = 0;
            for (const auto& btn : btns) {
                const ZoneInfo& zi = ALL_ZONES[bi]; // parallel indexing
                // Find matching zone info for label/color
                const ZoneInfo* zi_ptr = nullptr;
                int src_i = 0;
                for (const auto& z : ALL_ZONES) {
                    bool skip = (z.zone == current_zone) && !(z.zone == Zone::DECK);
                    if (current_zone == Zone::DECK && z.zone == Zone::DECK) skip = true;
                    if (!skip) {
                        if (src_i == bi) { zi_ptr = &z; break; }
                        ++src_i;
                    }
                }
                if (!zi_ptr) { ++bi; continue; }

                sf::RectangleShape b(btn.rect.size);
                b.setPosition(btn.rect.position);
                b.setFillColor(zi_ptr->color);
                b.setOutlineColor(sf::Color(180,180,180));
                b.setOutlineThickness(0.5f);
                target.draw(b);

                sf::Text lbl(*font, zi_ptr->label, 9);
                lbl.setPosition({std::round(btn.rect.position.x + 2.f),
                                 std::round(btn.rect.position.y + 2.f)});
                target.draw(lbl);
                ++bi;
            }
        }
    }

    drawScrollbar(target, total_content_h);

    sf::Text hint(*font, "Hover a card for zone options  -  Click elsewhere to close", 11);
    hint.setFillColor(sf::Color(120, 120, 120));
    hint.setPosition({std::round(OX + 15.f), std::round(OY + OH - 20.f)});
    target.draw(hint);

    // Card preview
    if (hovered_idx >= 0 && hovered_idx < (int)entries.size()) {
        const Card* c = entries[hovered_idx].card;
        constexpr float PS = 2.5f;
        float pw = CARD_W * PS, ph = CARD_H * PS;
        sf::Vector2f window_size = target.getView().getSize();

        // Try right side first, then flip to left if off-screen
        sf::Vector2f pp = { OX + OW + 10.f, OY };
        if (pp.x + pw > window_size.x) pp.x = OX - pw - 10.f;
        
        // Final clamp to window bounds using utility
        pp = fitToWindow(pp, {pw, ph}, window_size);

        Card preview = *c;
        preview.position  = pp + sf::Vector2f(pw / 2.f, ph / 2.f);
        preview.scale     = PS;
        preview.tapped    = false;
        preview.face_down = false;
        preview.draw(target, font);
    }
}
