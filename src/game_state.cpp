#include <SFML/Graphics/RenderWindow.hpp>
#include "game_state.hpp"
#include "card_requester.hpp"
#include <fstream>
#include <algorithm>
#include <cctype>

// ── Deck loading ───────────────────────────────────────────────────────────

bool GameState::loadDeck(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        size_t s = line.find_first_not_of(" \t\r\n");
        if (s == std::string::npos) continue;
        line = line.substr(s);
        if (line.size() >= 2 && line[0] == '/' && line[1] == '/') continue;
        if (line[0] == '#') continue;

        size_t i = 0;
        while (i < line.size() && std::isdigit(static_cast<unsigned char>(line[i]))) ++i;
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
            c.name         = name;
            c.art_texture  = art.front;
            c.back_texture = art.back;

            // First card in the file becomes the commander when --commander is set.
            if (commander_mode && deck.empty() && command_zone.empty() && j == 0) {
                c.is_commander = true;
                command_zone.push_back(c);
            } else {
                deck.push_back(c);
            }
        }
    }
    return !deck.empty() || !command_zone.empty();
}

// ── Core deck operations ────────────────────────────────────────────────────

void GameState::shuffleDeck()
{
    std::shuffle(deck.begin(), deck.end(), rng_);
    // Shuffle is not undoable — clear the redo stack so players can't redo past it.
    history.clearRedo();
}

void GameState::drawCard()
{
    if (deck.empty()) return;
    Card c = deck.back();
    c.position    = {65.f, 160.f};  // deck pile position in HandWindow
    c.is_animating = true;
    c.anim_timer   = 0.f;
    hand.push_back(c);
    deck.pop_back();
}

// ── Play from hand (keeps cross-window animation) ──────────────────────────

void GameState::playCard(int hand_idx, sf::Vector2f pos, sf::Vector2f start_pos)
{
    if (hand_idx < 0 || hand_idx >= static_cast<int>(hand.size())) return;
    Card c        = hand[hand_idx];
    c.target_position = pos;
    c.position    = start_pos;
    c.selected    = false;
    c.tapped      = false;
    c.scale       = 1.0f;
    c.is_animating = false;
    c.anim_timer   = 0.f;

    if (hand_window_ptr && playmat_window_ptr) {
        c.start_desktop_pos = hand_window_ptr->getPosition() + hand_window_ptr->mapCoordsToPixel(start_pos);
        sf::Vector2u psz = playmat_window_ptr->getSize();
        c.end_desktop_pos   = playmat_window_ptr->getPosition() + sf::Vector2i(psz.x / 2, psz.y / 2);
        c.is_flying_cross_window = true;
    } else {
        c.is_animating = true;
    }

    battlefield.push_back(c);
    hand.erase(hand.begin() + hand_idx);
}

sf::Vector2i GameState::zoneDesktopCenter(Zone z) const {
    auto toDesktop = [](sf::RenderWindow* win, sf::Vector2f local) -> sf::Vector2i {
        if (!win) return {0, 0};
        return win->getPosition() + sf::Vector2i(win->mapCoordsToPixel(local));
    };
    switch (z) {
        case Zone::DECK:         return toDesktop(hand_window_ptr,    pos_zone_deck);
        case Zone::HAND:         return toDesktop(hand_window_ptr,    pos_zone_hand);
        case Zone::BATTLEFIELD:  return toDesktop(playmat_window_ptr, pos_zone_bf);
        case Zone::GRAVEYARD:    return toDesktop(playmat_window_ptr, pos_zone_gy);
        case Zone::EXILE:        return toDesktop(playmat_window_ptr, pos_zone_exile);
        case Zone::COMMAND_ZONE: return toDesktop(playmat_window_ptr, pos_zone_cmd);
        default:                 return {0, 0};
    }
}

Card* GameState::lastInZone(Zone z, DeckPos dp) {
    auto& v = zoneVec(z);
    if (v.empty()) return nullptr;
    if (z == Zone::DECK && dp == DeckPos::BOTTOM) return &v.front();
    return &v.back();
}

std::vector<Card>& GameState::zoneVec(Zone z)
{
    switch (z) {
        case Zone::DECK:         return deck;
        case Zone::HAND:         return hand;
        case Zone::BATTLEFIELD:  return battlefield;
        case Zone::GRAVEYARD:    return graveyard;
        case Zone::EXILE:        return exile;
        case Zone::COMMAND_ZONE: return command_zone;
    }
    return deck; // unreachable
}

void GameState::createToken(const std::string& name)
{
    Card c;
    c.name       = name;
    c.is_token   = true;
    // Tokens have no Scryfall art — the card renderer will show the name as text
    // on a plain card face.
    c.art_texture  = nullptr;
    c.back_texture = nullptr;

    // Place the token near the center of the battlefield, offset slightly
    // so multiple tokens don't perfectly overlap.
    float off = static_cast<float>(battlefield.size()) * 115.f;
    c.position = {220.f + std::fmod(off, 820.f),
                  420.f + (static_cast<int>(off / 820.f) % 2 == 0 ? 0.f : 100.f)};

    battlefield.push_back(c);
}

void GameState::moveCard(Zone from, int idx, Zone to, DeckPos deck_pos)
{
    auto& src = zoneVec(from);
    if (idx < 0 || idx >= static_cast<int>(src.size())) return;

    Card c = src[idx];

    // Tokens are removed from the game (not placed in GY/exile/deck/hand).
    if (c.is_token && (to == Zone::GRAVEYARD || to == Zone::EXILE ||
                       to == Zone::DECK       || to == Zone::HAND)) {
        src.erase(src.begin() + idx);
        return;
    }

    src.erase(src.begin() + idx);

    // Commander tax persists across all zone transitions for commander cards.
    int saved_counters = c.is_commander ? c.counters : 0;
    c.resetState();
    c.counters = saved_counters;

    auto& dst = zoneVec(to);
    if (to == Zone::DECK && deck_pos == DeckPos::BOTTOM)
        dst.insert(dst.begin(), c);
    else
        dst.push_back(c);
}

int GameState::rollDice(int sides)
{
    std::uniform_int_distribution<int> dist(1, sides);
    dice_result = dist(rng_);
    return dice_result;
}

void GameState::resetAll()
{
    history.clearAll();
    std::vector<Card> all;
    auto collect = [&](std::vector<Card>& zone) {
        for (auto& c : zone) { c.resetState(); all.push_back(c); }
        zone.clear();
    };

    collect(hand);
    collect(battlefield);
    collect(graveyard);
    collect(exile);
    collect(command_zone);

    for (auto& c : all) {
        if (c.is_token) continue;  // tokens are removed from the game on reset
        if (commander_mode && c.is_commander)
            command_zone.push_back(c);  // commander returns to command zone
        else
            deck.push_back(c);
    }
    shuffleDeck();
}
