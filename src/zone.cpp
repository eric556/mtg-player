#include "zone.hpp"
#include <algorithm>

void PileViewer::show(const std::string& t, const std::vector<Card>& pile)
{
    title = t;
    cards.clear();
    for (const auto& c : pile) cards.push_back(&c);
    scroll_offset = 0.f;
    hovered_idx = -1;
    visible = true;
}

void PileViewer::hide() {
    visible = false;
}

bool PileViewer::handleClick(sf::Vector2f /*p*/)
{
    if (!visible) return false;
    visible = false;
    return true;
}

void PileViewer::handleScroll(float delta) {
    if (!visible) return;
    scroll_offset -= delta * 25.f; // 25px per scroll click
}

void PileViewer::handleMouseMove(sf::Vector2f p) {
    if (!visible || !overlay.contains(p)) {
        hovered_idx = -1;
        return;
    }

    float relative_y = p.y - (overlay.position.y + 40.f) + scroll_offset;
    int idx = static_cast<int>(relative_y / 18.f);
    if (idx >= 0 && idx < static_cast<int>(cards.size())) {
        hovered_idx = idx;
    } else {
        hovered_idx = -1;
    }
}

void PileViewer::drawScrollbar(sf::RenderTarget& target, float total_height) const {
    const float view_h = overlay.size.y - 60.f;
    if (total_height <= view_h) return;

    float ratio = view_h / total_height;
    float thumb_h = view_h * ratio;
    float track_y = overlay.position.y + 40.f;
    float thumb_y = track_y + (scroll_offset / total_height) * view_h;

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

    sf::RectangleShape bg({OW, OH});
    bg.setPosition({OX, OY});
    bg.setFillColor(sf::Color(20, 20, 25, 240));
    bg.setOutlineColor(sf::Color(180, 180, 180));
    bg.setOutlineThickness(2.f);
    target.draw(bg);

    // Title
    sf::Text ttl(*font, title + " (" + std::to_string(cards.size()) + ")", 16);
    ttl.setFillColor(sf::Color(230, 220, 180));
    ttl.setPosition({OX + 12.f, OY + 8.f});
    target.draw(ttl);

    // Simple clipping: only draw what fits in the box
    float list_top = OY + 40.f;
    float list_bot = OY + OH - 30.f;
    float total_content_h = cards.size() * 18.f;

    // Constrain scroll
    float max_scroll = std::max(0.f, total_content_h - (list_bot - list_top));
    const_cast<PileViewer*>(this)->scroll_offset = std::clamp(scroll_offset, 0.f, max_scroll);

    for (int i = 0; i < (int)cards.size(); ++i) {
        float y = list_top + (i * 18.f) - scroll_offset;
        if (y < list_top - 18.f) continue;
        if (y > list_bot) continue;

        sf::Text row(*font, std::to_string(i + 1) + ". " + cards[i]->name, 13);
        row.setFillColor(i == hovered_idx ? sf::Color::Cyan : sf::Color(210, 210, 200));
        row.setPosition({OX + 15.f, y});
        target.draw(row);
    }

    drawScrollbar(target, total_content_h);

    // Hint
    sf::Text hint(*font, "Scroll to browse • Click to close", 11);
    hint.setFillColor(sf::Color(120, 120, 120));
    hint.setPosition({OX + 15.f, OY + OH - 20.f});
    target.draw(hint);

    // Preview Hovered Card
    if (hovered_idx >= 0 && hovered_idx < (int)cards.size()) {
        const Card* c = cards[hovered_idx];
        float preview_scale = 3.0f; // Big preview!
        float pw = CARD_W * preview_scale;
        float ph = CARD_H * preview_scale;
        
        sf::Vector2f p_pos = { OX + OW + 10.f, OY };
        // Flip to left side if it doesn't fit on right
        if (p_pos.x + pw > target.getView().getSize().x) {
            p_pos.x = OX - pw - 10.f;
        }

        Card preview = *c; // Copy for drawing
        preview.position = p_pos + sf::Vector2f(pw/2.f, ph/2.f);
        preview.scale = preview_scale;
        preview.tapped = false;
        preview.face_down = false; // Always show face-up in preview
        drawCard(target, preview, font);
    }
}
