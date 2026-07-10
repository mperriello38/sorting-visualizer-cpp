# Rendering Layer

The rendering layer draws `SortState` with raylib.

This layer is allowed to know about pixels, colors, fonts, chart geometry, and
raylib drawing calls. It should not know how sorting algorithms work, how input
is generated, how events are replayed, or how tests are recorded.

## Public Interface

The current renderer is intentionally small:

```cpp
class RaylibRenderer {
public:
    void drawSortState(const SortState& state) const;
};
```

`drawSortState` assumes the app has already started a raylib drawing frame with
`BeginDrawing()`. The app also owns `ClearBackground()` and `EndDrawing()`.

## Current Behavior

The renderer currently:

- handles an empty `SortState`
- maps `ItemVisualState` values to raylib colors
- draws one vertical bar per item
- scales bar height relative to the largest positive value
- draws nonpositive values as zero-height bars
- calculates a chart rectangle from the current window size
- uses renderer-private margins to reserve space for app-level controls and status text

This is enough for a visual slice, but it is not the final renderer design.

## Boundary Rules

Rendering may depend on:

- `SortState`
- `SortItemState`
- `ItemVisualState`
- raylib drawing functions

Rendering should not depend on:

- `SortInputSpec`
- `SortTrace`
- `SortStats`
- `AnimationPlayer`
- algorithm-specific files
- testing code

The app decides when to draw. The renderer decides how a given state is drawn.

The current app also draws keyboard-control text and settings status directly.
That is still app-level UI, not chart rendering. The main boundary to watch is
layout: if app controls need deliberate panels or mouse hit boxes, the app and
renderer should share an explicit chart rectangle instead of relying on matching
hard-coded margins.

## Likely Next Work

Do not rush into a large rendering framework. The next improvements should be
small and motivated by visible pain:

- keep colors, margins, and geometry centralized in renderer-only helpers
- add a small renderer configuration object only when constants become awkward
- pass a chart rectangle from the app only when custom controls need reserved space
- add hover hit testing only after the bar geometry is stable
- handle negative values with a visible zero baseline if mixed-sign inputs become
  important in the main visualizer
