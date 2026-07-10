#pragma once

#include "domain/Algorithm.hpp"
#include "domain/SortEvent.hpp"
#include "domain/SortItem.hpp"

#include <vector>

// Aggregate counters collected while an algorithm runs.
// Algorithms update these values; animation and rendering should treat them as read-only results.
struct SortStats {
    int comparisons = 0;
    int swaps = 0;
    int moves = 0;
};

// Complete output from one sorting run.
// finalItems is the sorted result. events is the replayable history used later by animation.
struct SortTrace {
    std::vector<SortItem> finalItems;
    std::vector<SortEvent> events;
    SortStats stats;
};

// Public entry point for the sorting layer.
// Individual algorithm functions stay private to the sorting implementation.
SortTrace runSort(
    Algorithm algorithm,
    const std::vector<SortItem>& items);
