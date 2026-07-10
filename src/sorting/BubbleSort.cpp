#include "SortRunner.hpp"

#include <cstddef>
#include <utility>
#include <vector>

SortTrace bubbleSort(const std::vector<SortItem>& items) {

    SortTrace sortTrace;

    // Sorting works on a copy so callers keep their original input unchanged.
    std::vector<SortItem> workingItems = items;

    // Each pass bubbles the largest remaining value into the final slot of the
    // unsorted region. unsortedEnd is one past the last index still being checked.
    for (std::size_t unsortedEnd = workingItems.size(); unsortedEnd > 1; --unsortedEnd) {
        bool swapped = false;
        for (std::size_t i = 0; i + 1 < unsortedEnd; ++i) {

            sortTrace.events.push_back(CompareEvent{static_cast<int>(i), static_cast<int>(i + 1)});
            sortTrace.stats.comparisons += 1;
            if (workingItems[i].value > workingItems[i + 1].value) {

                // Record the visual/logical event before mutating the working items.
                sortTrace.events.push_back(SwapEvent{static_cast<int>(i), static_cast<int>(i + 1)});
                sortTrace.stats.swaps += 1;

                std::swap(workingItems[i], workingItems[i + 1]);
                swapped = true;

            }
        }

        if (!swapped) {
            break;
        }

    }

    sortTrace.finalItems = workingItems;
    return sortTrace;

}
