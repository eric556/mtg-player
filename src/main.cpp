#include "game_state.hpp"
#include "hand_window.hpp"
#include "playmat_window.hpp"
#include "card_requester.hpp"
#include <iostream>

int main(int argc, char* argv[])
{
    bool        commander_mode = false;
    const char* deck_path      = nullptr;
    const char* cache_path     = nullptr;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--commander" || a == "-c") {
            commander_mode = true;
        } else if ((a == "--deck" || a == "-d") && i + 1 < argc) {
            deck_path = argv[++i];
        } else if ((a == "--cache") && i + 1 < argc) {
            cache_path = argv[++i];
        } else {
            std::cerr << "Unknown argument: " << a << "\n"
                      << "Usage: mtg-sim --deck <deck.txt> [--cache <dir>] [--commander|-c]\n"
                      << "  --deck / -d       Path to deck list file (required)\n"
                      << "  --cache           Directory for caching card art\n"
                      << "  --commander / -c  First card becomes the commander\n"
                      << "  Deck format (one entry per line):\n"
                      << "    4 Lightning Bolt\n"
                      << "    1x Black Lotus\n"
                      << "    // comments are ignored\n";
            return 1;
        }
    }

    if (!deck_path) {
        std::cerr << "Error: --deck <deck.txt> is required.\n"
                  << "Usage: mtg-sim --deck <deck.txt> [--cache <dir>] [--commander|-c]\n";
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
              << " cards from \"" << deck_path << "\".\n";

    HandWindow    hand_win(state);
    PlaymatWindow playmat_win(state);

    hand_win.setPlaymatWindow(&playmat_win);
    playmat_win.setHandWindow(&hand_win);

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

        sf::Time elapsed = frame_clock.restart();
        state.updateAnimations(elapsed.asSeconds());

        hand_win.render();
        playmat_win.render();

        // -- 60 FPS cap ----------------------------------------------------
        if (elapsed < FRAME_TIME)
            sf::sleep(FRAME_TIME - elapsed);
    }

    return 0;
}
