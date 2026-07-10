#include "SortRunner.hpp"

#include <stdexcept>

// These algorithm functions are implemented in their own .cpp files.
// They are intentionally not exposed in SortRunner.hpp because most callers only need runSort.
SortTrace bubbleSort(const std::vector<SortItem>& items);

SortTrace insertionSort(const std::vector<SortItem>& items);

SortTrace selectionSort(const std::vector<SortItem>& items);

SortTrace runSort(
    Algorithm algorithm,
    const std::vector<SortItem>& items)
{
    switch (algorithm) {
    case Algorithm::Bubble:
        return bubbleSort(items);
        
    case Algorithm::Insertion:
        return insertionSort(items);

    case Algorithm::Selection:
        return selectionSort(items);
    }

    throw std::invalid_argument("Unknown algorithm.");
}
