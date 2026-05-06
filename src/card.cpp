#include "card.hpp"
#include <sstream>
#include <vector>
#include <cmath>

// -- Internal drawing implementation ----------------------------------------
// All geometry is in card-local space (origin = card center).
// A single sf::Transform handles position, scale, and tap/custom rotation,
// so counter badges and text automatically rotate with the card body.

static void drawCardImpl(sf::RenderTarget& target, const Card& card,
                         const sf::Font* font, sf::Vector2f center)
{
    sf::Transform tf;
    tf.translate(center);
    tf.scale({card.scale, card.scale});
    if (card.tapped)  tf.rotate(sf::degrees(90.f));
    tf.rotate(sf::degrees(card.rotation));
    const sf::RenderStates states(tf);

    // -- Card body ---------------------------------------------------------
    sf::RectangleShape body({CARD_W, CARD_H});
    body.setOrigin({CARD_W / 2.f, CARD_H / 2.f});

    if (card.face_down) {
        if (card.back_texture) {
            body.setTexture(card.back_texture);
            body.setFillColor(sf::Color::White);
            body.setOutlineThickness(0.f);
        } else {
            body.setFillColor(sf::Color(30, 30, 120));
            body.setOutlineColor(sf::Color(10, 10, 80));
            body.setOutlineThickness(1.5f);
        }
    } else {
        body.setFillColor(sf::Color(245, 235, 200));
        if (card.selected)
            { body.setOutlineColor(sf::Color::Yellow);          body.setOutlineThickness(3.f);  }
        else if (card.is_commander)
            { body.setOutlineColor(sf::Color(218, 165, 32));    body.setOutlineThickness(2.5f); }
        else
            { body.setOutlineColor(sf::Color(50, 30, 10));      body.setOutlineThickness(1.5f); }
        if (card.art_texture) {
            body.setTexture(card.art_texture);
            body.setFillColor(sf::Color::White);
        }
    }
    target.draw(body, states);

    // ── Card name (face-up, no art, font available) ───────────────────────
    if (!card.face_down && !card.name.empty() && font && !card.art_texture) {
        std::vector<std::string> lines;
        {
            sf::Text measure(*font, "", 11);
            std::istringstream iss(card.name);
            std::string word, line;
            while (iss >> word) {
                std::string candidate = line.empty() ? word : (line + ' ' + word);
                measure.setString(candidate);
                if (!line.empty() &&
                    measure.getLocalBounds().size.x > CARD_W - 10.f) {
                    lines.push_back(line);
                    line = word;
                } else {
                    line = candidate;
                }
            }
            if (!line.empty()) lines.push_back(line);
        }

        const float LINE_H  = 14.f;
        const float total_h = static_cast<float>(lines.size()) * LINE_H;
        const float start_y = -total_h / 2.f;

        for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
            sf::Text t(*font, lines[i], 11);
            t.setFillColor(sf::Color(20, 10, 0));
            sf::FloatRect lb = t.getLocalBounds();
            t.setOrigin({std::round(lb.position.x + lb.size.x / 2.f),
                         std::round(lb.position.y)});
            t.setPosition({0.f, std::round(start_y + i * LINE_H)});
            target.draw(t, states);
        }
    }

    // ── Counter badge (top-right corner) ──────────────────────────────────
    if (card.counters != 0) {
        constexpr float R = 9.f;
        sf::CircleShape badge(R);
        badge.setOrigin({R, R});
        badge.setPosition({CARD_W / 2.f - R - 2.f, -CARD_H / 2.f + R + 2.f});
        badge.setFillColor(sf::Color(40, 120, 40));
        badge.setOutlineColor(sf::Color::White);
        badge.setOutlineThickness(1.f);
        target.draw(badge, states);

        if (font) {
            sf::Text cnt(*font, std::to_string(card.counters), 10);
            cnt.setFillColor(sf::Color::White);
            sf::FloatRect lb = cnt.getLocalBounds();
            cnt.setOrigin({std::round(lb.position.x + lb.size.x / 2.f),
                           std::round(lb.position.y + lb.size.y / 2.f)});
            cnt.setPosition({std::round(CARD_W / 2.f - R - 2.f),
                             std::round(-CARD_H / 2.f + R + 2.f)});
            target.draw(cnt, states);
        }
    }
}

// ── Card member function implementations ───────────────────────────────────

void Card::draw(sf::RenderTarget& target, const sf::Font* font,
                sf::Vector2f center) const
{
    drawCardImpl(target, *this, font, center);
}

void Card::draw(sf::RenderTarget& target, const sf::Font* font) const
{
    drawCardImpl(target, *this, font, position);
}

bool Card::contains(sf::Vector2f point) const
{
    sf::Transform tf;
    tf.translate(position);
    tf.scale({scale, scale});
    if (tapped) tf.rotate(sf::degrees(90.f));
    sf::Vector2f local = tf.getInverse().transformPoint(point);
    return local.x >= -CARD_W / 2.f && local.x <= CARD_W / 2.f &&
           local.y >= -CARD_H / 2.f && local.y <= CARD_H / 2.f;
}
