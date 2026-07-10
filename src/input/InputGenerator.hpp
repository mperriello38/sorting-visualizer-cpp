#pragma once

#include "domain/SortItem.hpp"
#include "domain/SortSpec.hpp"

#include <vector>

// Public entry point for the input layer.
//
// generateInput reads the non-visual setup in SortInputSpec and returns the
// starting array used by the sorting algorithms.
//
// The input layer owns:
// - generating raw integer values from ValueSpec
// - applying InitialOrderSpec
// - wrapping values into SortItem objects with stable ids
//
// It should not sort the data, animate events, or draw with raylib.
std::vector<SortItem> generateInput(const SortInputSpec& spec);
