#include "game_state.hpp"
#include "hand_window.hpp"
#include "playmat_window.hpp"
#include <iostream>

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: mtg-sim <deck.txt>\n"
                  << "  Deck format (one entry per line):\n"
                  << "    4 Lightning Bolt\n"
                  << "    1x Black Lotus\n"
                  << "    // comments are ignored\n";
        return 1;
    }

    GameState state;
    if (!state.loadDeck(argv[1])) {
        std::cerr << "Error: could not load deck from \"" << argv[1] << "\"\n"
                  << "  Make sure the file exists and contains valid entries.\n";
        return 1;
    }
    std::cout << "Loaded " << state.deck.size()
              << " cards from \"" << argv[1] << "\".\n";

    HandWindow    hand_win(state);
    PlaymatWindow playmat_win(state);

    // Draw an opening hand of 7 (standard MTG rule).
    for (int i = 0; i < 7 && !state.deck.empty(); ++i)
        state.drawCard();

    const sf::Time FRAME_TIME = sf::seconds(1.f / 60.f);
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
