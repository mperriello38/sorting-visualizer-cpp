#include "SortRunner.hpp"

#include <cstddef>
#include <utility>
#include <vector>

SortTrace selectionSort(const std::vector<SortItem>& items) {
    SortTrace sortTrace;

    // Sorting works on a copy so callers keep their original input unchanged.
    std::vector<SortItem> workingItems = items;

    for (std::size_t i = 0; i + 1 < workingItems.size(); ++i) {
        // Find the smallest item in the unsorted suffix starting at i.
        std::size_t minIndex = i;

        for (std::size_t j = i + 1; j < workingItems.size(); ++j) {
            sortTrace.events.push_back(CompareEvent{static_cast<int>(j), static_cast<int>(minIndex)});
            sortTrace.stats.comparisons += 1;
            if (workingItems[j].value < workingItems[minIndex].value) {
                minIndex = j;
            }
        }

        if (minIndex != i) {
            // Selection sort places the minimum by swapping it into position.
            sortTrace.events.push_back(SwapEvent{static_cast<int>(i), static_cast<int>(minIndex)});
            sortTrace.stats.swaps += 1;
            std::swap(workingItems[i], workingItems[minIndex]);
        }
    }

    sortTrace.finalItems = workingItems;
    return sortTrace;
}
