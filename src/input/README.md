# Input Layer

The input layer creates the starting `std::vector<SortItem>` for a run.

Its public interface is intentionally small:

```cpp
std::vector<SortItem> generateInput(const SortInputSpec& spec);
```

Callers describe what they want with `SortInputSpec`. The input layer owns the
mechanics of turning that description into concrete items.

## Flow

`generateInput` follows one sequence:

```text
SortInputSpec
  -> generate integer values from ValueSpec
  -> apply InitialOrderSpec to those values
  -> wrap values into SortItem objects with stable ids
```

This keeps value generation separate from starting order. For example,
`PermutationValueSpec + DescendingOrderSpec` means "create values 1..N, then
arrange them descending." `RangeValueSpec + AscendingOrderSpec` means "sample
from the range, then sort the sampled values ascending."

## Value Specs

`ValueSpec` decides what values exist before ordering is applied:

- `PermutationValueSpec`: creates `1, 2, ..., itemCount`.
- `RangeValueSpec`: samples from an inclusive range, either with duplicates or
  without duplicates.
- `AllEqualValueSpec`: fills every item with the same value.
- `FewUniqueValueSpec`: chooses a small set of distinct values, guarantees each
  chosen value appears at least once, then fills the rest from that set.
- `PeriodicValueSpec`: repeats `1, 2, ..., periodLength`.

The implementation uses overloaded private helpers named
`generateValuesFromSpec`. `std::visit` opens the `ValueSpec` variant and calls
the overload for the active spec type.

## Initial Order Specs

`InitialOrderSpec` decides how generated values are arranged:

- `RandomOrderSpec`: shuffles the values.
- `AscendingOrderSpec`: sorts values from low to high.
- `DescendingOrderSpec`: sorts values from high to low.
- `NearlyAscendingOrderSpec`: sorts ascending, then applies random swaps based
  on `disorderFraction`.

The current meaning of "nearly ascending" is intentionally simple. It does not
guarantee an exact fraction of out-of-order elements.

## Randomness

`SortInputSpec::seed` creates one `std::mt19937` generator for the whole input
run. This makes generation deterministic:

```text
same SortInputSpec + same seed -> same generated input
```

That property is tested and is useful for debugging.

## Boundary Rules

The input layer may depend on:

- `SortInputSpec`
- `ValueSpec`
- `InitialOrderSpec`
- `SortItem`
- standard-library random, vector, and algorithm tools

The input layer should not depend on:

- raylib
- renderer geometry or colors
- `AnimationPlayer`
- sorting algorithm implementations
- sorting statistics
- app controls
- testing code

If a future UI slider changes item count, the app should build a new
`SortInputSpec` and call `generateInput` through the normal run-loading path.
The input layer should not know whether a spec came from a slider, a keyboard
shortcut, a config file, or a test.
