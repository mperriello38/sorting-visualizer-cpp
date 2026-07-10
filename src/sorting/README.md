# Sorting Layer

The sorting layer owns algorithm implementations. It should be pure C++ logic: no raylib, no frame timing, no drawing, no UI controls, and no input generation.

## Public Interface

Most code should enter the sorting layer through:

```cpp
SortTrace runSort(
    Algorithm algorithm,
    const std::vector<SortItem>& items);
```

`runSort` dispatches to the selected algorithm and returns a `SortTrace`.

The individual algorithm functions live in separate `.cpp` files. They are not declared in `SortRunner.hpp` because app, animation, rendering, and tests should not need to know those function names.

## Output Contract

Every algorithm should:

- accept `const std::vector<SortItem>& items`
- sort a local copy so the caller's input is unchanged
- preserve whole `SortItem` identity, including `id`
- fill `SortTrace::finalItems`
- append replayable `SortEvent`s
- update `SortStats`
- avoid rendering, animation, app, test, and raylib dependencies

## Files

### `SortRunner.hpp`

Defines the public sorting-layer data structures:

- `SortStats`
- `SortTrace`
- `runSort`

`SortStats` starts at zero by default so each algorithm can increment only the counters it actually uses.

### `SortRunner.cpp`

Dispatches from `Algorithm` to the matching algorithm implementation.

The forward declarations in this file are intentionally private to the sorting layer. A separate `SortAlgorithms.hpp` is not needed yet because it would expose internal implementation names without removing much complexity.

### `BubbleSort.cpp`

Implements bubble sort.

Current event behavior:

- emits `CompareEvent` for each adjacent comparison
- emits `SwapEvent` for each actual swap
- increments comparisons and swaps

Bubble sort is stable in this implementation because equal values are not swapped.

### `InsertionSort.cpp`

Implements insertion sort.

Current event behavior:

- emits `CompareEvent` while scanning left for the insertion point
- emits `MoveEvent` for each right shift
- emits `MoveEvent` when the saved key item is placed
- increments comparisons and moves

The implementation finds the insertion point before shifting. This keeps `CompareEvent` simple because the key item is still physically present at its original index during comparisons.

### `SelectionSort.cpp`

Implements selection sort.

Current event behavior:

- emits `CompareEvent` while searching the unsorted suffix for the minimum
- emits `SwapEvent` only when the minimum is not already in position
- increments comparisons and swaps

Selection sort is not stable in general, because swapping the minimum into position can move equal-valued items past each other.

## Event Guidelines

- Use `CompareEvent` when the algorithm compares two current indices.
- Use `SwapEvent` only when two positions exchange items.
- Use `MoveEvent` when one `SortItem` is assigned into one destination.
- Do not emit no-op swap events.
- Do not add animation timing or visual state to algorithm code.

## Current Test Coverage

The testing layer now checks the main sorting contract for bubble, insertion, and selection sort:

- final output is sorted
- caller input is unchanged
- item identity is preserved
- stats agree with emitted event counts
- event indices are valid
- recorded events replay to `SortTrace::finalItems`
- stable algorithms preserve equal-value id order

Stability is required for bubble sort and insertion sort. Selection sort is treated
as naturally unstable, so its stability result is diagnostic rather than required.

Event replay is tested both directly in sorting tests and through
`AnimationPlayer` integration tests. This keeps the event stream contract owned
by sorting, animation, and testing together, instead of letting rendering or app
code discover hidden algorithm-specific assumptions later.

## Next Work

`AnimationPlayer` now consumes the sorting event stream and has focused tests.
The next sorting-related work is mostly protective:

- keep event replay tests required for every algorithm
- preserve the event contract when adding future algorithms such as shell sort,
  quicksort, and merge sort

New algorithms should follow the existing pattern: sort a local copy, emit
replayable events, update stats, and avoid rendering or animation dependencies.
