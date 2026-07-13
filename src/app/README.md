# App Layer

The app layer is the composition root and realtime control boundary for the
visualizer. It is split into three cohesive modules:

- `App` owns raylib window setup, frame lifetime, and object composition.
- `AppUi` owns raylib input polling, layout, and app-level drawing.
- `VisualizerSession` owns non-graphical application state and behavior.

The public entry point remains deliberately small:

```cpp
class App {
public:
    void run();
};
```

## VisualizerSession

`VisualizerSession` is pure C++. It owns:

- draft settings currently being edited
- loaded settings that produced the active trace
- applying draft settings and regenerating a run
- the `AnimationPlayer` for the active trace
- play, pause, reset, step, and seek commands
- playback speed, elapsed event time, and the per-frame event cap
- item-count bounds and current few-unique defaults shared with controls

Its setters change draft settings without modifying the loaded trace. Calling
`applyDraftSettings()` is the explicit transition that generates input, runs the
selected algorithm, loads the new trace, pauses playback, and clears old timing.

Dirty state is derived from:

```text
draftSettings != loadedSettings
```

It is not stored as a separate Boolean, so it cannot become stale.

`VisualizerSession::update(frameTime)` receives elapsed time as ordinary data.
It does not call raylib, which keeps playback policy deterministic and testable.

## AppUi

`AppUi` is the raylib-facing user-interface boundary. Its public interface is:

```cpp
class AppUi {
public:
    void handleInput(VisualizerSession& session);
    void draw(
        const VisualizerSession& session,
        const RaylibRenderer& renderer) const;
};
```

`handleInput()` translates keyboard and future mouse interactions into session
commands. `draw()` calculates app layout, draws app-level controls and status,
and delegates chart drawing to `RaylibRenderer`. Layout structures, key codes,
display names, colors, and text helpers remain private to `AppUi.cpp`.

`AppUi` does not own the session or renderer. `App` owns their lifetimes and
passes references once per frame. This avoids hidden lifetime coupling while
keeping the UI interface small.

## App.cpp

`App.cpp` owns:

- window creation and shutdown
- construction of `VisualizerSession`, `RaylibRenderer`, and `AppUi`
- frame timing and session updates
- `BeginDrawing()` / `EndDrawing()` lifetime

The main loop should remain easy to scan:

```text
ask AppUi to handle input
update the session with frame time
open the frame
ask AppUi to draw
close the frame
```

Raylib input helpers in `AppUi.cpp` should stay thin. Future buttons and sliders
should call the same session commands used by keyboard input instead of
generating data, running algorithms, or manipulating `AnimationPlayer`
directly.

## Keyboard Controls

- `A` / `D`: decrease or increase draft item count
- `1`, `2`, `3`: choose bubble, insertion, or selection sort
- `4`, `5`, `6`: choose permutation, few-unique, or all-equal values
- `7`, `8`, `9`, `0`: choose random, ascending, descending, or nearly ascending order
- `ENTER`: apply draft settings and regenerate the loaded run
- `SPACE`: play or pause
- `R`: reset playback
- `UP` / `DOWN`: increase or decrease playback speed
- `LEFT` / `RIGHT`: step backward or forward while paused

## Dependency Direction

```text
App.cpp -> VisualizerSession
App.cpp -> AppUi
App.cpp -> RaylibRenderer
App.cpp -> Domain
App.cpp -> raylib

AppUi -> VisualizerSession
AppUi -> RaylibRenderer
AppUi -> Domain
AppUi -> raylib

VisualizerSession -> Input
VisualizerSession -> Sorting
VisualizerSession -> Animation
VisualizerSession -> Domain
```

`VisualizerSession` must not include raylib or rendering headers.

## Design Warnings

- Draft validation is not yet generalized. The current item-count compatibility
  behavior preserves the keyboard UI's few-unique convention, but future range
  and parameter controls will need one explicit validation policy.
- Live preview while dragging a future item-count slider requires a deliberate
  policy. Editing draft settings and applying on release is the safe default.
- Keep layout and input translation in `AppUi.cpp`; do not move pixel geometry
  or key codes into `App.cpp` or `VisualizerSession`.
- Do not split each control or state transition into its own shallow module.
  Extract reusable widget machinery from `AppUi` only after buttons, sliders,
  and selectors reveal a cohesive abstraction with state or policy to hide.
