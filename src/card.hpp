#pragma once
#include <string>
#include <SFML/Graphics.hpp>

constexpr float CARD_W = 100.f;
constexpr float CARD_H = 140.f;

enum class Zone    { DECK, HAND, BATTLEFIELD, GRAVEYARD, EXILE, COMMAND_ZONE };
enum class DeckPos { TOP, BOTTOM };

class Card {
public:
    // -- Data --------------------------------------------------------------
    std::string  name;
    bool         is_commander = false; // identity field - never cleared by resetState
    bool         is_token     = false; // tokens are removed instead of going to GY/exile
    bool         tapped     = false;
    bool         face_down  = false;
    int          counters   = 0;
    sf::Vector2f position;          // center of the card in world space
    bool         selected   = false;
    sf::Texture* art_texture  = nullptr;
    sf::Texture* back_texture = nullptr;
    float        scale      = 1.0f;

    // Animation
    bool         is_animating          = false;
    sf::Vector2f target_position;
    float        anim_timer            = 0.f;
    sf::Vector2i start_desktop_pos;
    sf::Vector2i end_desktop_pos;
    bool         is_flying_cross_window = false;
    float        rotation              = 0.f;

    // -- Methods -----------------------------------------------------------

    // Strips all transient state (tapped, counters, animations, etc.)
    // leaving only identity data (name, textures).
    void resetState() {
        tapped = false; face_down = false; counters = 0; selected = false;
        scale = 1.0f; rotation = 0.f;
        is_animating = false; is_flying_cross_window = false; anim_timer = 0.f;
        position = {}; target_position = {};
        start_desktop_pos = {}; end_desktop_pos = {};
    }

    // Draw the card centered at `center` (ignores this->position).
    void draw(sf::RenderTarget& target, const sf::Font* font,
              sf::Vector2f center) const;

    // Draw the card at this->position.
    void draw(sf::RenderTarget& target, const sf::Font* font) const;

    // Hit test — accounts for scale and 90° tap rotation.
    bool contains(sf::Vector2f point) const;
};


