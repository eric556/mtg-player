# MTG Sim

A two-window Magic: The Gathering tabletop simulator built with C++ and SFML 3.

- **Playmat window** — share this on stream; shows the battlefield, graveyard, exile, and command zone
- **Hand window** — keep this private; shows your hand, deck controls, and pile browsers

## Building

**Requirements:** CMake 3.28+, a C++20 compiler. All other dependencies (SFML, cpr, nlohmann/json) are fetched automatically.

**Linux** — install system deps first:
```
sudo apt-get install libx11-dev libxrandr-dev libxcursor-dev libxi-dev libudev-dev libgl-dev libfreetype6-dev libharfbuzz-dev
```

**Build (all platforms):**
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The binary ends up at `build/bin/Release/mtg-sim` (Linux: `build/bin/mtg-sim`).

## Usage

```
mtg-sim --deck <deck.txt> [--cache <dir>] [--commander | -c]
```

- `--deck` / `-d` — path to deck list file (required)
- `--cache` — optional directory for caching card art fetched from Scryfall
- `--commander` / `-c` — treat the first card in the list as the commander; it starts in the command zone instead of the deck

**Example:**
```
mtg-sim --deck my_deck.txt --cache ./cache --commander
```

## Controls

### Hand window
| Action | Result |
|---|---|
| Click a hand card | Select it |
| Click selected card again | Deselect |
| **Play Card** button | Move selected card to battlefield |
| **Draw** button / click deck | Draw a card |
| **Shuffle** button | Shuffle the deck |
| **Reset Deck** button | Return all cards to deck and shuffle |
| **Search Deck** button | Browse and move cards from deck |
| Right-click hand card | Send to graveyard, exile, or deck |
| Click graveyard / exile / command zone pile | Browse and move cards |

### Playmat window
| Action | Result |
|---|---|
| Drag card | Move it on the battlefield |
| Double-click card | Tap / untap |
| Scroll wheel on card | Resize card |
| Right-click card | Zone actions (return to hand, send to GY/exile, etc.) |
| Shift + right-click card | Z-depth ordering (send to front/back) |
| Click graveyard or exile pile | Browse and move cards |
| Right-click command zone card | Move to battlefield, add/remove counters (tax), etc. |
