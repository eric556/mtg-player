#pragma once
#include "card.hpp"
#include <memory>
#include <deque>
#include <vector>

// Forward-declare so actions can reference GameState without a full include cycle.
struct GameState;

// ── IGameAction ────────────────────────────────────────────────────────────
// Pure interface for reversible game actions.

class IGameAction {
public:
    virtual ~IGameAction() = default;
    virtual void execute(GameState& gs) = 0;
    virtual void undo(GameState& gs)    = 0;
};

// ── TapAction ──────────────────────────────────────────────────────────────
// Toggles the tapped state of a card on the battlefield.

class TapAction : public IGameAction {
public:
    explicit TapAction(int idx, bool new_tapped)
        : idx_(idx), new_tapped_(new_tapped) {}

    void execute(GameState& gs) override;
    void undo(GameState& gs)    override;

private:
    int  idx_;
    bool new_tapped_;
};

// ── MoveCardAction ─────────────────────────────────────────────────────────
// Moves a card from one zone to another.
// Captures the full Card state before the move so undo can restore it exactly
// (position, tapped, counters, etc.).

class MoveCardAction : public IGameAction {
public:
    MoveCardAction(Zone from, int idx, Zone to, DeckPos deck_pos = DeckPos::TOP)
        : from_(from), idx_(idx), to_(to), deck_pos_(deck_pos) {}

    void execute(GameState& gs) override;
    void undo(GameState& gs)    override;

private:
    Zone    from_;
    int     idx_;            // index in source zone at time of execute()
    Zone    to_;
    DeckPos deck_pos_;

    // Snapshot of the card before the move (filled by execute).
    Card    saved_card_;
    // Index where the card landed in the destination zone (filled by execute).
    int     dst_idx_ = -1;
};

// ── DrawCardAction ─────────────────────────────────────────────────────────
// Draws the top card of the deck into hand.

class DrawCardAction : public IGameAction {
public:
    void execute(GameState& gs) override;
    void undo(GameState& gs)    override;

private:
    // Snapshot of the card that was drawn (filled by execute).
    Card saved_card_;
};

// ── ActionHistory ──────────────────────────────────────────────────────────
// Owns the undo/redo stacks.  Shuffle and resetAll clear both stacks.

class ActionHistory {
public:
    static constexpr std::size_t MAX_UNDO = 50;

    // Record and immediately execute an action.
    void push(std::unique_ptr<IGameAction> action, GameState& gs);

    // Clear the redo stack without recording — use for non-undoable ops (shuffle).
    void clearRedo();

    // Clear both stacks (used by resetAll).
    void clearAll();

    bool canUndo() const { return !undo_stack_.empty(); }
    bool canRedo() const { return !redo_stack_.empty(); }

    void undo(GameState& gs);
    void redo(GameState& gs);

private:
    std::deque<std::unique_ptr<IGameAction>> undo_stack_;
    std::deque<std::unique_ptr<IGameAction>> redo_stack_;
};
