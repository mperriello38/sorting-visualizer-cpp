# Domain Layer

The domain layer defines the shared vocabulary of the visualizer. These files should stay renderer-independent and easy for every other layer to understand.

Domain code should not include raylib, own animation timing, generate random input, run tests, or implement sorting algorithms.

## Files

### `Algorithm.hpp`

Defines the sorting algorithms the app can request and the sorting layer can dispatch:

- `Algorithm::Selection`
- `Algorithm::Bubble`
- `Algorithm::Insertion`

This enum belongs in `domain` because it is shared vocabulary, not an algorithm implementation.

### `SortItem.hpp`

Defines one sortable item:

```cpp
struct SortItem {
    int value;
    int id;
};
```

`value` is used for comparisons. `id` is stable identity, which lets tests and animation distinguish equal-valued items.

### `SortEvent.hpp`

Defines the event vocabulary produced by sorting algorithms.

`SortEvent` is a `std::variant` that can hold exactly one event shape:

- `CompareEvent`: two indices were compared.
- `SwapEvent`: two indices exchanged items.
- `MoveEvent`: one `SortItem` was placed at a destination index.
- `MarkSortedEvent`: one index is known to be finalized.

Sorting emits these events. Animation consumes them over time. Rendering should normally draw `SortState`, not raw events.

### `SortSpec.hpp`

Defines the non-visual setup for input generation and full sorting runs.

`SortInputSpec` contains exactly the information needed to create starting items:

- `itemCount`
- `valueSpec`
- `initialOrderSpec`
- `seed`

`SortRunSpec` contains the whole-run configuration:

- `algorithm`
- `SortInputSpec inputSpec`

This split keeps `input` from needing an algorithm field it does not use. The app can use `SortRunSpec` to coordinate a complete run, while input generation can depend only on `SortInputSpec`.

The design separates two concepts that were previously bundled together:

- `ValueSpec`: what values exist.
- `InitialOrderSpec`: how those values are arranged at the start.

`ValueSpec` is a `std::variant` over the supported value modes:

- `PermutationValueSpec`: values are implied by `itemCount`, usually `1..itemCount`, with no duplicates.
- `RangeValueSpec`: values come from `minValue..maxValue`; this mode owns `DuplicatePolicy`.
- `AllEqualValueSpec`: every generated value is the same.
- `FewUniqueValueSpec`: exactly `uniqueValueCount` distinct values are chosen from a range; each chosen value appears at least once.
- `PeriodicValueSpec`: values repeat as `1, 2, ..., periodLength`.

`InitialOrderSpec` is a `std::variant` over the supported starting arrangements:

- `RandomOrderSpec`: shuffle the generated values.
- `AscendingOrderSpec`: sort the generated values from low to high.
- `DescendingOrderSpec`: sort the generated values from high to low.
- `NearlyAscendingOrderSpec`: sort ascending, then add random swaps controlled by `disorderFraction`.

`DuplicatePolicy` belongs inside `RangeValueSpec` because duplicate behavior is configurable for ranges, but not meaningful for every value mode.

The spec structs describe intent. The input layer interprets them.

The spec structs are value-comparable. This lets the app ask whether draft run
settings differ from loaded run settings without manually comparing every nested
field.

### `SortState.hpp`

Defines the current visualizable state of the run.

`SortItemState` pairs a `SortItem` with its current `ItemVisualState`. This avoids keeping separate item and visual-state vectors in sync.

`SortState::complete` describes whether the whole sorting run has finished. Individual finalized items are represented with `ItemVisualState::Sorted`.

## Boundary Rules

- `domain` may use standard-library types such as `std::vector`, `std::variant`, and fixed-width integers.
- `domain` should not include `<raylib.h>`.
- `domain` should not contain drawing, window, keyboard, mouse, random-generation, test, or algorithm implementation code.
- If a concept is only about colors, pixels, or layout, it belongs in `rendering`.
- If a concept is only about time and playback, it belongs in `animation`.
- If a concept is only about how an algorithm works, it belongs in `sorting`.
- If a concept is about generating starting values from a `SortInputSpec`, it belongs in `input`.
