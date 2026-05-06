#include "game_state.hpp"
#include "hand_window.hpp"
#include "playmat_window.hpp"
#include "card_requester.hpp"
#include <iostream>

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: mtg-sim [--commander|-c] <deck.txt> [cache_dir]\n"
                  << "  --commander / -c  First card in deck list becomes the commander\n"
                  << "                    and starts in the command zone.\n"
                  << "  Deck format (one entry per line):\n"
                  << "    4 Lightning Bolt\n"
                  << "    1x Black Lotus\n"
                  << "    // comments are ignored\n";
        return 1;
    }

    bool commander_mode = false;
    const char* deck_path  = nullptr;
    const char* cache_path = nullptr;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--commander" || a == "-c") { commander_mode = true; }
        else if (!deck_path)                  deck_path  = argv[i];
        else if (!cache_path)                 cache_path = argv[i];
    }
    if (!deck_path) {
        std::cerr << "Error: no deck file specified.\n";
        return 1;
    }

    if (cache_path) {
        CardRequester::getInstance().setCacheDirectory(cache_path);
        std::cout << "Using cache directory: " << cache_path << "\n";
    }

    GameState state;
    state.commander_mode = commander_mode;
    if (!state.loadDeck(deck_path)) {
        std::cerr << "Error: could not load deck from \"" << deck_path << "\"\n"
                  << "  Make sure the file exists and contains valid entries.\n";
        return 1;
    }
    std::cout << "Loaded " << state.deck.size()
              << " cards from \"" << argv[1] << "\".\n";

    HandWindow    hand_win(state);
    PlaymatWindow playmat_win(state);

    state.hand_window_ptr = &hand_win.window;
    state.playmat_window_ptr = &playmat_win.window;

    const sf::Time FRAME_TIME = sf::seconds(1.f / 120.f);
    sf::Clock frame_clock;

    while (hand_win.window.isOpen() && playmat_win.window.isOpen()) {
        // ── Hand window events ────────────────────────────────────────────
        while (const auto evt = hand_win.window.pollEvent()) {
            if (evt->is<sf::Event::Closed>()) {
                hand_win.window.close();
                playmat_win.window.close();
            } else {
                hand_win.handleEvent(*evt);
            }
        }

        // ── Playmat window events ─────────────────────────────────────────
        while (const auto evt = playmat_win.window.pollEvent()) {
            if (evt->is<sf::Event::Closed>()) {
                playmat_win.window.close();
                hand_win.window.close();
            } else {
                playmat_win.handleEvent(*evt);
            }
        }

        hand_win.render();
        playmat_win.render();

        // ── 60 FPS cap ────────────────────────────────────────────────────
        sf::Time elapsed = frame_clock.restart();
        if (elapsed < FRAME_TIME)
            sf::sleep(FRAME_TIME - elapsed);
    }

    return 0;
}
