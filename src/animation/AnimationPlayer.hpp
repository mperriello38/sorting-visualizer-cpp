#pragma once

#include "domain/SortEvent.hpp"
#include "domain/SortItem.hpp"
#include "domain/SortState.hpp"

#include <cstddef>
#include <vector>

// AnimationPlayer replays a completed sorting trace one event at a time.
//
// This layer owns playback state: current item order, visual marks, sorted marks,
// and which event should be applied next. It should not know about raylib,
// drawing coordinates, colors, buttons, windows, algorithms, or input generation.
//
// Input:  initial SortItems plus SortEvents produced by the sorting layer.
// Output: SortState snapshots that the rendering layer can draw.
//
// The player exposes event positions instead of reverse-event mechanics.
// Position 0 means no events have been applied. Position N means the first N
// events have been applied. This lets the implementation support backward
// stepping and future timeline scrubbing without making SortEvents reversible.

class AnimationPlayer {
public:
    // Replace the current animation with a new trace and return to frame zero.
    //
    // After load(), currentState() should show the initial item order with no
    // transient event marks. Any previous trace should be discarded.
    void load(
        const std::vector<SortItem>& initialItems,
        const std::vector<SortEvent>& events
    );

    // Return the current visualizable state.
    //
    // The renderer should read this state, but should not modify it. Returning a
    // const reference avoids copying the whole item list every frame.
    const SortState& currentState() const;

    // Return true when every event has been applied.
    //
    // This is a playback question, not a sorting correctness test. Sorting
    // correctness belongs in the testing layer.
    bool isComplete() const;

    // Return to the beginning of the loaded trace.
    //
    // The initial items and events should remain loaded. Only current item order,
    // visual states, sorted marks, and the next event index should reset.
    void reset();

    // Apply exactly one event, then rebuild the current SortState.
    //
    // If playback is already complete, this should do nothing. Keeping this
    // function step-based makes it easy to test before adding real-time controls.
    void stepForward();

    // Move to a playback position in the loaded event stream.
    //
    // eventPosition is clamped to the valid range 0..eventCount(). The first
    // implementation rebuilds from frame zero, but callers should not depend on
    // that detail; a future checkpoint implementation can keep this interface.
    void seekToEventPosition(std::size_t eventPosition);

    // Move one event backward. If already at frame zero, this does nothing.
    void stepBackward();

    // Return the number of events already applied to the current state.
    std::size_t currentEventPosition() const;

    // Return the number of events in the loaded trace.
    std::size_t eventCount() const;

private:
    // The original items for the loaded trace. reset() uses this to return to
    // frame zero without asking the sorting layer to run again.
    std::vector<SortItem> initialItems_;

    // The current logical item order after applying events up to nextEventIndex_.
    std::vector<SortItem> currentItems_;

    // The replayable event stream produced by the sorting layer.
    std::vector<SortEvent> events_;

    // Persistent per-index sorted flags. This is separate from currentState_
    // because SortState is the output snapshot, not the source of truth.
    std::vector<bool> sorted_;

    // Cached output snapshot returned by currentState().
    SortState currentState_;

    // Index of the next event to apply. If this equals events_.size(), playback
    // has consumed the whole event stream.
    std::size_t nextEventIndex_ = 0;
};
