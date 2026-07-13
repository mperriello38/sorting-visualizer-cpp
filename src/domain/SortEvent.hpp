#pragma once

#include "SortItem.hpp"

#include <variant>

// Sorting algorithms do not mutate the visualizer directly.
// Instead, they emit a list of events. The animation layer replays those events later.
//
// Each event shape is its own struct so that every event carries only the fields
// that are meaningful for that event. This avoids one generic "event" struct with
// unused fields and hidden agreements.

// Event replay contract:
//
// CompareEvent:
// Does not change item order.
// Marks both indices Comparing for the current state.

// SwapEvent:
// Swaps current items.
// Marks both indices Swapping for the current state.

// MoveEvent:
// Assigns movedItem into destinationIndex.
// Marks destinationIndex Moving for the current state.

// MarkSortedEvent:
// Does not change item order.
// Records that index as persistently Sorted.

// Important distinction:
// Comparing, Swapping, Moving = transient states for the most recent event.
// Sorted = persistent state that should remain visible after later events.

// Record a comparison between the items at two current indices.
struct CompareEvent {
    int leftIndex;
    int rightIndex;
};

// Exchange the items at two current indices.
struct SwapEvent {
    int leftIndex;
    int rightIndex;
};

// Place movedItem at destinationIndex.
// This supports algorithms such as insertion sort and merge sort where a step is
// not naturally described as "swap these two indices."
struct MoveEvent {
    int destinationIndex;
    SortItem movedItem;
};

// Mark one index as sorted/finalized.
// Marking one index at a time keeps the event meaning simple.
struct MarkSortedEvent {
    int index;
};

// A SortEvent is exactly one event shape.
using SortEvent = std::variant<
    CompareEvent,
    SwapEvent,
    MoveEvent,
    MarkSortedEvent
>;
