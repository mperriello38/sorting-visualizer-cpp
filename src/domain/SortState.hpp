#pragma once

#include "SortItem.hpp"

#include <vector>

// Visual status for one item at the current animation frame.
// This is domain vocabulary, not raylib drawing state: it says what an item means,
// not what color or pixel position it should have.
enum class ItemVisualState {
    Normal,
    Comparing,
    Swapping,
    Moving,
    Sorted
};

// SortItemState pairs the item with its visual state.
// Keeping these together avoids the hidden invariant of maintaining two separate
// vectors with matching sizes and indices.
struct SortItemState {
    SortItem item;
    ItemVisualState visualState;
};

// Complete visualizable state for one frame.
// The animation layer should produce this. The rendering layer should draw it.
struct SortState {
    std::vector<SortItemState> items;
    bool complete = false;
};
