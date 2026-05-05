#include "game_state.hpp"
#include "card_requester.hpp"
#include <fstream>
#include <algorithm>
#include <cctype>

bool GameState::loadDeck(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        // ... (existing parsing logic)
        size_t s = line.find_first_not_of(" \t\r\n");
        if (s == std::string::npos) continue;
        line = line.substr(s);

        if (line.size() >= 2 && line[0] == '/' && line[1] == '/') continue;
        if (line[0] == '#') continue;

        size_t i = 0;
        while (i < line.size() && std::isdigit(static_cast<unsigned char>(line[i])))
            ++i;
        if (i == 0) continue;
        int qty = std::stoi(line.substr(0, i));

        if (i < line.size() && (line[i] == 'x' || line[i] == 'X')) ++i;
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;

        std::string name = line.substr(i);
        size_t e = name.find_last_not_of(" \t\r\n");
        if (e == std::string::npos) continue;
        name = name.substr(0, e + 1);
        if (name.empty()) continue;

        CardArt art = CardRequester::getInstance().getArt(name);

        for (int j = 0; j < qty; ++j) {
            Card c;
            c.name = name;
            c.art_texture = art.front;
            c.back_texture = art.back;
            deck.push_back(c);
        }
    }
    return !deck.empty();
}

void GameState::drawCard()
{
    if (deck.empty()) return;
    hand.push_back(deck.back());
    deck.pop_back();
}

void GameState::shuffleDeck()
{
    std::shuffle(deck.begin(), deck.end(), rng_);
}

void GameState::playCard(int hand_idx, sf::Vector2f pos)
{
    if (hand_idx < 0 || hand_idx >= static_cast<int>(hand.size())) return;
    Card c        = hand[hand_idx];
    c.position    = pos;
    c.selected    = false;
    c.tapped      = false;
    battlefield.push_back(c);
    hand.erase(hand.begin() + hand_idx);
}

void GameState::sendToGraveyard(int bf_idx)
{
    if (bf_idx < 0 || bf_idx >= static_cast<int>(battlefield.size())) return;
    Card c     = battlefield[bf_idx];
    c.tapped   = false;
    c.selected = false;
    graveyard.push_back(c);
    battlefield.erase(battlefield.begin() + bf_idx);
}

void GameState::sendToExile(int bf_idx)
{
    if (bf_idx < 0 || bf_idx >= static_cast<int>(battlefield.size())) return;
    Card c     = battlefield[bf_idx];
    c.tapped   = false;
    c.selected = false;
    exile.push_back(c);
    battlefield.erase(battlefield.begin() + bf_idx);
}

int GameState::rollDice(int sides)
{
    std::uniform_int_distribution<int> dist(1, sides);
    dice_result = dist(rng_);
    return dice_result;
}
