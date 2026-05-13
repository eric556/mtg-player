// playmat_tick.cpp — Per-frame animation and alt-preview resolution for PlaymatWindow.
// Contains: tick.

#include "playmat_window.hpp"
#include "playmat_defs.hpp"

void PlaymatWindow::tick(float dt) {
    // Advance cross-window fly animations in non-battlefield zones.
    auto advanceFlyZone = [&](std::vector<Card>& zone) {
        for (auto& card : zone) {
            if (!card.is_flying_cross_window) continue;
            card.anim_timer += dt;
            if (card.anim_timer >= 0.6f) {
                card.is_flying_cross_window = false;
                card.rotation = 0.f;
            }
        }
    };
    advanceFlyZone(state_.hand);
    advanceFlyZone(state_.graveyard);
    advanceFlyZone(state_.exile);
    advanceFlyZone(state_.deck);
    advanceFlyZone(state_.command_zone);

    // Battlefield cards: cross-window fly then drop-in animation.
    sf::Vector2f center = {w_ / 2.f, h_ / 2.f};
    sf::Vector2f bottom = {w_ / 2.f, h_ + 300.f};

    for (auto& card : state_.battlefield) {
        if (card.is_flying_cross_window) {
            card.anim_timer += dt;
            constexpr float duration = 0.6f;
            if (card.anim_timer >= duration) {
                card.is_flying_cross_window = false;
                card.rotation   = 0.f;
                card.is_animating = true;
                card.anim_timer   = 0.25f;
            } else {
                float t = card.anim_timer / duration;
                sf::Vector2i cur = {
                    (int)(card.start_desktop_pos.x
                          + (card.end_desktop_pos.x - card.start_desktop_pos.x) * t),
                    (int)(card.start_desktop_pos.y
                          + (card.end_desktop_pos.y - card.start_desktop_pos.y) * t)
                };
                card.position = window.mapPixelToCoords(cur - window.getPosition());
                float dx = (float)(card.end_desktop_pos.x - card.start_desktop_pos.x);
                float dy = (float)(card.end_desktop_pos.y - card.start_desktop_pos.y);
                card.rotation = std::atan2(dy, dx) * 180.f / 3.14159265f + 90.f;
            }
        } else if (card.is_animating) {
            card.anim_timer += dt;
            constexpr float fly_in = 0.25f, hang = 1.0f, settle = 0.4f;
            if (card.anim_timer <= fly_in) {
                float ease = 1.0f - std::pow(1.0f - card.anim_timer / fly_in, 3.0f);
                card.position = bottom + (center - bottom) * ease;
                card.scale    = 4.0f  + (2.5f - 4.0f)    * ease;
            } else if (card.anim_timer <= fly_in + hang) {
                card.position = center;
                card.scale    = 2.5f;
            } else {
                float t = (card.anim_timer - fly_in - hang) / settle;
                if (t >= 1.0f) { t = 1.0f; card.is_animating = false; }
                float ease    = 1.0f - std::pow(1.0f - t, 3.0f);
                card.position = center + (card.target_position - center) * ease;
                card.scale    = 2.5f   + (1.0f - 2.5f)               * ease;
            }
        }
    }

    // Resolve alt-preview card (hold Alt, hit-test at mouse position).
    alt_card_ = nullptr;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LAlt) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RAlt))
    {
        alt_mouse_window_ = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        sf::Vector2f mp   = alt_mouse_window_;

        if (gy_viewer_.visible) {
            if (gy_viewer_.hovered_idx >= 0 &&
                gy_viewer_.hovered_idx < (int)gy_viewer_.entries.size())
                alt_card_ = gy_viewer_.entries[gy_viewer_.hovered_idx].card;
        } else if (exile_viewer_.visible) {
            if (exile_viewer_.hovered_idx >= 0 &&
                exile_viewer_.hovered_idx < (int)exile_viewer_.entries.size())
                alt_card_ = exile_viewer_.entries[exile_viewer_.hovered_idx].card;
        } else {
            int idx = cardAt(mp);
            if (idx >= 0) {
                alt_card_ = &state_.battlefield[idx];
            } else {
                int cidx = cmdCardAt(mp);
                if (cidx >= 0) {
                    alt_card_ = &state_.command_zone[cidx];
                } else {
                    auto hitPile = [&](sf::Vector2f ctr,
                                      const std::vector<Card>& pile) -> const Card* {
                        sf::FloatRect r({ctr.x - CARD_W / 2.f, ctr.y - CARD_H / 2.f},
                                        {CARD_W, CARD_H});
                        if (r.contains(mp))
                            return (pile.empty() ||
                                    (&pile == &state_.deck && !state_.deck_top_visible))
                                   ? nullptr : &pile.back();
                        return nullptr;
                    };
                    if (auto* c = hitPile(gy_ctr_,     state_.graveyard)) alt_card_ = c;
                    else if (auto* c = hitPile(exile_ctr_, state_.exile)) alt_card_ = c;
                }
            }
        }
    }
}
