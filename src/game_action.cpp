#include "game_action.hpp"
#include "game_state.hpp"

// ── Helpers ────────────────────────────────────────────────────────────────

static std::vector<Card>& zoneVecOf(GameState& gs, Zone z)
{
    switch (z) {
        case Zone::DECK:         return gs.deck;
        case Zone::HAND:         return gs.hand;
        case Zone::BATTLEFIELD:  return gs.battlefield;
        case Zone::GRAVEYARD:    return gs.graveyard;
        case Zone::EXILE:        return gs.exile;
        case Zone::COMMAND_ZONE: return gs.command_zone;
    }
    return gs.deck; // unreachable
}

// ── TapAction ──────────────────────────────────────────────────────────────

void TapAction::execute(GameState& gs)
{
    if (idx_ >= 0 && idx_ < static_cast<int>(gs.battlefield.size()))
        gs.battlefield[idx_].tapped = new_tapped_;
}

void TapAction::undo(GameState& gs)
{
    if (idx_ >= 0 && idx_ < static_cast<int>(gs.battlefield.size()))
        gs.battlefield[idx_].tapped = !new_tapped_;
}

// ── MoveCardAction ─────────────────────────────────────────────────────────

void MoveCardAction::execute(GameState& gs)
{
    auto& src = zoneVecOf(gs, from_);
    if (idx_ < 0 || idx_ >= static_cast<int>(src.size())) return;

    // Save full card state before moveCard resets it.
    saved_card_ = src[idx_];

    // Delegate to GameState::moveCard for the actual move.
    gs.moveCard(from_, idx_, to_, deck_pos_);

    // Find where the card ended up.
    auto& dst = zoneVecOf(gs, to_);
    if (to_ == Zone::DECK && deck_pos_ == DeckPos::BOTTOM)
        dst_idx_ = 0;
    else
        dst_idx_ = static_cast<int>(dst.size()) - 1;
}

void MoveCardAction::undo(GameState& gs)
{
    auto& dst = zoneVecOf(gs, to_);
    if (dst_idx_ < 0 || dst_idx_ >= static_cast<int>(dst.size())) return;

    // Remove the card from the destination zone.
    dst.erase(dst.begin() + dst_idx_);

    // Re-insert the original card snapshot into the source zone.
    auto& src = zoneVecOf(gs, from_);
    // Clamp index so we never go out of range.
    int ins = std::min(idx_, static_cast<int>(src.size()));
    src.insert(src.begin() + ins, saved_card_);
}

// ── DrawCardAction ─────────────────────────────────────────────────────────

void DrawCardAction::execute(GameState& gs)
{
    if (gs.deck.empty()) return;
    // Save the card that will be drawn.
    saved_card_ = gs.deck.back();
    gs.drawCard();
}

void DrawCardAction::undo(GameState& gs)
{
    if (gs.hand.empty()) return;
    // Remove from hand and push back to top of deck.
    gs.hand.pop_back();
    saved_card_.resetState();
    gs.deck.push_back(saved_card_);
}

// ── ActionHistory ──────────────────────────────────────────────────────────

void ActionHistory::push(std::unique_ptr<IGameAction> action, GameState& gs)
{
    action->execute(gs);
    redo_stack_.clear();
    undo_stack_.push_back(std::move(action));
    // Enforce the cap.
    while (undo_stack_.size() > MAX_UNDO)
        undo_stack_.pop_front();
}

void ActionHistory::clearRedo()
{
    redo_stack_.clear();
}

void ActionHistory::clearAll()
{
    undo_stack_.clear();
    redo_stack_.clear();
}

void ActionHistory::undo(GameState& gs)
{
    if (undo_stack_.empty()) return;
    auto action = std::move(undo_stack_.back());
    undo_stack_.pop_back();
    action->undo(gs);
    redo_stack_.push_back(std::move(action));
}

void ActionHistory::redo(GameState& gs)
{
    if (redo_stack_.empty()) return;
    auto action = std::move(redo_stack_.back());
    redo_stack_.pop_back();
    action->execute(gs);
    undo_stack_.push_back(std::move(action));
    while (undo_stack_.size() > MAX_UNDO)
        undo_stack_.pop_front();
}
