#pragma once

// Domain vocabulary for the sorting algorithms the app can run.
//
// This file is intentionally small. It is not a "module" in the APOSD sense
// with a deep implementation behind it; it is a shared type used by the app,
// sorting layer, and UI. Keeping the enum here prevents the same algorithm
// names from being redefined differently in several layers.
//
// enum class is a scoped enum:
// - use Algorithm::Selection, not just Selection
// - it will not silently mix with ordinary integers
enum class Algorithm {
    Selection,
    Bubble,
    Insertion
};
