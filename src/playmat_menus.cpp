// playmat_menus.cpp — Context-menu action dispatch for PlaymatWindow.
// Contains: applyContextAction, applyZAction, applyCmdContextAction,
//           applyGYBulkAction, applyCmdViewerCtxAction.

#include "playmat_window.hpp"
#include "playmat_defs.hpp"
#include "game_action.hpp"

// ── Battlefield card right-click ──────────────────────────────────────────

void PlaymatWindow::applyContextAction(int item) {
    int idx = ctx_menu_.target_idx;
    if (idx < 0 || idx >= static_cast<int>(state_.battlefield.size())) return;

    sf::Vector2i src = window.getPosition()
                     + sf::Vector2i(window.mapCoordsToPixel(state_.battlefield[idx].position));
    auto fly = [&](Card& c, Zone to) {
        c.is_flying_cross_window = true;
        c.start_desktop_pos      = src;
        c.end_desktop_pos        = state_.zoneDesktopCenter(to);
        c.anim_timer             = 0.f;
        c.is_animating           = false;
    };

    // Dynamic "Return to command zone" item appended past the fixed enum range.
    if (item == static_cast<int>(CTX_ITEMS.size())) {
        state_.history.push(
            std::make_unique<MoveCardAction>(Zone::BATTLEFIELD, idx, Zone::COMMAND_ZONE), state_);
        if (auto* c = state_.lastInZone(Zone::COMMAND_ZONE)) fly(*c, Zone::COMMAND_ZONE);
        return;
    }
    switch (item) {
        case CTX_FLIP:
            state_.battlefield[idx].face_down = !state_.battlefield[idx].face_down;
            break;
        case CTX_TO_HAND:
            state_.history.push(
                std::make_unique<MoveCardAction>(Zone::BATTLEFIELD, idx, Zone::HAND), state_);
            if (auto* c = state_.lastInZone(Zone::HAND)) fly(*c, Zone::HAND);
            break;
        case CTX_TO_GY:
            state_.history.push(
                std::make_unique<MoveCardAction>(Zone::BATTLEFIELD, idx, Zone::GRAVEYARD), state_);
            if (auto* c = state_.lastInZone(Zone::GRAVEYARD)) fly(*c, Zone::GRAVEYARD);
            break;
        case CTX_TO_EXILE:
            state_.history.push(
                std::make_unique<MoveCardAction>(Zone::BATTLEFIELD, idx, Zone::EXILE), state_);
            if (auto* c = state_.lastInZone(Zone::EXILE)) fly(*c, Zone::EXILE);
            break;
        case CTX_TO_DECK_TOP:
            state_.history.push(
                std::make_unique<MoveCardAction>(Zone::BATTLEFIELD, idx, Zone::DECK, DeckPos::TOP), state_);
            if (auto* c = state_.lastInZone(Zone::DECK, DeckPos::TOP)) fly(*c, Zone::DECK);
            break;
        case CTX_TO_DECK_BOT:
            state_.history.push(
                std::make_unique<MoveCardAction>(Zone::BATTLEFIELD, idx, Zone::DECK, DeckPos::BOTTOM), state_);
            if (auto* c = state_.lastInZone(Zone::DECK, DeckPos::BOTTOM)) fly(*c, Zone::DECK);
            break;
        case CTX_ADD_CTR: state_.battlefield[idx].counters++; break;
        case CTX_REM_CTR: state_.battlefield[idx].counters--; break;
    }
}

// ── Battlefield card shift+right-click (z-order) ─────────────────────────

void PlaymatWindow::applyZAction(int item) {
    int idx = z_menu_.target_idx;
    if (idx < 0 || idx >= static_cast<int>(state_.battlefield.size())) return;
    switch (item) {
        case ZTX_TOP: {
            auto c = state_.battlefield[idx];
            state_.battlefield.erase(state_.battlefield.begin() + idx);
            state_.battlefield.push_back(c);
            break;
        }
        case ZTX_BOT: {
            auto c = state_.battlefield[idx];
            state_.battlefield.erase(state_.battlefield.begin() + idx);
            state_.battlefield.insert(state_.battlefield.begin(), c);
            break;
        }
        case ZTX_UP:
            if (idx < static_cast<int>(state_.battlefield.size()) - 1)
                std::swap(state_.battlefield[idx], state_.battlefield[idx + 1]);
            break;
        case ZTX_DOWN:
            if (idx > 0)
                std::swap(state_.battlefield[idx], state_.battlefield[idx - 1]);
            break;
    }
}

// ── Command zone card right-click ─────────────────────────────────────────

void PlaymatWindow::applyCmdContextAction(int item) {
    int idx = cmd_ctx_menu_.target_idx;
    if (idx < 0 || idx >= (int)state_.command_zone.size()) return;

    sf::Vector2f cmd_pos = {PM_CMD_CX + idx * (CARD_W + 10.f), PM_CMD_CY};
    sf::Vector2i src = window.getPosition()
                     + sf::Vector2i(window.mapCoordsToPixel(cmd_pos));
    auto fly = [&](Card& c, Zone to) {
        c.is_flying_cross_window = true;
        c.start_desktop_pos      = src;
        c.end_desktop_pos        = state_.zoneDesktopCenter(to);
        c.anim_timer             = 0.f;
        c.is_animating           = false;
    };

    switch (item) {
        case CMD_TO_BF: {
            state_.history.push(
                std::make_unique<MoveCardAction>(Zone::COMMAND_ZONE, idx, Zone::BATTLEFIELD), state_);
            if (auto* c = state_.lastInZone(Zone::BATTLEFIELD)) {
                c->target_position = {w_ / 2.f, h_ / 2.f};
                c->position        = cmd_pos;
                fly(*c, Zone::BATTLEFIELD);
            }
            break;
        }
        case CMD_TO_HAND:
            state_.history.push(
                std::make_unique<MoveCardAction>(Zone::COMMAND_ZONE, idx, Zone::HAND), state_);
            if (auto* c = state_.lastInZone(Zone::HAND)) fly(*c, Zone::HAND);
            break;
        case CMD_TO_GY:
            state_.history.push(
                std::make_unique<MoveCardAction>(Zone::COMMAND_ZONE, idx, Zone::GRAVEYARD), state_);
            if (auto* c = state_.lastInZone(Zone::GRAVEYARD)) fly(*c, Zone::GRAVEYARD);
            break;
        case CMD_TO_EXILE:
            state_.history.push(
                std::make_unique<MoveCardAction>(Zone::COMMAND_ZONE, idx, Zone::EXILE), state_);
            if (auto* c = state_.lastInZone(Zone::EXILE)) fly(*c, Zone::EXILE);
            break;
        case CMD_TO_DECK_TOP:
            state_.history.push(
                std::make_unique<MoveCardAction>(Zone::COMMAND_ZONE, idx, Zone::DECK, DeckPos::TOP), state_);
            if (auto* c = state_.lastInZone(Zone::DECK, DeckPos::TOP)) fly(*c, Zone::DECK);
            break;
        case CMD_TO_DECK_BOT:
            state_.history.push(
                std::make_unique<MoveCardAction>(Zone::COMMAND_ZONE, idx, Zone::DECK, DeckPos::BOTTOM), state_);
            if (auto* c = state_.lastInZone(Zone::DECK, DeckPos::BOTTOM)) fly(*c, Zone::DECK);
            break;
        case CMD_ADD_CTR: state_.command_zone[idx].counters++; break;
        case CMD_REM_CTR: state_.command_zone[idx].counters--; break;
    }
}

// ── Graveyard viewer bulk action ──────────────────────────────────────────

void PlaymatWindow::applyGYBulkAction(int item) {
    Zone    to = Zone::HAND;
    DeckPos dp = DeckPos::TOP;
    switch (item) {
        case GY_BULK_TO_BF:    to = Zone::BATTLEFIELD; break;
        case GY_BULK_TO_DECK:  to = Zone::DECK;        break;
        case GY_BULK_TO_EXILE: to = Zone::EXILE;       break;
        default: return;
    }
    while (!state_.graveyard.empty())
        state_.moveCard(Zone::GRAVEYARD, (int)state_.graveyard.size() - 1, to, dp);
    gy_viewer_.hide();
}

// ── Commander viewer card right-click ─────────────────────────────────────

void PlaymatWindow::applyCmdViewerCtxAction(int item) {
    int viewer_idx = cmd_viewer_ctx_.target_idx;
    if (viewer_idx < 0 || viewer_idx >= (int)cmd_viewer_.entries.size()) return;
    int original_idx = cmd_viewer_.entries[viewer_idx].original_idx;
    if (original_idx < 0 || original_idx >= (int)state_.command_zone.size()) return;

    if (item == CMD_VC_PLAY) {
        sf::Vector2f cmd_pos = {PM_CMD_CX + original_idx * (CARD_W + 10.f), PM_CMD_CY};
        sf::Vector2i src = window.getPosition()
                         + sf::Vector2i(window.mapCoordsToPixel(cmd_pos));
        state_.moveCard(Zone::COMMAND_ZONE, original_idx, Zone::BATTLEFIELD);
        if (auto* c = state_.lastInZone(Zone::BATTLEFIELD)) {
            c->target_position        = {w_ / 2.f, h_ / 2.f};
            c->position               = cmd_pos;
            c->is_flying_cross_window = true;
            c->start_desktop_pos      = src;
            c->end_desktop_pos        = state_.zoneDesktopCenter(Zone::BATTLEFIELD);
            c->anim_timer             = 0.f;
            c->is_animating           = false;
        }
        cmd_viewer_.hide();
    } else if (item == CMD_VC_ADD_TAX) {
        state_.command_zone[original_idx].counters++;
        // Re-open viewer so the updated counter is reflected.
        cmd_viewer_.show("Command Zone", state_.command_zone, Zone::COMMAND_ZONE);
    }
}
