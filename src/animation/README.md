# Animation Layer

The animation layer turns a completed sorting trace into visualizable playback state.

It should stay pure C++ logic. It should not include raylib, draw bars, read mouse input, own window timing, generate inputs, or choose sorting algorithms.

## Public Interface

The current public type is:

```cpp
class AnimationPlayer;
```

`AnimationPlayer` is intentionally event-position based. The public interface is:

```cpp
void load(
    const std::vector<SortItem>& initialItems,
    const std::vector<SortEvent>& events);

const SortState& currentState() const;
bool isComplete() const;
void reset();
void stepForward();
void seekToEventPosition(std::size_t eventPosition);
void stepBackward();
std::size_t currentEventPosition() const;
std::size_t eventCount() const;
```

This is deliberately small. The app can load a trace, ask for the current frame,
step one event at a time, move backward, seek to a playback position, reset
playback, and ask whether replay is complete.

Real-time controls such as play, pause, speed, elapsed-frame timing, and the
per-frame event cap live in the app-layer `VisualizerSession`. That class calls
this step-based core without exposing raylib to animation or playback policy to
rendering.

## Responsibilities

### `load`

Stores a new trace and resets playback to frame zero.

Input comes from the sorting layer:

- the original `SortItem` list
- the `SortEvent` list produced while sorting

`load` should discard any previous trace.

### `currentState`

Returns the current `SortState` snapshot.

The rendering layer should read this state and draw it. Rendering should not
modify the returned state, which is why the function returns a `const` reference.

### `isComplete`

Answers whether every event has been applied.

This is only a playback question. It does not prove that the sorting algorithm
was correct. Sorting correctness belongs in the testing layer.

### `reset`

Returns the loaded trace to frame zero.

The loaded events remain available. The current item order goes back to the
initial item order, sorted marks are cleared, and the next event index returns to
zero.

### `stepForward`

Applies exactly one event and rebuilds the current `SortState`.

Current event behavior:

- `CompareEvent`: marks two positions as `Comparing`; does not move items.
- `SwapEvent`: swaps two current items and marks those positions as `Swapping`.
- `MoveEvent`: assigns one whole `SortItem` to one destination and marks that position as `Moving`.
- `MarkSortedEvent`: records one position as persistently `Sorted`.

If playback is already complete, `stepForward` does nothing.

### `seekToEventPosition`

Moves to a specific playback position.

An event position is the number of events that have already been applied:

```text
position 0 = before any events have been applied
position 1 = after event 0 has been applied
position N = after N events have been applied
```

The current implementation rebuilds from frame zero and replays forward to the
requested position. That is intentionally hidden inside `AnimationPlayer`.
Future checkpoint-based seeking should be an internal optimization, not a public
interface change.

Seek requests are clamped to `0..eventCount()`. Invalid indices inside the event
stream still throw, because those indicate a broken sorting trace.

### `stepBackward`

Moves one event position backward.

This is implemented through `seekToEventPosition`, not by trying to invert the
previous event. That keeps `SortEvent` one-directional and avoids adding undo
fields to events such as `MoveEvent`.

### `currentEventPosition` And `eventCount`

`currentEventPosition()` reports how many events have already been applied.
`eventCount()` reports the total number of events in the loaded trace. These are
the quantities a future timeline slider should use.

## Internal State

`AnimationPlayer` keeps separate hidden state:

- `initialItems_`: the frame-zero item order
- `currentItems_`: the current logical item order after replayed events
- `events_`: the event stream being replayed
- `sorted_`: persistent sorted marks by index
- `currentState_`: the cached output snapshot for rendering
- `nextEventIndex_`: the next event that will be applied

`currentState_` is not the source of truth. It is rebuilt from `currentItems_`,
`sorted_`, and the most recent transient event marks.

This avoids a hidden synchronization problem where `currentItems_`,
`sorted_`, and `currentState_` could drift apart.

`seekToEventPosition()` currently rebuilds hidden state by replaying events from
the beginning. If this becomes too slow for timeline dragging, this layer can add
private checkpoints without changing app, rendering, sorting, or domain code.

## Event Index Rule

`nextEventIndex_` means:

```text
the next event that has not been applied yet
```

Therefore:

- `nextEventIndex_ == 0` means no events have been applied.
- `nextEventIndex_ == events_.size()` means playback is complete.

## Boundary Notes

The animation layer should know about:

- `SortItem`
- `SortEvent`
- `SortState`
- playback state

The animation layer should not know about:

- raylib
- colors
- bar geometry
- mouse or keyboard input
- input generation specs
- algorithm dispatch
- sorting statistics

Those concerns belong to rendering, app, input, or sorting.

## Current Tests

The testing layer currently verifies:

- `load` produces the initial state
- `stepForward` handles compare, swap, move, and mark-sorted events
- event count and current event position are reported correctly
- `stepBackward` is harmless at the beginning
- `stepBackward` rebuilds earlier state after a move event
- `seekToEventPosition` rebuilds item order and sorted marks
- seeking beyond the end clamps to completion
- `isComplete` changes from false to true at the right time
- stepping after completion is harmless
- final animated item order matches sorting trace replay expectations

## Next Work

The next animation-related work is not more event replay behavior. The core is
already tested. The next pressure will come from app controls:

- pause/play should remain responsive and predictable
- speed and per-frame event limits should remain explicit `VisualizerSession`
  policy instead of scattered numbers
- a future timeline slider should call `seekToEventPosition` rather than
  manipulating event indices itself
- checkpoint-based seeking should be added only if replay-from-start becomes a
  real performance problem

Do not add raylib or drawing code to this layer. Rendering should continue to
consume `SortState` without learning how `AnimationPlayer` stores events,
current items, or sorted marks.
