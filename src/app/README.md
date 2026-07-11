# App Layer

The app layer is the composition root and realtime control boundary for the
visualizer. It is split into two cohesive modules:

- `App` owns raylib window setup, input polling, layout, and app-level drawing.
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

## App.cpp

`App.cpp` owns:

- window creation and shutdown
- keyboard and future mouse polling
- translating input into `VisualizerSession` commands
- keyboard-specific increments and default choices
- screen and panel layout
- title, controls, playback status, and settings-status drawing
- passing `VisualizerSession::currentSortState()` to `RaylibRenderer`

The main loop should remain easy to scan:

```text
poll settings input
poll playback input
update the session with frame time
draw the app
```

Raylib input helpers should stay thin. Future buttons and sliders should call
the same session commands used by keyboard input instead of generating data,
running algorithms, or manipulating `AnimationPlayer` directly.

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
App.cpp -> RaylibRenderer
App.cpp -> raylib

VisualizerSession -> input
VisualizerSession -> sorting
VisualizerSession -> animation
VisualizerSession -> domain
```

`VisualizerSession` must not include raylib or rendering headers.

## Design Warnings

- Draft validation is not yet generalized. The current item-count compatibility
  behavior preserves the keyboard UI's few-unique convention, but future range
  and parameter controls will need one explicit validation policy.
- Live preview while dragging a future item-count slider requires a deliberate
  policy. Editing draft settings and applying on release is the safe default.
- Keep layout and input translation in `App.cpp`; do not move pixel geometry or
  key codes into `VisualizerSession`.
- Do not split each control or state transition into its own shallow module.
  Extract a UI-controls module only after buttons, sliders, and selectors reveal
  a cohesive reusable abstraction.
