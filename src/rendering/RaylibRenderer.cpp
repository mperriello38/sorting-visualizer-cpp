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
    // switch chooses one branch based on the enum value.
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

Rectangle calculateChartBounds()
{
    // These margins reserve space for the title, playback text, and future UI.
    // They are renderer-private layout policy: App should not need to know where
    // the chart begins or how much space is left around it.
    //
    // Design warning: these margins currently line up with app-level text drawn
    // in App.cpp. If the UI becomes a real panel system, pass a deliberate chart
    // rectangle or layout object instead of growing hidden margin agreements.
    const float leftMargin = 60.0f;
    const float rightMargin = 60.0f;
    const float topMargin = 380.0f;
    const float bottomMargin = 380.0f;

    // Keep the chart dimensions positive even if the window is made very small.
    // This prevents invalid bar-width math later in drawSortState().
    const float minimumChartWidth = 1.0f;
    const float minimumChartHeight = 1.0f;

    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float screenHeight = static_cast<float>(GetScreenHeight());
    const float chartWidth = std::max(
        minimumChartWidth,
        screenWidth - leftMargin - rightMargin);

    const float chartHeight = std::max(
        minimumChartHeight,
        screenHeight - topMargin - bottomMargin);

    return Rectangle{
        leftMargin,
        topMargin,
        chartWidth,
        chartHeight
    };
}

}

void RaylibRenderer::drawSortState(const SortState& state) const
{
    // App owns BeginDrawing(), ClearBackground(), and EndDrawing().
    // This function assumes raylib is already inside an active drawing frame.

    // If there are no items, there is no bar geometry to calculate.
    if (state.items.empty()) {
        // DrawText(text, x, y, fontSize, color)
        // raylib positions are pixels measured from the top-left corner.
        DrawText("No items to draw.", 40, 120, 20, GRAY);
        return;
    }

    // Find the largest positive value so every bar can be scaled relative to it.
    // Starting at 1 prevents division by zero if every value is 0 or negative.
    int maxValue = 1;

    for (const SortItemState& itemState : state.items) {
        maxValue = std::max(maxValue, itemState.item.value);
    }

    // The renderer owns chart layout. The app can resize the window or later
    // change item counts without needing to know this geometry.
    const Rectangle chart = calculateChartBounds();
    const float chartBottom = chart.y + chart.height;

    // Draw the chart outline so it is clear where the plotting area is.
    DrawRectangleLines(
        static_cast<int>(chart.x),
        static_cast<int>(chart.y),
        static_cast<int>(chart.width),
        static_cast<int>(chart.height),
        LIGHTGRAY);

    // Convert item count to float for pixel layout math.
    // The empty-state check above guarantees itemCount is not zero.
    const int itemCount = static_cast<int>(state.items.size());
    const float slotWidth = chart.width / static_cast<float>(itemCount);

    // Each item owns one horizontal slot. The actual bar uses 80% of the slot,
    // leaving 20% as visual gap between adjacent bars.
    const float barWidth = slotWidth * 0.8f;

    for (int index = 0; index < itemCount; ++index) {
        // std::vector indexing uses std::size_t. The loop index is int because
        // raylib drawing functions also use int pixel coordinates.
        const SortItemState& itemState = state.items[static_cast<std::size_t>(index)];

        // This first renderer treats nonpositive values as zero-height bars.
        // Later, negative values should be handled with a visible zero baseline.
        const int positiveValue = std::max(0, itemState.item.value);
        const float normalizedHeight = static_cast<float>(positiveValue) / static_cast<float>(maxValue);

        // Convert the normalized 0.0 -> 1.0 height into screen pixels.
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
