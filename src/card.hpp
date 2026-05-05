#pragma once
#include <string>
#include <SFML/Graphics.hpp>

constexpr float CARD_W = 100.f;
constexpr float CARD_H = 140.f;

struct Card {
    std::string  name;
    bool         tapped     = false;
    bool         face_down  = false;
    int          counters   = 0;
    sf::Vector2f position;          // battlefield center position
    bool         selected   = false;
    sf::Texture* art_texture = nullptr;
    // TODO: fetch https://api.scryfall.com/cards/named?fuzzy=<name>&format=image&version=art_crop
    // TODO: cache fetched art in local cache.json keyed by card name
};

// font may be nullptr — text and counter badges are skipped in that case.
void drawCard(sf::RenderTarget& target, const Card& card,
              const sf::Font* font, sf::Vector2f center);

void drawCard(sf::RenderTarget& target, const Card& card,
              const sf::Font* font);

// Hit test — accounts for 90° rotation when tapped.
bool cardContains(const Card& card, sf::Vector2f point);
