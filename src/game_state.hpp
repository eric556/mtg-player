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

    int  life_total       = 20;
    int  dice_result      = 0;
    bool commander_mode   = false;
    bool deck_top_visible = false;

    // Zone display centers in their respective window's coordinate space.
    // Updated by each window's reflow() so zoneDesktopCenter() stays accurate.
    sf::Vector2f pos_zone_deck  = {65.f,   160.f};
    sf::Vector2f pos_zone_hand  = {450.f,  590.f};
    sf::Vector2f pos_zone_bf    = {640.f,  400.f};
    sf::Vector2f pos_zone_gy    = {1190.f, 100.f};
    sf::Vector2f pos_zone_exile = {1075.f, 100.f};
    sf::Vector2f pos_zone_cmd   = {65.f,   100.f};

    // Returns the desktop pixel center of where a zone is displayed.
    sf::Vector2i zoneDesktopCenter(Zone z) const;

    // Returns a pointer to the card most recently added to a zone (nullptr if empty).
    // For DECK, respects deck_pos: TOP = back(), BOTTOM = front().
    Card* lastInZone(Zone z, DeckPos dp = DeckPos::TOP);

    bool loadDeck(const std::string& path);

    void shuffleDeck();
    void drawCard();
    void resetAll();

    void playCard(int hand_idx, sf::Vector2f pos, sf::Vector2f start_pos);

    void moveCard(Zone from, int idx, Zone to, DeckPos deck_pos = DeckPos::TOP);

    int rollDice(int sides);

private:
    std::vector<Card>& zoneVec(Zone z);
    std::mt19937 rng_{std::random_device{}()};
};
