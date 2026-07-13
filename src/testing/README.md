# Testing Layer

The testing layer contains development checks. These tests verify that low-level modules behave correctly, but they are not part of normal input generation, sorting, animation, or rendering.

Production code should not call into `testing`.

## Public Interface

Current public test runner:

```cpp
TestReport runAllTests();
```

`runAllTests()` is intentionally the only public entry point. The input,
sorting, and animation suite runners are private implementation details inside
`SortTests.cpp`. This keeps the testing module's interface small while allowing
the implementation to stay organized by layer.

`TestReport` contains both summary counts and failure details:

```cpp
struct TestReport {
    TestSummary summary;
    std::vector<TestFailure> failures;
};
```

`TestSummary` separates required contract checks from diagnostic/property checks.
Required failures mean a module violated its current contract. Diagnostic failures are reserved for useful algorithm properties that may not apply to every valid implementation.

`TestFailure` stores the context for one failed check:

```cpp
struct TestFailure {
    TestSeverity severity;
    const char* suiteName;
    const char* caseName;
    const char* checkName;
    const char* message;
};
```

The main app does not display these results. The separate `sorting_tests`
executable prints the summary and one line for each failed check.

From the project root in PowerShell:

```powershell
cmake --build build --config Debug
.\build\Debug\sorting_tests.exe
```

The test executable returns `1` if any required checks fail and `0` otherwise.

## Structure

Most private checks return:

```cpp
struct TestResult {
    bool passed;
    const char* message;
};
```

Recording helpers decide whether a result is required or diagnostic:

- `recordRequired`
- `recordDiagnostic`

This keeps individual checks simple. A check answers "did this condition pass?"
The case runner supplies the suite name, case name, check name, and severity.

`runAllTests()` combines private suite runners for:

- input generation
- sorting
- animation
- app-layer `VisualizerSession`

The private `addReport` helper merges summary counters and failure records.

## Input-Generator Tests

Input-generator tests are fixed development cases. They are meant to catch broken assumptions in `InputGenerator.cpp`.

Each generated input goes through:

- universal checks
- one `ValueSpec` check
- one `InitialOrderSpec` check

Universal checks currently verify:

- generated item count matches `SortInputSpec::itemCount`
- ids are exactly `0..itemCount - 1`
- the same spec and seed produce the same generated input

`ValueSpec` checks currently verify:

- permutation values are exactly `1..itemCount`
- range values stay inside their range and honor `RequireUnique`
- all-equal values are equal
- few-unique values use the requested number of unique values
- periodic values have the expected occurrence counts

`InitialOrderSpec` checks currently verify:

- ascending output is nondecreasing
- descending output is nonincreasing
- nearly ascending with `disorderFraction == 0.0` is ascending
- random order is not judged for "random-looking" behavior

All current input-generator checks are required checks.

## Sorting Tests

Sorting tests use handwritten `std::vector<SortItem>` cases instead of generated input. This keeps sorting tests independent from the input layer.

Current handwritten cases cover:

- empty input
- one item
- already sorted values
- reverse sorted values
- duplicate values
- all equal values
- negative, zero, and positive values

Each case is run against:

- bubble sort
- insertion sort
- selection sort

For every algorithm/input pair, `runSortCase` records these required checks:

- final output is nondecreasing
- caller input was not mutated
- final output contains the same items as the input
- `SortStats` agrees with emitted event counts
- every event index is inside the valid item range
- replaying `SortTrace::events` reconstructs `SortTrace::finalItems`
- stable algorithms preserve equal-value id order

The stability check is required for algorithms whose contract promises stability:

- bubble sort
- insertion sort

The same stability check is recorded diagnostically for selection sort, because
selection sort is naturally unstable. A diagnostic stability failure for selection
sort is useful information, not a broken required contract.

## Animation Tests

Animation tests use handwritten event streams and one real sorting trace. They
protect the playback contract without involving raylib.

Current animation checks verify:

- loading an empty event stream creates a complete initial state
- loading a non-empty event stream starts at frame zero and is not complete
- `CompareEvent` marks two indices without moving items
- `SwapEvent` swaps two items and marks both indices
- `MoveEvent` assigns one `SortItem` to one destination and marks that destination
- `MarkSortedEvent` creates persistent `Sorted` state across later transient events
- event count and current event position are tracked correctly
- `stepBackward()` is harmless at frame zero
- `stepBackward()` can rebuild the state before a `MoveEvent`
- `seekToEventPosition()` rebuilds item order and sorted marks
- seeking beyond the loaded event count clamps to completion
- calling `stepForward()` after completion does not change the state
- a real `SortTrace` from the sorting layer can be replayed to the same final items

All current animation checks are required checks.

## Event Replay Checks

Event replay is now checked for every sorting case:

- start with the original input
- replay every event in `SortTrace::events`
- verify the replayed items match `SortTrace::finalItems`

This test protects the event contract from the sorting side. Animation tests
protect the same contract from the playback side. Together they keep rendering
and app code from needing algorithm-specific assumptions.

## VisualizerSession Tests

App playback policy lives in the pure C++ `VisualizerSession`, so it can be
tested without opening a raylib window. Current required checks cover:

- construction with a loaded deterministic run
- draft isolation and reversible dirty state
- item-count bounds and few-unique compatibility
- applying draft settings as one explicit run transition
- play/pause toggling
- manual stepping changes exactly one event while paused and is ignored while playing
- seeking pauses playback, reaches valid positions, and clamps past the end
- elapsed frame time accumulates, advances multiple due events, and does nothing while paused or complete
- speed changes preserve positive bounded event durations and saturate at both limits
- reset restores frame zero and visual state, clears completion, pauses, and preserves the loaded trace

## Design Notes

Tests may depend on the modules they test. That is not a dangerous dependency direction because tests are outside the production path.

The reverse direction would be dangerous: input, sorting, animation, rendering, and app code should not depend on `testing`.

`sorting_visualizer` and `sorting_tests` are separate executables. The visualizer owns the raylib app path. The test runner owns `TestMain.cpp` and calls the public test functions.
