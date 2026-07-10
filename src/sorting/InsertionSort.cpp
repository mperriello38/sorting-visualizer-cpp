#include "SortRunner.hpp"

#include <cstddef>
#include <vector>

SortTrace insertionSort(const std::vector<SortItem>& items) {
    SortTrace sortTrace;

    // Sorting works on a copy so callers keep their original input unchanged.
    std::vector<SortItem> workingItems = items;

    for (std::size_t i = 1; i < workingItems.size(); ++i) {
        SortItem keyItem = workingItems[i];
        std::size_t insertionIndex = i;

        // First find where keyItem belongs. During this scan the item is still
        // physically at index i, so CompareEvent can use i as the key index.
        while (insertionIndex > 0) {
            sortTrace.events.push_back(CompareEvent{
                static_cast<int>(insertionIndex - 1),
                static_cast<int>(i)
            });
            sortTrace.stats.comparisons += 1;

            if (workingItems[insertionIndex - 1].value <= keyItem.value) {
                break;
            }

            --insertionIndex;
        }

        // Shift larger items one slot to the right, then place keyItem in the
        // gap. These are moves rather than swaps because only one item is
        // assigned to each destination.
        for (std::size_t position = i; position > insertionIndex; --position) {
            sortTrace.events.push_back(MoveEvent{
                static_cast<int>(position),
                workingItems[position - 1]
            });
            sortTrace.stats.moves += 1;

            workingItems[position] = workingItems[position - 1];
        }

        if (insertionIndex != i) {
            sortTrace.events.push_back(MoveEvent{
                static_cast<int>(insertionIndex),
                keyItem
            });
            sortTrace.stats.moves += 1;

            workingItems[insertionIndex] = keyItem;
        }
    }

    sortTrace.finalItems = workingItems;
    return sortTrace;
}
