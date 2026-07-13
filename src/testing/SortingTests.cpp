#include "TestSupport.hpp"

#include "domain/Algorithm.hpp"
#include "sorting/SortRunner.hpp"

#include <cstddef>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace {

    // =================================================================================
    // Universal sorting checks
    // =================================================================================

    // Event structs store indices as int, while vector sizes use std::size_t.
    // Check for negative values before casting so signed/unsigned conversion
    // cannot turn a negative index into a huge positive one.
    bool isValidIndex(int index, std::size_t itemCount)
    {
        if (index < 0) {
            return false;
        }

        return static_cast<std::size_t>(index) < itemCount;
    }

    // Sorting output must be nondecreasing by value.
    TestResult checkSortedAscending(const std::vector<SortItem>& items)
    {
        for (std::size_t index = 0; index + 1 < items.size(); ++index) {
            if (items[index].value > items[index + 1].value) {
                return {false, "The finalized list has at least one value out of order."};
            }
        }
        return {true, ""};
    }

    // Sorting may reorder items, but it must not lose, duplicate, or corrupt them.
    // SortItem::id is treated as stable identity, and value must stay attached to that id.
    TestResult checkSameItems(
        const std::vector<SortItem>& initialItems,
        const std::vector<SortItem>& finalItems)
    {
        if (finalItems.size() != initialItems.size()) {
            return {false, "The initial and final items vectors have different sizes."};
        }
        std::unordered_map<int, int> originalValueById;

        for (const SortItem& item : initialItems) {
            originalValueById[item.id] = item.value;
        }

        for (const SortItem& item : finalItems) {
            auto foundItem = originalValueById.find(item.id);

            if (foundItem == originalValueById.end()) {
                return {false, "The final items contain an unknown or duplicated id."};
            }

            if (foundItem->second != item.value) {
                return {false, "At least one item id is now paired with a different value."};
            }

            originalValueById.erase(foundItem);
        }
        if (!originalValueById.empty()) {
            return {false, "At least one original item was missing from the final items."};
        }

        return {true, ""};

    }

    // Algorithms sort a local copy. The caller's original input vector should
    // still have the same items in the same order after runSort returns.
    TestResult checkInputUnchanged(
        const std::vector<SortItem>& beforeRun,
        const std::vector<SortItem>& afterRun)
    {
        if (beforeRun.size() != afterRun.size()) {
            return {false, "The input vector size changed during sorting."};
        }

        for (std::size_t index = 0; index < beforeRun.size(); ++index) {
            if (beforeRun[index].value != afterRun[index].value) {
                return {false, "At least one input value changed during sorting."};
            }

            if (beforeRun[index].id != afterRun[index].id) {
                return {false, "At least one input id changed during sorting."};
            }
        }

        return {true, ""};
    }

    // SortStats should agree with the event stream. MarkSortedEvent is ignored
    // because SortStats does not currently track a sorted-mark count.
    TestResult checkStatsMatchEvents(const SortTrace& trace)
    {
        int comparisonCount = 0;
        int swapCount = 0;
        int moveCount = 0;

        for (const SortEvent& event : trace.events) {
            if (std::holds_alternative<CompareEvent>(event)) {
                comparisonCount += 1;
            }
            if (std::holds_alternative<SwapEvent>(event)) {
                swapCount += 1;
            }
            if (std::holds_alternative<MoveEvent>(event)) {
                moveCount += 1;
            }
        }

        if (comparisonCount != trace.stats.comparisons) {
            return {false, "comparisonCount does not match comparisons in trace.stats."};
        }
        if (swapCount != trace.stats.swaps) {
            return {false, "swapCount does not match swaps in trace.stats."};
        }
        if (moveCount != trace.stats.moves) {
            return {false, "moveCount does not match moves in trace.stats."};
        }

        return {true, ""};
    }

    TestResult checkEventsReplayToFinalItems(
        const std::vector<SortItem>& initialItems,
        const SortTrace& trace)
    {
        // Replay starts from the same input the algorithm received.
        // The event stream should contain enough information to transform this
        // copy into trace.finalItems without knowing which algorithm produced it.
        std::vector<SortItem> replayItems = initialItems;

        for (const SortEvent& event : trace.events) {
            // CompareEvent is observational: it says two positions were compared,
            // but it does not change item order.
            //
            // MarkSortedEvent is also non-mutating for replay. It will matter to
            // animation state later, but it should not move or replace items.
            if (std::holds_alternative<CompareEvent>(event) ||
                std::holds_alternative<MarkSortedEvent>(event))
            {
                continue;
            }

            if (std::holds_alternative<SwapEvent>(event)) {
                const SwapEvent& swapEvent = std::get<SwapEvent>(event);

                if (!isValidIndex(swapEvent.leftIndex, replayItems.size()) ||
                    !isValidIndex(swapEvent.rightIndex, replayItems.size()))
                {
                    return {false, "SwapEvent could not be replayed because it used an invalid index."};
                }

                // SwapEvent mutates replay state by exchanging the two current
                // positions. The stored event indices are int, so cast after
                // validating that they are nonnegative and inside the vector.
                std::swap(
                    replayItems[static_cast<std::size_t>(swapEvent.leftIndex)],
                    replayItems[static_cast<std::size_t>(swapEvent.rightIndex)]
                );
            }
            if (std::holds_alternative<MoveEvent>(event)) {
                const MoveEvent& moveEvent = std::get<MoveEvent>(event);

                if (!isValidIndex(moveEvent.destinationIndex, replayItems.size())) {
                    return {false, "MoveEvent could not be replayed because it used an invalid index."};
                }

                // MoveEvent means "place this exact SortItem at destinationIndex."
                // It replaces an existing vector element; it does not push a new
                // item onto the end.
                replayItems[static_cast<std::size_t>(moveEvent.destinationIndex)] = moveEvent.movedItem;
            }
        }

        if (replayItems.size() != trace.finalItems.size()) {
            return {false, "Replay produced a different item count than trace.finalItems."};
        }

        // If the event contract is complete, replaying events from the initial
        // input should produce exactly the same items as the algorithm's final
        // output, including both value and stable id.
        for (std::size_t itemIndex = 0; itemIndex < replayItems.size(); ++itemIndex) {
            if (replayItems[itemIndex].value != trace.finalItems[itemIndex].value ||
                replayItems[itemIndex].id != trace.finalItems[itemIndex].id)
            {
                return {false, "Replay of SortEvents did not produce the same final result."};
            }
        }

        return {true, ""};
    }
    // Every event index must refer to an item that exists in the sorted vector.
    // This protects the future animation layer from out-of-bounds event replay.
    TestResult checkEventIndicesValid(
        const SortTrace& trace,
        std::size_t itemCount)
    {
        for (const SortEvent& event : trace.events) {
            if (std::holds_alternative<CompareEvent>(event)) {
                const CompareEvent& compareEvent = std::get<CompareEvent>(event);

                if (!isValidIndex(compareEvent.leftIndex, itemCount) ||
                    !isValidIndex(compareEvent.rightIndex, itemCount))
                {
                    return {false, "Comparison event recorded out of bounds indices."};
                }
            }
            if (std::holds_alternative<SwapEvent>(event)) {
                const SwapEvent& swapEvent = std::get<SwapEvent>(event);

                if (!isValidIndex(swapEvent.leftIndex, itemCount) ||
                    !isValidIndex(swapEvent.rightIndex, itemCount))
                {
                    return {false, "Swap event recorded out of bounds indices."};
                }

            }
            if (std::holds_alternative<MoveEvent>(event)) {
                const MoveEvent& moveEvent = std::get<MoveEvent>(event);

                if (!isValidIndex(moveEvent.destinationIndex, itemCount))
                {
                    return {false, "Move event recorded out of bounds index."};
                }

            }
            if (std::holds_alternative<MarkSortedEvent>(event)) {
                const MarkSortedEvent& markSortedEvent = std::get<MarkSortedEvent>(event);

                if (!isValidIndex(markSortedEvent.index, itemCount))
                {
                    return {false, "Mark sorted event recorded out of bounds index."};
                }
            }
        }

        return {true, ""};
    }

    // =================================================================================
    // Diagnostic sorting checks
    // =================================================================================

    // Diagnostic sorting checks describe useful algorithm properties that are not
    // required of every valid sorting algorithm.
    //
    // Stability belongs here because some algorithms are naturally stable
    // (bubble sort, insertion sort) while others are naturally unstable
    // (selection sort). The same stability-checking helper can still be used for
    // all algorithms, but runSortCase should decide whether to record that result
    // as required, diagnostic, or skip it for a particular algorithm.

    TestResult checkStableForEqualValues(
        const std::vector<SortItem>& initialItems,
        const std::vector<SortItem>& finalItems)
    {
        if (initialItems.size() != finalItems.size()) {
            return {false, "Initial and final items did not have the same size."};
        }

        std::unordered_map<int, std::vector<int>> initialIdsByValue;
        std::unordered_map<int, std::vector<int>> finalIdsByValue;


        for (const SortItem& item : initialItems) {
            initialIdsByValue[item.value].push_back(item.id);
        }

        for (const SortItem& item : finalItems) {
            finalIdsByValue[item.value].push_back(item.id);
        }

        if (initialIdsByValue.size() != finalIdsByValue.size()) {
            return {false, "The initial and final idsByValue maps did not have the same size."};
        }

        for (const auto& entry : initialIdsByValue) {
            int value = entry.first;
            const std::vector<int>& initialIds = entry.second;

            auto foundFinalEntry = finalIdsByValue.find(value);

            if (foundFinalEntry == finalIdsByValue.end()) {
                return {false, "Value was missing from finalIdsByValue."};
            }
            if (foundFinalEntry->second != initialIds) {
                return {false, "The algorithm is NOT stable for duplicate values."};
            }
        }

        return {true, ""};
    }

    // This is the policy layer for stability.
    //
    // checkStableForEqualValues only answers "was this output stable?"
    // This function answers "does this algorithm promise stable output?"
    //
    // Keeping those questions separate keeps the check reusable. Selection sort
    // can still be measured diagnostically without pretending stability is part
    // of its required contract.
    bool algorithmRequiresStableOutput(Algorithm algorithm)
    {
        switch (algorithm) {
            case Algorithm::Bubble:
            case Algorithm::Insertion:
                return true;

            case Algorithm::Selection:
                return false;
        }

        return false;
    }


    // =================================================================================
    // Test case runners
    // =================================================================================

    struct SortTestCase {
        const char* name;
        std::vector<SortItem> input;
    };

    const char* algorithmSuiteName(Algorithm algorithm)
    {
        switch (algorithm) {
            case Algorithm::Bubble:
                return "sorting/bubble";

            case Algorithm::Insertion:
                return "sorting/insertion";

            case Algorithm::Selection:
                return "sorting/selection";
        }

        return "sorting/unknown";
    }

    void runSortCase(
        Algorithm algorithm,
        const std::vector<SortItem>& input,
        TestReport& report,
        const char* caseName)
    {
        std::vector<SortItem> inputCopy = input;
        SortTrace trace = runSort(algorithm, input);
        const char* suiteName = algorithmSuiteName(algorithm);

        recordRequired(report, suiteName, caseName, "checkSortedAscending", checkSortedAscending(trace.finalItems));
        recordRequired(report, suiteName, caseName, "checkInputUnchanged", checkInputUnchanged(inputCopy, input));
        recordRequired(report, suiteName, caseName, "checkSameItems", checkSameItems(inputCopy, trace.finalItems));
        recordRequired(report, suiteName, caseName, "checkStatsMatchEvents", checkStatsMatchEvents(trace));
        recordRequired(report, suiteName, caseName, "checkEventIndicesValid", checkEventIndicesValid(trace, input.size()));
        recordRequired(report, suiteName, caseName, "checkEventsReplayToFinalItems", checkEventsReplayToFinalItems(input, trace));

        TestResult stabilityResult = checkStableForEqualValues(inputCopy, trace.finalItems);

        if (algorithmRequiresStableOutput(algorithm)) {
            recordRequired(report, suiteName, caseName, "checkStableForEqualValues", stabilityResult);
        }
        else {
            recordDiagnostic(report, suiteName, caseName, "checkStableForEqualValues", stabilityResult);
        }
    }
}

TestReport runSortingTests()
{
    TestReport report;

    const std::vector<Algorithm> algorithms{
        Algorithm::Bubble,
        Algorithm::Insertion,
        Algorithm::Selection
    };

    // Handwritten cases keep sorting tests independent from input generation.
    const std::vector<SortTestCase> cases{
        {"empty", {}},
        {"single item", {{42, 0}}},
        {"already sorted", {{1, 0}, {2, 1}, {3, 2}, {4, 3}}},
        {"reverse sorted", {{4, 0}, {3, 1}, {2, 2}, {1, 3}}},
        {"duplicates", {{2, 0}, {1, 1}, {2, 2}, {1, 3}}},
        {"all equal", {{5, 0}, {5, 1}, {5, 2}, {5, 3}}},
        {"mixed signs", {{0, 0}, {-3, 1}, {7, 2}, {-1, 3}, {7, 4}}}
    };

    for (Algorithm algorithm : algorithms) {
        for (const SortTestCase& testCase : cases) {
            runSortCase(algorithm, testCase.input, report, testCase.name);
        }
    }

    return report;
}
