#include "zone.hpp"

void PileViewer::show(const std::string& t, const std::vector<Card>& pile)
{
    title = t;
    names.clear();
    for (const auto& c : pile) names.push_back(c.name);
    visible = true;
}

bool PileViewer::handleClick(sf::Vector2f /*p*/)
{
    if (!visible) return false;
    visible = false;
    return true;
}

void PileViewer::draw(sf::RenderTarget& target, const sf::Font* font) const
{
    if (!visible || !font) return;

    const float OX = overlay.position.x, OY = overlay.position.y;
    const float OW = overlay.size.x,     OH = overlay.size.y;

    sf::RectangleShape bg({OW, OH});
    bg.setPosition({OX, OY});
    bg.setFillColor(sf::Color(15, 15, 15, 248));
    bg.setOutlineColor(sf::Color(200, 200, 200));
    bg.setOutlineThickness(2.f);
    target.draw(bg);

    {
        sf::Text ttl(*font,
                     title + "  (" + std::to_string(names.size()) + " cards)", 15);
        ttl.setFillColor(sf::Color(230, 220, 180));
        ttl.setPosition({OX + 10.f, OY + 8.f});
        target.draw(ttl);
    }

    float y = OY + 36.f;
    for (int i = 0; i < static_cast<int>(names.size()); ++i) {
        if (y + 16.f > OY + OH - 20.f) {
            sf::Text more(*font,
                          "  … and " + std::to_string(names.size() - i) + " more", 12);
            more.setFillColor(sf::Color(160, 160, 160));
            more.setPosition({OX + 12.f, y});
            target.draw(more);
            break;
        }
        sf::Text row(*font, std::to_string(i + 1) + ".  " + names[i], 13);
        row.setFillColor(sf::Color(215, 215, 200));
        row.setPosition({OX + 12.f, y});
        target.draw(row);
        y += 17.f;
    }

    sf::Text hint(*font, "click anywhere to close", 11);
    hint.setFillColor(sf::Color(110, 110, 110));
    hint.setPosition({OX + OW - 162.f, OY + OH - 18.f});
    target.draw(hint);
}
