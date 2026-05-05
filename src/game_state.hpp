#pragma once
#include "card.hpp"
#include <vector>
#include <string>
#include <random>

struct GameState {
    std::vector<Card> deck;
    std::vector<Card> hand;
    std::vector<Card> battlefield;
    std::vector<Card> graveyard;
    std::vector<Card> exile;

    int life_total  = 20;
    int dice_result = 0;

    // Returns false if the file couldn't be opened or contained no cards.
    bool loadDeck(const std::string& path);

    void drawCard();                                // top of deck → hand
    void shuffleDeck();
    void playCard(int hand_idx, sf::Vector2f pos);  // hand → battlefield
    void sendToGraveyard(int bf_idx);               // battlefield → graveyard
    void sendToExile(int bf_idx);                   // battlefield → exile
    int  rollDice(int sides);                       // sets dice_result and returns it

    void resetAll();                                // return every card to deck and reshuffle

    // Move a card to the bottom of the deck (deck[0], drawn last).
    void battlefieldToBottomOfDeck(int bf_idx);
    void graveyardToBottomOfDeck(int gy_idx);
    void exileToBottomOfDeck(int exile_idx);

    // Play directly from a non-hand zone onto the battlefield.
    void playFromGraveyard(int gy_idx, sf::Vector2f pos);
    void playFromExile(int exile_idx, sf::Vector2f pos);

private:
    std::mt19937 rng_{std::random_device{}()};
};
