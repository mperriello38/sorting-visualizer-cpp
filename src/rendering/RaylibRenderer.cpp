#include "RaylibRenderer.hpp"

#include <algorithm>
#include <cstddef>

#include <raylib.h>

namespace {

// Renderer-only policy: map domain visual states to raylib colors.
// This stays private because the rest of the program should not know which
// colors represent comparing, swapping, moving, or sorted items.
Color colorForVisualState(ItemVisualState visualState)
{
    // This is rendering policy: the domain says "Comparing"; rendering decides
    // that "Comparing" should be drawn as GOLD.
    switch (visualState) {
    case ItemVisualState::Normal:
        return DARKGRAY;
    
    case ItemVisualState::Comparing:
        return GOLD;

    case ItemVisualState::Swapping:
        return RED;

    case ItemVisualState::Moving:
        return BLUE;

    case ItemVisualState::Sorted:
        return GREEN;
    }

    // Defensive fallback. All current enum values are handled above, but if a
    // new ItemVisualState is added later this prevents undefined color behavior.
    return DARKGRAY;
}

}

void RaylibRenderer::drawSortState(const SortState& state, Rectangle chartBounds) const
{
    // App owns BeginDrawing(), ClearBackground(), and EndDrawing().
    // This function assumes raylib is already inside an active drawing frame.

    if (state.items.empty()) {
        DrawText("No items to draw.", 40, 120, 20, GRAY);
        return;
    }

    // Find the largest positive value so every bar can be scaled relative to it.
    // Starting at 1 prevents division by zero if every value is 0 or negative.
    int maxValue = 1;

    for (const SortItemState& itemState : state.items) {
        maxValue = std::max(maxValue, itemState.item.value);
    }

    // The app owns the chart rectangle because app-level controls and status
    // panels decide how screen space is reserved. The renderer only converts
    // SortState items into bars inside that rectangle.
    const Rectangle chart = chartBounds;
    const float chartBottom = chart.y + chart.height;

    // Draw the chart outline so it is clear where the plotting area is.
    DrawRectangleLines(
        static_cast<int>(chart.x),
        static_cast<int>(chart.y),
        static_cast<int>(chart.width),
        static_cast<int>(chart.height),
        LIGHTGRAY);

    // The empty-state guard above makes this division safe.
    const int itemCount = static_cast<int>(state.items.size());
    const float slotWidth = chart.width / static_cast<float>(itemCount);

    // Each item owns one horizontal slot. The actual bar uses 80% of the slot,
    // leaving 20% as visual gap between adjacent bars.
    const float barWidth = slotWidth * 0.8f;

    for (int index = 0; index < itemCount; ++index) {
        const SortItemState& itemState = state.items[static_cast<std::size_t>(index)];

        // The current chart has no zero baseline, so nonpositive values map to
        // zero-height bars rather than using misleading negative geometry.
        const int positiveValue = std::max(0, itemState.item.value);
        const float normalizedHeight = static_cast<float>(positiveValue) / static_cast<float>(maxValue);

        const int barHeight = static_cast<int>(normalizedHeight * chart.height);

        // raylib coordinates:
        // (0, 0) is the top-left corner.
        // x increases to the right.
        // y increases downward.
        //
        // A bar grows upward from chartBottom, so its top y coordinate is
        // chartBottom - barHeight.
        const int x = static_cast<int>(
            chart.x +
            static_cast<float>(index) * slotWidth +
            (slotWidth - barWidth) / 2.0f);

        const int y = static_cast<int>(chartBottom) - barHeight;

        // Very large item counts can make barWidth less than one pixel.
        // std::max keeps every bar at least barely visible.
        const int width = std::max(1, static_cast<int>(barWidth));
        const int height = barHeight;

        DrawRectangle(
            x,
            y,
            width,
            height,
            colorForVisualState(itemState.visualState));
    }
}
