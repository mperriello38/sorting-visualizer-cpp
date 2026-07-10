#pragma once

#include "domain/SortState.hpp"

// Renderer boundary.
//
// The renderer draws an already-built SortState. It should not generate input,
// run sorting algorithms, replay events, or poll keyboard/mouse input.
class RaylibRenderer {
public:
    // Read SortState, map ItemVisualState to colors, calculate bar positions,
    // and draw bars using raylib. App owns BeginDrawing()/EndDrawing().
    void drawSortState(const SortState& state) const;    
};
