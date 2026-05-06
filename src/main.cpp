#include "game_state.hpp"
#include "hand_window.hpp"
#include "playmat_window.hpp"
#include "card_requester.hpp"
#include "vcam/vcam_ffmpeg.hpp"
#include <iostream>
#include <cstdlib>
#include <memory>

#ifdef _WIN32
#include "vcam/vcam_win32.hpp"
#endif

int main(int argc, char* argv[])
{
    bool        commander_mode = false;
    const char* deck_path      = nullptr;
    const char* cache_path     = nullptr;
    const char* vcam_output    = nullptr;  // --vcam <path>  → ffmpeg pipe
    bool        vcam_native    = false;    // --vcam-native  → Windows IMFVirtualCamera
    int         vcam_fps       = 30;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--commander" || a == "-c") {
            commander_mode = true;
        } else if ((a == "--deck" || a == "-d") && i + 1 < argc) {
            deck_path = argv[++i];
        } else if (a == "--cache" && i + 1 < argc) {
            cache_path = argv[++i];
        } else if (a == "--vcam" && i + 1 < argc) {
            vcam_output = argv[++i];
        } else if (a == "--vcam-native") {
            vcam_native = true;
        } else if (a == "--vcam-fps" && i + 1 < argc) {
            vcam_fps = std::atoi(argv[++i]);
            if (vcam_fps <= 0) vcam_fps = 30;
        } else {
            std::cerr << "Unknown argument: " << a << "\n"
                      << "Usage: mtg-sim --deck <deck.txt> [options]\n"
                      << "  --deck / -d       Path to deck list file (required)\n"
                      << "  --cache           Directory for caching card art\n"
                      << "  --commander / -c  First card becomes the commander\n"
                      << "  --vcam <output>   Stream via ffmpeg (file, RTMP, /dev/video0, ...)\n"
                      << "  --vcam-native     Virtual webcam via Windows IMFVirtualCamera (Win 11+)\n"
                      << "  --vcam-fps <n>    Framerate for vcam (default: 30)\n";
            return 1;
        }
    }

    if (!deck_path) {
        std::cerr << "Error: --deck <deck.txt> is required.\n";
        return 1;
    }

    if (vcam_output && vcam_native) {
        std::cerr << "Error: --vcam and --vcam-native are mutually exclusive.\n";
        return 1;
    }

#ifndef _WIN32
    if (vcam_native) {
        std::cerr << "Error: --vcam-native is only available on Windows 11.\n";
        return 1;
    }
#endif

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

    state.hand_window_ptr    = &hand_win.window;
    state.playmat_window_ptr = &playmat_win.window;

    if (vcam_output) {
        playmat_win.setVcam(std::make_unique<VcamFfmpeg>(vcam_output), vcam_fps);
    }
#ifdef _WIN32
    else if (vcam_native) {
        playmat_win.setVcam(std::make_unique<VcamWin32>(), vcam_fps);
    }
#endif

    const sf::Time FRAME_TIME = sf::seconds(1.f / 120.f);
    sf::Clock frame_clock;

    while (hand_win.window.isOpen() && playmat_win.window.isOpen()) {
        while (const auto evt = hand_win.window.pollEvent()) {
            if (evt->is<sf::Event::Closed>()) {
                hand_win.window.close();
                playmat_win.window.close();
            } else {
                hand_win.handleEvent(*evt);
            }
        }

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

        sf::Time elapsed = frame_clock.restart();
        if (elapsed < FRAME_TIME)
            sf::sleep(FRAME_TIME - elapsed);
    }

    return 0;
}
