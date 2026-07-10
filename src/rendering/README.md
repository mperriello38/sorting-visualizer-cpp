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
    void drawSortState(const SortState& state, Rectangle chartBounds) const;
};
```

`drawSortState` assumes the app has already started a raylib drawing frame with
`BeginDrawing()`. The app also owns `ClearBackground()`, `EndDrawing()`, and
the chart rectangle passed into the renderer.

## Current Behavior

The renderer currently:

- handles an empty `SortState`
- maps `ItemVisualState` values to raylib colors
- draws one vertical bar per item
- scales bar height relative to the largest positive value
- draws nonpositive values as zero-height bars
- draws inside an app-provided chart rectangle
- keeps chart color and bar geometry decisions private

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

The app decides when to draw and how much screen space the chart receives. The
renderer decides how a given state is drawn inside that chart area.

The current app also draws keyboard-control text and settings status directly.
That is still app-level UI, not chart rendering. The main boundary to watch is
layout: app-level controls, mouse hit boxes, and panel sizing should stay in the
app layer, while the renderer should stay focused on drawing a `SortState`
inside the rectangle it is given.

## Likely Next Work

Do not rush into a large rendering framework. The next improvements should be
small and motivated by visible pain:

- keep colors and bar geometry centralized in renderer-only helpers
- add a small renderer configuration object only when constants become awkward
- keep passing a chart rectangle from the app as custom controls claim space
- add hover hit testing only after the bar geometry is stable
- handle negative values with a visible zero baseline if mixed-sign inputs become
  important in the main visualizer
