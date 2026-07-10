# App Layer

The app layer is the composition root for the visualizer.

It owns the raylib window loop and wires together input generation, sorting,
animation, and rendering. It should coordinate modules without moving their
internal logic into the app.

## Current Responsibilities

`App.cpp` currently owns:

- window creation and shutdown
- initial run settings
- draft run settings edited by keyboard controls
- loaded run settings used by the active animation trace
- explicit apply/regenerate behavior
- playback controls
- per-frame playback work limits
- app-level text for controls and status
- keyboard mappings for item count, algorithm, value spec, initial order, and apply
- calling `RaylibRenderer` to draw the current `SortState`

This is acceptable while the interface is still keyboard-driven. The main loop
should remain easy to scan:

```text
handle settings input
handle playback input
update playback
draw app
```

## Draft Settings And Loaded Settings

The app intentionally separates:

- `draftSettings`: what the user is currently editing
- `loadedSettings`: what generated the active animation trace

Changing item count, algorithm, value spec, or initial order edits
`draftSettings` only. Pressing ENTER applies the draft, generates new input,
runs the selected algorithm, and loads a new trace into the animation player.

This prevents a dangerous hidden rule where item count changes could invalidate
the old event stream in the middle of playback.

`App.cpp` now separates app actions from keyboard polling. App actions describe
user intent and mutate app state:

- set or nudge the draft item count
- choose draft algorithm, value spec, or initial order
- apply draft settings
- toggle playback
- reset playback
- step forward while paused
- step backward while paused
- set or change playback speed

Keyboard helpers should stay thin: they check raylib key state and call these
actions. Future buttons and sliders should call the same actions instead of
duplicating state changes.

Current keyboard settings are:

- `A` / `D`: decrease or increase draft item count
- `1`, `2`, `3`: choose bubble, insertion, or selection sort
- `4`, `5`, `6`: choose permutation, few-unique, or all-equal values
- `7`, `8`, `9`, `0`: choose random, ascending, descending, or nearly ascending order
- `ENTER`: apply the draft settings and regenerate the loaded run
- `LEFT` / `RIGHT`: step backward or forward while paused

## Playback Policy

`AnimationPlayer` applies one event at a time. The app decides how often to call
`stepForward()`.

The current app policy includes:

- `secondsPerEvent`
- speed-up and slow-down key controls
- `maxEventsPerFrame`
- manual one-event stepping while paused
- manual one-event backward stepping while paused
- reset to the start of the currently loaded trace

`maxEventsPerFrame` is a responsiveness cap. It prevents very fast playback and
large item counts from consuming too much work in one rendered frame.

## Design Warnings

Watch these areas as the app grows:

- App drawing is split into small helpers now, but the positions are still
  hard-coded. A custom UI pass should introduce clearer layout helpers before
  adding many more text rows or mouse controls.
- The app action helpers are private to `App.cpp` for now. If custom UI grows
  enough that these helpers need their own tests or files, extract them as one
  cohesive app-control concept rather than many tiny modules.
- `refreshSettingsDirty` currently relies on `SortRunSpec` value equality. If a
  future editable setting is not part of `SortRunSpec`, dirty-state logic must
  account for it explicitly.
- Keyboard controls are useful for proving behavior, but custom UI controls
  should request app actions rather than generating input or replaying events
  themselves.
- Live preview while dragging a future slider should be treated as a deliberate
  policy. The safe default is still edit draft settings first, apply later.
