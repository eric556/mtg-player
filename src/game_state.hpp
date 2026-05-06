#pragma once
#include "card.hpp"
#include <vector>
#include <string>
#include <random>

namespace sf { class RenderWindow; }

struct GameState {
    sf::RenderWindow* hand_window_ptr    = nullptr;
    sf::RenderWindow* playmat_window_ptr = nullptr;

    std::vector<Card> deck;
    std::vector<Card> hand;
    std::vector<Card> battlefield;
    std::vector<Card> graveyard;
    std::vector<Card> exile;
    std::vector<Card> command_zone;
    std::vector<Card> flying_cards;

    int  life_total      = 20;
    int  dice_result     = 0;
    bool commander_mode  = false;
    bool deck_top_visible = false;

    bool loadDeck(const std::string& path);

    void shuffleDeck();
    void drawCard();
    void resetAll();

    void playCard(int hand_idx, sf::Vector2f pos, sf::Vector2f start_pos);

    void moveCard(Zone from, int idx, Zone to, DeckPos deck_pos = DeckPos::TOP);
    void moveCardAnimated(Zone from, int idx, Zone to, sf::Vector2i start_pix, sf::Vector2i end_pix, DeckPos deck_pos = DeckPos::TOP);

    void updateAnimations(float dt);

    int rollDice(int sides);

private:
    std::vector<Card>& zoneVec(Zone z);
    std::mt19937 rng_{std::random_device{}()};
};
