#pragma once
// Internal definitions shared across playmat_window implementation files.
// Not part of the public API — do not include from other translation units.

#include <vector>
#include <string>

// Command zone anchor (top-left corner, fixed).
static constexpr float PM_CMD_CX = 65.f, PM_CMD_CY = 100.f;

// ── Right-click menu: zone actions + card state ───────────────────────────
enum CtxItem {
    CTX_FLIP = 0, CTX_TO_HAND, CTX_TO_GY, CTX_TO_EXILE,
    CTX_TO_DECK_TOP, CTX_TO_DECK_BOT,
    CTX_ADD_CTR, CTX_REM_CTR,
};
inline const std::vector<std::string> CTX_ITEMS = {
    "Flip face-down / up",
    "Return to hand",
    "Send to graveyard",
    "Send to exile",
    "To top of deck",
    "To bottom of deck",
    "Add counter",
    "Remove counter",
};

// ── Right-click menu on command zone cards ────────────────────────────────
enum CmdCtxItem {
    CMD_TO_BF = 0, CMD_TO_HAND, CMD_TO_GY, CMD_TO_EXILE,
    CMD_TO_DECK_TOP, CMD_TO_DECK_BOT,
    CMD_ADD_CTR, CMD_REM_CTR,
};
inline const std::vector<std::string> CMD_ITEMS = {
    "Play commander",
    "Return to hand",
    "Send to graveyard",
    "Send to exile",
    "To top of deck",
    "To bottom of deck",
    "Add commander tax",
    "Remove commander tax",
};

// ── Shift+right-click menu: z-depth ordering ─────────────────────────────
enum ZCtxItem {
    ZTX_TOP = 0, ZTX_BOT, ZTX_UP, ZTX_DOWN,
};
inline const std::vector<std::string> Z_ITEMS = {
    "Send to front",
    "Send to back",
    "Move up one",
    "Move down one",
};

// ── Right-click inside GY viewer: bulk zone actions ───────────────────────
enum GYBulkItem {
    GY_BULK_TO_BF = 0, GY_BULK_TO_DECK, GY_BULK_TO_EXILE,
};
inline const std::vector<std::string> GY_BULK_ITEMS = {
    "All to battlefield",
    "All to deck (top)",
    "All to exile",
};

// ── Right-click on a card inside the commander viewer ─────────────────────
enum CmdViewerCtxItem {
    CMD_VC_PLAY = 0, CMD_VC_ADD_TAX,
};
inline const std::vector<std::string> CMD_VIEWER_CTX_ITEMS = {
    "Play commander",
    "Add commander tax",
};
