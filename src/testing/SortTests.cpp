#include "SortTests.hpp"

#include <cstddef>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "animation/AnimationPlayer.hpp"
#include "domain/SortItem.hpp"
#include "domain/SortSpec.hpp"
#include "input/InputGenerator.hpp"
#include "sorting/SortRunner.hpp"
#include "app/VisualizerSession.hpp"

namespace {

    // =================================================================================
    // Shared test helpers
    // =================================================================================

    // Result from one individual check.
    // The message is stored now so a reporting function can print useful
    // failure details later without changing every check function.
    struct TestResult {
        bool passed;
        const char* message;
    };

    // Recording helpers translate individual TestResults into the accumulated
    // public TestReport without making each check know its own severity or
    // caller context.
    void recordRequired(
        TestReport& report,
        const char* suiteName,
        const char* caseName,
        const char* checkName,
        const TestResult& result)
    {
        if (result.passed) {
            report.summary.requiredPassed += 1;
        } 
        else {
            report.summary.requiredFailed += 1;
            report.failures.push_back(
                TestFailure{
                    TestSeverity::Required,
                    suiteName,
                    caseName,
                    checkName,
                    result.message
                }
            );
        }
    }

    void recordDiagnostic(
        TestReport& report,
        const char* suiteName,
        const char* caseName,
        const char* checkName,
        const TestResult& result)
    {
        if (result.passed) {
            report.summary.diagnosticPassed += 1;
        }
        else {
            report.summary.diagnosticFailed += 1;
            report.failures.push_back(
                TestFailure{
                    TestSeverity::Diagnostic,
                    suiteName,
                    caseName,
                    checkName,
                    result.message
                }
            );
        }
    }

    void addReport(
        TestReport& total,
        const TestReport& addition)
    {
        total.summary.requiredPassed += addition.summary.requiredPassed;
        total.summary.requiredFailed += addition.summary.requiredFailed;
        total.summary.diagnosticPassed += addition.summary.diagnosticPassed;
        total.summary.diagnosticFailed += addition.summary.diagnosticFailed;

        total.failures.insert(
            total.failures.end(),
            addition.failures.begin(),
            addition.failures.end());
    }

    // Counts how many times each integer value appears in the generated input.
    //
    // The key is the SortItem value.
    // The mapped value is the number of times that value appeared.
    //
    // Example:
    // item values: 4, 7, 4
    // occurrences[4] == 2
    // occurrences[7] == 1
    std::unordered_map<int, int> countValueOccurrences(
        const std::vector<SortItem>& items)
    {
        std::unordered_map<int, int> occurrences;

        for (const SortItem& item : items) {
            occurrences[item.value] += 1;
        }

        return occurrences;
    }

    // =================================================================================
    // Universal input checks
    // =================================================================================

    // Universal checks are run for every generated input, regardless of ValueSpec
    // or InitialOrderSpec.

    // This universal test makes sure the items list has exactly itemCount values.
    TestResult checkItemCount(
        const SortInputSpec& inputSpec,
        const std::vector<SortItem>& items)
    {
        if (inputSpec.itemCount > items.size())
        {
            return {false, "Less values were input than specified by itemCount."};
        } 

        if (inputSpec.itemCount < items.size())
        {
            return {false, "More values were input than specified by itemCount."};
        }

        return {true, ""};
    }

    // This universal test makes sure ids are exactly 0 -> itemCount - 1.
    // This one check covers both ID rules:
    // - every expected id exists
    // - no id appears more than once
    //
    // If the generated ids are exactly 0, 1, 2, ..., itemCount - 1, then they
    // are automatically unique and complete.
    TestResult checkIdsValid(
        const SortInputSpec& inputSpec,
        const std::vector<SortItem>& items)
    {
        std::unordered_map<int, int> idOccurrences;

        for (const SortItem& item : items) {
            if (item.id < 0) {
                return {false, "At least one id was outside the valid range 0..itemCount - 1."};
            }

            // item.id is signed because SortItem stores ids as int.
            // inputSpec.itemCount is unsigned because negative item counts are impossible.
            //
            // We check item.id < 0 first. After that, it is safe to cast item.id
            // to unsigned int for comparison with itemCount.
            //
            // This avoids casting itemCount down to int, which would be a weaker
            // habit if itemCount ever became larger than int can represent.
            if (static_cast<unsigned int>(item.id) >= inputSpec.itemCount) {
                return {false, "At least one id was outside the valid range 0..itemCount - 1."};
            }

            idOccurrences[item.id] += 1;
        }

        for (unsigned int id = 0; id < inputSpec.itemCount; ++id) {
            auto foundId = idOccurrences.find(static_cast<int>(id));

            if (foundId == idOccurrences.end()) {
                return {false, "At least one expected id was missing."};
            }

            if (foundId->second != 1) {
                return {false, "At least one id was found to be non-unique."};
            }
        }

        return {true, ""};
    }

    // This universal test creates an identical generated input from the inputSpec and checks
    // that the same seed gives the same random result.
    TestResult checkDeterministicGeneration(
        const SortInputSpec& inputSpec)
    {
        std::vector<SortItem> firstInput = generateInput(inputSpec);
        std::vector<SortItem> secondInput = generateInput(inputSpec);

        if (firstInput.size() != secondInput.size()) {
            return {false, "Same inputSpec produced vectors with different sizes."};
        }

        for (std::size_t index = 0; index < firstInput.size(); ++index) {
            if (firstInput[index].value != secondInput[index].value)
            {
                return {false, "Same inputSpec produced vectors with at least 1 differing value."};
            }
            if (firstInput[index].id != secondInput[index].id) {
                return {false, "Same inputSpec produced vectors with at least 1 differing id."};
            }
        }

        return {true, ""};
    }

    // =================================================================================
    // ValueSpec checks
    // =================================================================================

    // The checkValueSpec overloads verify the rules implied by each ValueSpec type.
    // They do not check universal rules such as item count or id validity.

    // Checks that for PermutationValueSpec, the values are numbered from 1 to itemCount and are unique.
    TestResult checkValueSpec(
        const PermutationValueSpec&,
        const SortInputSpec& inputSpec,
        const std::vector<SortItem>& items)
    {
        std::unordered_map<int, int> occurrences = countValueOccurrences(items);

        for (unsigned int value = 1; value <= inputSpec.itemCount; ++value) {
            // .find() searches for a key without modifying the unordered_map.
            // This is different from occurrences[key], which creates the key with
            // a default value if it does not already exist.
            auto foundValue = occurrences.find(static_cast<int>(value));

            if (foundValue == occurrences.end()) {
                return {false, "PermutationValueSpec did not produce every value from 1 to itemCount."};
            }

            // foundValue->second is the occurrence count for this value.
            if (foundValue->second != 1) {
                return {false, "PermutationValueSpec did not produce each value exactly once."};
            }
        }

        return {true, ""};
    }

    // Checks that for RangeValueSpec, the values are within the min and max, and if RequireUnique
    // it makes sure no duplicates are found.
    TestResult checkValueSpec(
        const RangeValueSpec& valueSpec,
        const SortInputSpec&,
        const std::vector<SortItem>& items)
    {
        for (const SortItem& item : items) {
            if (item.value < valueSpec.minValue || item.value > valueSpec.maxValue) {
                return {false, "RangeValueSpec did not produce values in the range [minValue, maxValue]."};
            }
        }
        if (valueSpec.duplicatePolicy == DuplicatePolicy::RequireUnique) {
            std::unordered_map<int, int> occurrences = countValueOccurrences(items);

            for (const auto& occurrence : occurrences) {
                if (occurrence.second != 1) {
                    return {false, "RangeValueSpec had DuplicatePolicy RequireUnique, but at least one duplicate was found."};
                }
            }
        }

        return {true, ""};
    }

    // Checks that for AllEqualValueSpec, every value equals valueSpec.value
    TestResult checkValueSpec(
        const AllEqualValueSpec& valueSpec,
        const SortInputSpec&,
        const std::vector<SortItem>& items)
    {
        // Use a range-based loop because this test does not need the item index.
        // It says the intent directly: inspect every generated item.
        for (const SortItem& item : items) {
            if (item.value != valueSpec.value)
            {
                return {false, "AllEqualValueSpec did not produce all values equal to valueSpec.value"};
            }
        }

        return {true, ""};
    }

    // Checks that for FewUniqueValueSpec, the number of unique values found is equal to uniqueValueCount,
    // and that all values are in the range [minValue, maxValue]
    TestResult checkValueSpec(
        const FewUniqueValueSpec& valueSpec,
        const SortInputSpec&,
        const std::vector<SortItem>& items)
    {
        std::unordered_map<int, int> occurrences = countValueOccurrences(items);

        if (occurrences.size() != static_cast<std::size_t>(valueSpec.uniqueValueCount)) {
            return {false, "FewUniqueValueSpec did not produce the requested number of unique values."};
        }

        for (const auto& occurrence : occurrences) {
            if (occurrence.first < valueSpec.minValue || occurrence.first > valueSpec.maxValue) {
                return {false, "FewUniqueValueSpec did not produce values in the range [minValue, maxValue]"};
            }
        }

        return {true, ""};
    }

    TestResult checkValueSpec(
        const PeriodicValueSpec& valueSpec,
        const SortInputSpec& inputSpec,
        const std::vector<SortItem>& items)
    {
        if (valueSpec.periodLength <= 0) {
            return {false, "PeriodicValueSpec had periodLength <= 0."};
        }

        std::unordered_map<int, int> occurrences = countValueOccurrences(items);

        // PeriodicValueSpec should only produce values 1, 2, ..., periodLength.
        // This checks the allowed value range without assuming every possible value appears.
        // If itemCount is smaller than periodLength, some valid values will not appear.
        for (const auto& occurrence : occurrences) {
            if (occurrence.first < 1 || occurrence.first > valueSpec.periodLength) {
                return {false, "PeriodicValueSpec produced a value outside 1..periodLength."};
            }
        }

        unsigned int periodLength = static_cast<unsigned int>(valueSpec.periodLength);

        // Only values up to this limit can have a nonzero expected count.
        //
        // If itemCount < periodLength, the generated pattern is only a partial
        // period. Example: itemCount = 3 and periodLength = 5 gives values
        // 1, 2, 3. Values 4 and 5 are valid periodic values in general, but
        // they should not appear in this particular generated input.
        //
        // If itemCount >= periodLength, every value in 1..periodLength should
        // appear at least once, so the limit is periodLength.
        unsigned int expectedValueLimit = periodLength;
        if (inputSpec.itemCount < periodLength) {
            expectedValueLimit = inputSpec.itemCount;
        }

        // Integer division gives the number of complete periods that fit.
        // Example: itemCount = 10, periodLength = 3 gives fullPeriods = 3.
        unsigned int fullPeriods = inputSpec.itemCount / periodLength;

        // The remainder gives the number of values in the partial final period.
        // Example: itemCount = 10, periodLength = 3 gives remainder = 1,
        // so value 1 appears one extra time.
        unsigned int remainder = inputSpec.itemCount % periodLength;

        for (const auto& occurrence : occurrences) {
            // Values above expectedValueLimit should have expected count 0.
            // Checking this directly avoids looping through a huge periodLength
            // when itemCount is small.
            if (static_cast<unsigned int>(occurrence.first) > expectedValueLimit) {
                return {false, "PeriodicValueSpec produced a value that should not appear for this itemCount."};
            }
        }

        // Check only values with nonzero expected counts.
        // Missing values with expected count 0 do not need to be checked.
        for (unsigned int value = 1; value <= expectedValueLimit; ++value) {
            unsigned int expectedCount = fullPeriods;

            if (value <= remainder) {
                expectedCount += 1;
            }

            // If a value is absent from the map, it appeared zero times.
            // This matters when itemCount is smaller than periodLength.
            auto foundValue = occurrences.find(static_cast<int>(value));
            int actualCount = 0;

            if (foundValue != occurrences.end()) {
                actualCount = foundValue->second;
            }

            if (actualCount != static_cast<int>(expectedCount)) {
                return {false, "PeriodicValueSpec produced an unexpected occurrence count."};
            }
        }

        return {true, ""};
    }

    // =================================================================================
    // InitialOrderSpec checks
    // =================================================================================

    // The checkInitialOrderSpec overloads verify the final arrangement of the
    // generated values. These belong separately from ValueSpec checks because
    // ValueSpec says what values exist, while InitialOrderSpec says how they are arranged.

    // RandomOrderSpec intentionally does not try to prove the output "looks random."
    // Random-looking tests are fragile and can fail for valid random outputs.
    // The important input-generator property, deterministic output for a fixed seed,
    // is covered by checkDeterministicGeneration.
    TestResult checkInitialOrderSpec(
        const RandomOrderSpec&,
        const SortInputSpec&,
        const std::vector<SortItem>&)
    {
        return {true, ""};
    }

    // Checks that for the AscendingOrderSpec, the values are nondecreasing.
    TestResult checkInitialOrderSpec(
        const AscendingOrderSpec&,
        const SortInputSpec&,
        const std::vector<SortItem>& items)
    {
        for (std::size_t index = 1; index < items.size(); ++index) {
            if (items[index - 1].value > items[index].value) {
                return {false, "AscendingOrderSpec failed to produce a nondecreasing output."};
            }
        }

        return {true, ""};
    }

    // Checks that for the DescendingOrderSpec, the values are nonincreasing.
    TestResult checkInitialOrderSpec(
        const DescendingOrderSpec&,
        const SortInputSpec&,
        const std::vector<SortItem>& items)
    {
        for (std::size_t index = 1; index < items.size(); ++index) {
            if (items[index - 1].value < items[index].value) {
                return {false, "DescendingOrderSpec failed to produce a nonincreasing output."};
            }
        }

        return {true, ""};
    }

    // This mirrors the current input-generator contract:
    // disorderFraction controls a number of random swaps, not an exact count of
    // out-of-order elements. The strict check is only possible for 0.0.
    TestResult checkInitialOrderSpec(
        const NearlyAscendingOrderSpec& orderSpec,
        const SortInputSpec&,
        const std::vector<SortItem>& items)
    {
        if (orderSpec.disorderFraction > 1.0 || orderSpec.disorderFraction < 0.0) {
            return {false, "NearlyAscendingOrderSpec.disorderFraction was not in the range ([0.0, 1.0])"};
        }

        if (orderSpec.disorderFraction == 0.0) {
            for (std::size_t index = 1; index < items.size(); ++index) {
                if (items[index - 1].value > items[index].value) {
                    return {false, "NearlyAscendingOrderSpec had disorderFraction = 0.0, but did not create an ascending input."};
                }
            }
        }

        return {true, ""};
    }

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
    // Required animation tests
    // =================================================================================

    // These helpers are animation-specific, so they live near the animation
    // tests instead of in the shared helper section at the top of the file.
    //
    // If another test section later needs the same checks, that will be the
    // signal to move them upward into "Shared test helpers."

    // Checks that a SortState contains the expected SortItems in the expected order.
    //
    // This hides the nested access pattern:
    // state.items[index].item.value
    // state.items[index].item.id
    //
    // That keeps animation tests focused on behavior instead of SortState's
    // internal representation.
    TestResult checkStateItems(
        const SortState& state,
        const std::vector<SortItem>& expectedItems)
    {
        if (expectedItems.size() != state.items.size()) {
            return {false, "Expected items size differs from state items size."};
        }
        for (std::size_t index = 0; index < expectedItems.size(); ++index) {
            if (expectedItems[index].value != state.items[index].item.value ||
                expectedItems[index].id != state.items[index].item.id) 
            {
                return {false, "At least one expected item differs from the items in state."};
            }
        }

        return {true, ""};
    }

    // Checks that a SortState contains the expected visual state at each index.
    //
    // Animation tests should use this instead of manually reaching through
    // state.items[index].visualState in every test case.
    TestResult checkVisualStates(
        const SortState& state,
        const std::vector<ItemVisualState>& expectedVisualStates)
    {
        if (expectedVisualStates.size() != state.items.size()) {
            return {false, "Expected visual states and state.items have different sizes."};
        }
        for (std::size_t index = 0; index < expectedVisualStates.size(); ++index) {
            if (expectedVisualStates[index] != state.items[index].visualState) {
                return {false, "At least one visual state differs from expected in SortState."};
            }
        }

        return {true, ""};
    }

    // Checks the SortState completion flag.
    //
    // This is intentionally tiny, but it gives completion checks the same
    // TestResult shape as the larger animation checks.
    TestResult checkComplete(
        const SortState& state,
        bool expectedComplete)
    {
        if (state.complete != expectedComplete) {
            return {false, "State completion status differs from expected completion status."};
        }

        return {true, ""};
    }

    TestResult checkEventPosition(
        const AnimationPlayer& player,
        std::size_t expectedEventPosition)
    {
        if (player.currentEventPosition() != expectedEventPosition) {
            return {false, "AnimationPlayer current event position differs from expected."};
        }

        return {true, ""};
    }

    TestResult checkEventCount(
        const AnimationPlayer& player,
        std::size_t expectedEventCount)
    {
        if (player.eventCount() != expectedEventCount) {
            return {false, "AnimationPlayer event count differs from expected."};
        }

        return {true, ""};
    }

    // Each animation test function records a small group of required checks.
    //
    // These are void functions because one animation scenario usually needs to
    // check item order, visual states, and completion together. Returning one
    // TestResult would either hide which part failed or force the test to stop
    // after the first failed check.

    void runAnimationLoadWithNoEventsTest(TestReport& report)
    {
        std::vector<SortItem> initialItems{{3, 0}, {1, 1}, {2, 2}};
        std::vector<SortEvent> events = {};

        AnimationPlayer player;

        player.load(initialItems, events);
        const SortState& state = player.currentState();

        std::vector<ItemVisualState> expectedVisualStates{
            ItemVisualState::Normal,
            ItemVisualState::Normal,
            ItemVisualState::Normal
        };

        recordRequired(
            report,
            "animation",
            "load with no events",
            "checkStateItems",
            checkStateItems(state, initialItems)
        );

        recordRequired(
            report,
            "animation",
            "load with no events",
            "checkVisualStates",
            checkVisualStates(state, expectedVisualStates)
        );

        recordRequired(
            report,
            "animation",
            "load with no events",
            "checkComplete",
            checkComplete(state, true)
        );
    }

    void runAnimationLoadWithPendingEventTest(TestReport& report)
    {
        std::vector<SortItem> initialItems{{3, 0}, {1, 1}, {2, 2}};
        std::vector<SortEvent> events{
            CompareEvent{0, 1}
        };

        AnimationPlayer player;
        
        player.load(initialItems, events);
        const SortState& state = player.currentState();

        std::vector<ItemVisualState> expectedVisualStates{
            ItemVisualState::Normal,
            ItemVisualState::Normal,
            ItemVisualState::Normal
        };

        recordRequired(
            report,
            "animation",
            "load with one event",
            "checkStateItems",
            checkStateItems(state, initialItems)
        );

        recordRequired(
            report,
            "animation",
            "load with one event",
            "checkVisualStates",
            checkVisualStates(state, expectedVisualStates)
        );

        recordRequired(
            report,
            "animation",
            "load with one event",
            "checkComplete",
            checkComplete(state, false)
        );
    }

    void runAnimationCompareEventTest(TestReport& report)
    {
        std::vector<SortItem> initialItems{{3, 0}, {1, 1}, {2, 2}};
        std::vector<SortEvent> events{
            CompareEvent{0, 1}
        };

        AnimationPlayer player;
        
        player.load(initialItems, events);
        player.stepForward();
        const SortState& state = player.currentState();

        std::vector<ItemVisualState> expectedVisualStates{
            ItemVisualState::Comparing,
            ItemVisualState::Comparing,
            ItemVisualState::Normal
        };

        recordRequired(
            report,
            "animation",
            "compare event",
            "checkStateItems",
            checkStateItems(state, initialItems)
        );

        recordRequired(
            report,
            "animation",
            "compare event",
            "checkVisualStates",
            checkVisualStates(state, expectedVisualStates)
        );

        recordRequired(
            report,
            "animation",
            "compare event",
            "checkComplete",
            checkComplete(state, true)
        );
    }

    void runAnimationSwapEventTest(TestReport& report)
    {
        std::vector<SortItem> initialItems{{3, 0}, {1, 1}, {2, 2}};
        std::vector<SortEvent> events{
            SwapEvent{0, 1}
        };

        AnimationPlayer player;
        
        player.load(initialItems, events);
        player.stepForward();
        const SortState& state = player.currentState();

        std::vector<ItemVisualState> expectedVisualStates{
            ItemVisualState::Swapping,
            ItemVisualState::Swapping,
            ItemVisualState::Normal
        };

        std::vector<SortItem> expectedItems{{1, 1}, {3, 0}, {2, 2}};

        recordRequired(
            report,
            "animation",
            "swap event",
            "checkStateItems",
            checkStateItems(state, expectedItems)
        );

        recordRequired(
            report,
            "animation",
            "swap event",
            "checkVisualStates",
            checkVisualStates(state, expectedVisualStates)
        );

        recordRequired(
            report,
            "animation",
            "swap event",
            "checkComplete",
            checkComplete(state, true)
        );
    }

    void runAnimationMoveEventTest(TestReport& report)
    {
        std::vector<SortItem> initialItems{{3, 0}, {1, 1}, {2, 2}};
        std::vector<SortEvent> events{
            MoveEvent{0, initialItems[1]}
        };

        AnimationPlayer player;
        
        player.load(initialItems, events);
        player.stepForward();
        const SortState& state = player.currentState();

        std::vector<ItemVisualState> expectedVisualStates{
            ItemVisualState::Moving,
            ItemVisualState::Normal,
            ItemVisualState::Normal
        };

        std::vector<SortItem> expectedItems{{1, 1}, {1, 1}, {2, 2}};

        recordRequired(
            report,
            "animation",
            "move event",
            "checkStateItems",
            checkStateItems(state, expectedItems)
        );

        recordRequired(
            report,
            "animation",
            "move event",
            "checkVisualStates",
            checkVisualStates(state, expectedVisualStates)
        );

        recordRequired(
            report,
            "animation",
            "move event",
            "checkComplete",
            checkComplete(state, true)
        );
    }

    void runAnimationMarkSortedPersistsTest(TestReport& report)
    {
        std::vector<SortItem> initialItems{{3, 0}, {1, 1}, {2, 2}};
        std::vector<SortEvent> events{
            MarkSortedEvent{1},
            CompareEvent{0, 2}
        };

        AnimationPlayer player;

        player.load(initialItems, events);
        player.stepForward();

        const SortState& afterMarkSorted = player.currentState();

        std::vector<ItemVisualState> expectedAfterMarkSorted{
            ItemVisualState::Normal,
            ItemVisualState::Sorted,
            ItemVisualState::Normal
        };

        recordRequired(
            report,
            "animation",
            "mark sorted persists",
            "checkStateItems after mark sorted",
            checkStateItems(afterMarkSorted, initialItems)
        );

        recordRequired(
            report,
            "animation",
            "mark sorted persists",
            "checkVisualStates after mark sorted",
            checkVisualStates(afterMarkSorted, expectedAfterMarkSorted)
        );

        recordRequired(
            report,
            "animation",
            "mark sorted persists",
            "checkComplete after mark sorted",
            checkComplete(afterMarkSorted, false)
        );

        player.stepForward();

        const SortState& afterCompare = player.currentState();

        std::vector<ItemVisualState> expectedAfterCompare{
            ItemVisualState::Comparing,
            ItemVisualState::Sorted,
            ItemVisualState::Comparing
        };

        recordRequired(
            report,
            "animation",
            "mark sorted persists",
            "checkStateItems after compare",
            checkStateItems(afterCompare, initialItems)
        );

        recordRequired(
            report,
            "animation",
            "mark sorted persists",
            "checkVisualStates after compare",
            checkVisualStates(afterCompare, expectedAfterCompare)
        );

        recordRequired(
            report,
            "animation",
            "mark sorted persists",
            "checkComplete after compare",
            checkComplete(afterCompare, true)
        );
    }

    void runAnimationStepAfterCompleteTest(TestReport& report)
    {
        std::vector<SortItem> initialItems{{3, 0}, {1, 1}, {2, 2}};
        std::vector<SortEvent> events{
            SwapEvent{0, 1}
        };

        AnimationPlayer player;
        
        player.load(initialItems, events);
        player.stepForward();

        std::vector<SortItem> expectedItems{{1, 1}, {3, 0}, {2, 2}};
        std::vector<ItemVisualState> expectedVisualStates{
            ItemVisualState::Swapping,
            ItemVisualState::Swapping,
            ItemVisualState::Normal,
        };

        const SortState& completedState = player.currentState();

        recordRequired(
            report,
            "animation",
            "step after complete",
            "checkStateItems before extra step",
            checkStateItems(completedState, expectedItems)
        );

        recordRequired(
            report,
            "animation",
            "step after complete",
            "checkVisualStates before extra step",
            checkVisualStates(completedState, expectedVisualStates)
        );

        recordRequired(
            report,
            "animation",
            "step after complete",
            "checkComplete before extra step",
            checkComplete(completedState, true)
        );

        player.stepForward();

        const SortState& afterExtraStep = player.currentState();

        recordRequired(
            report,
            "animation",
            "step after complete",
            "checkStateItems after extra step",
            checkStateItems(afterExtraStep, expectedItems)
        );

        recordRequired(
            report,
            "animation",
            "step after complete",
            "checkVisualStates after extra step",
            checkVisualStates(afterExtraStep, expectedVisualStates)
        );

        recordRequired(
            report,
            "animation",
            "step after complete",
            "checkComplete after extra step",
            checkComplete(afterExtraStep, true)
        );
    }

    void runAnimationEventPositionTest(TestReport& report)
    {
        std::vector<SortItem> initialItems{{3, 0}, {1, 1}, {2, 2}};
        std::vector<SortEvent> events{
            CompareEvent{0, 1},
            SwapEvent{0, 1}
        };

        AnimationPlayer player;

        player.load(initialItems, events);

        recordRequired(
            report,
            "animation",
            "event position",
            "checkEventCount after load",
            checkEventCount(player, 2)
        );

        recordRequired(
            report,
            "animation",
            "event position",
            "checkEventPosition after load",
            checkEventPosition(player, 0)
        );

        player.stepForward();

        recordRequired(
            report,
            "animation",
            "event position",
            "checkEventPosition after one step",
            checkEventPosition(player, 1)
        );

        player.stepForward();

        recordRequired(
            report,
            "animation",
            "event position",
            "checkEventPosition after completion",
            checkEventPosition(player, 2)
        );
    }

    void runAnimationStepBackwardAtBeginningTest(TestReport& report)
    {
        std::vector<SortItem> initialItems{{3, 0}, {1, 1}, {2, 2}};
        std::vector<SortEvent> events{
            CompareEvent{0, 1}
        };

        AnimationPlayer player;

        player.load(initialItems, events);
        player.stepBackward();

        const SortState& state = player.currentState();

        std::vector<ItemVisualState> expectedVisualStates{
            ItemVisualState::Normal,
            ItemVisualState::Normal,
            ItemVisualState::Normal
        };

        recordRequired(
            report,
            "animation",
            "step backward at beginning",
            "checkStateItems",
            checkStateItems(state, initialItems)
        );

        recordRequired(
            report,
            "animation",
            "step backward at beginning",
            "checkVisualStates",
            checkVisualStates(state, expectedVisualStates)
        );

        recordRequired(
            report,
            "animation",
            "step backward at beginning",
            "checkComplete",
            checkComplete(state, false)
        );

        recordRequired(
            report,
            "animation",
            "step backward at beginning",
            "checkEventPosition",
            checkEventPosition(player, 0)
        );
    }

    void runAnimationStepBackwardAfterMoveTest(TestReport& report)
    {
        std::vector<SortItem> initialItems{{3, 0}, {1, 1}, {2, 2}};
        std::vector<SortEvent> events{
            CompareEvent{0, 1},
            MoveEvent{0, initialItems[1]}
        };

        AnimationPlayer player;

        player.load(initialItems, events);
        player.stepForward();
        player.stepForward();
        player.stepBackward();

        const SortState& state = player.currentState();

        std::vector<ItemVisualState> expectedVisualStates{
            ItemVisualState::Comparing,
            ItemVisualState::Comparing,
            ItemVisualState::Normal
        };

        // This is the important replay-from-start behavior: stepping backward
        // from a MoveEvent does not need to know what item was overwritten.
        // It rebuilds the state after the previous event instead.
        recordRequired(
            report,
            "animation",
            "step backward after move",
            "checkStateItems",
            checkStateItems(state, initialItems)
        );

        recordRequired(
            report,
            "animation",
            "step backward after move",
            "checkVisualStates",
            checkVisualStates(state, expectedVisualStates)
        );

        recordRequired(
            report,
            "animation",
            "step backward after move",
            "checkComplete",
            checkComplete(state, false)
        );

        recordRequired(
            report,
            "animation",
            "step backward after move",
            "checkEventPosition",
            checkEventPosition(player, 1)
        );
    }

    void runAnimationSeekToEventPositionTest(TestReport& report)
    {
        std::vector<SortItem> initialItems{{3, 0}, {1, 1}, {2, 2}};
        std::vector<SortEvent> events{
            SwapEvent{0, 1},
            MarkSortedEvent{0},
            CompareEvent{0, 2}
        };

        AnimationPlayer player;

        player.load(initialItems, events);
        player.seekToEventPosition(2);

        const SortState& state = player.currentState();

        std::vector<SortItem> expectedItems{{1, 1}, {3, 0}, {2, 2}};
        std::vector<ItemVisualState> expectedVisualStates{
            ItemVisualState::Sorted,
            ItemVisualState::Normal,
            ItemVisualState::Normal
        };

        recordRequired(
            report,
            "animation",
            "seek to event position",
            "checkStateItems",
            checkStateItems(state, expectedItems)
        );

        recordRequired(
            report,
            "animation",
            "seek to event position",
            "checkVisualStates",
            checkVisualStates(state, expectedVisualStates)
        );

        recordRequired(
            report,
            "animation",
            "seek to event position",
            "checkComplete",
            checkComplete(state, false)
        );

        recordRequired(
            report,
            "animation",
            "seek to event position",
            "checkEventPosition",
            checkEventPosition(player, 2)
        );
    }

    void runAnimationSeekClampsToEndTest(TestReport& report)
    {
        std::vector<SortItem> initialItems{{3, 0}, {1, 1}, {2, 2}};
        std::vector<SortEvent> events{
            SwapEvent{0, 1}
        };

        AnimationPlayer player;

        player.load(initialItems, events);
        player.seekToEventPosition(999);

        const SortState& state = player.currentState();

        std::vector<SortItem> expectedItems{{1, 1}, {3, 0}, {2, 2}};
        std::vector<ItemVisualState> expectedVisualStates{
            ItemVisualState::Swapping,
            ItemVisualState::Swapping,
            ItemVisualState::Normal
        };

        recordRequired(
            report,
            "animation",
            "seek clamps to end",
            "checkStateItems",
            checkStateItems(state, expectedItems)
        );

        recordRequired(
            report,
            "animation",
            "seek clamps to end",
            "checkVisualStates",
            checkVisualStates(state, expectedVisualStates)
        );

        recordRequired(
            report,
            "animation",
            "seek clamps to end",
            "checkComplete",
            checkComplete(state, true)
        );

        recordRequired(
            report,
            "animation",
            "seek clamps to end",
            "checkEventPosition",
            checkEventPosition(player, 1)
        );
    }

    void runAnimationSortTraceReplayTest(TestReport& report)
    {
        // This is the cross-layer animation test:
        // sorting produces a real SortTrace, then AnimationPlayer consumes that
        // trace without knowing which algorithm produced it.
        std::vector<SortItem> input{{3, 0}, {1, 1}, {2, 2}};
        SortTrace trace = runSort(Algorithm::Insertion, input);

        AnimationPlayer player;

        player.load(input, trace.events);

        while (!player.isComplete()) {
            player.stepForward();
        }

        const SortState& finalState = player.currentState();

        recordRequired(
            report,
            "animation",
            "sort trace replay",
            "checkStateItems",
            checkStateItems(finalState, trace.finalItems)
        );

        recordRequired(
            report,
            "animation",
            "sort trace replay",
            "checkComplete",
            checkComplete(finalState, true)
        );
    }

    // =================================================================================
    // App layer (VisualizerSession) tests
    // =================================================================================

    // Shared deterministic fixture for session tests that do not need a
    // specialized ValueSpec or InitialOrderSpec.
    SortRunSpec createInitialRunSpec()
    {
        const SortRunSpec initialSettings{
            Algorithm::Selection,
            SortInputSpec{
                10,
                PermutationValueSpec{},
                RandomOrderSpec{},
                12345
            }
        };

        return initialSettings;
    }

    // A new session should load the requested run without marking settings dirty
    // or applying any animation events.
    void runVisualizerSessionConstructionTest(TestReport& report)
    {
        const SortRunSpec initialSettings = createInitialRunSpec();

        VisualizerSession session(initialSettings);

        recordRequired(
            report,
            "app/session",
            "construction",
            "checkDraftSettings",
            {
                session.draftSettings() == initialSettings,
                "Draft settings differ from the constructor settings."
            }
        );

        recordRequired(
            report,
            "app/session",
            "construction",
            "checkLoadedSettings",
            {
                session.loadedSettings() == initialSettings,
                "Loaded settings differ from the constructor settings."
            }
        );

        recordRequired(
            report,
            "app/session",
            "construction",
            "checkSettingsClean",
            {
                !session.settingsDirty(),
                "A newly constructed session reports dirty settings."
            }
        );

        recordRequired(
            report,
            "app/session",
            "construction",
            "checkItemCount",
            {
                session.currentSortState().items.size() ==
                    static_cast<std::size_t>(initialSettings.inputSpec.itemCount),
                "The initial SortState has an unexpected item count."
            }
        );

        recordRequired(
            report,
            "app/session",
            "construction",
            "checkEventPosition",
            {
                session.currentEventPosition() == 0,
                "A newly constructed session does not start at event position zero."
            }
        );
    }

    // Editing every draft field must not regenerate or advance the loaded run.
    // Apply is the only operation allowed to cross that boundary.
    void runVisualizerSessionDraftIsolationTest(TestReport& report)
    {
        const SortRunSpec initialSettings = createInitialRunSpec();

        VisualizerSession session(initialSettings);

        // Snapshot the loaded run so the checks can detect hidden side effects
        // from any of the draft setters below.
        const std::size_t initialEventPosition = session.currentEventPosition();

        const std::size_t initialEventCount = session.eventCount();

        const std::vector<SortItem> initialItems = generateInput(initialSettings.inputSpec);

        const SortRunSpec newSettings{
            Algorithm::Bubble,
            SortInputSpec{
                12,
                AllEqualValueSpec{7},
                DescendingOrderSpec{},
                12345
            }
        };
        session.setDraftAlgorithm(newSettings.algorithm);
        session.setDraftValueSpec(newSettings.inputSpec.valueSpec);
        session.setDraftOrderSpec(newSettings.inputSpec.initialOrderSpec);
        session.setDraftItemCount(newSettings.inputSpec.itemCount);

        recordRequired(
            report,
            "app/session",
            "draft isolation",
            "checkStateItemsUnchanged",
            checkStateItems(session.currentSortState(), initialItems)
        );
        recordRequired(
            report,
            "app/session",
            "draft isolation",
            "checkDraftSettings",
            {
                session.draftSettings() == newSettings,
                "Draft settings differ from the requested new settings."
            }
        );

        recordRequired(
            report,
            "app/session",
            "draft isolation",
            "checkLoadedSettingsUnchanged",
            {
                session.loadedSettings() == initialSettings,
                "Loaded settings changed while editing draft settings."
            }
        );

        recordRequired(
            report,
            "app/session",
            "draft isolation",
            "checkSettingsMarkedDirty",
            {
                session.settingsDirty(),
                "Settings were updated, but not marked as dirty."
            }
        );

        recordRequired(
            report,
            "app/session",
            "draft isolation",
            "checkCurrentEventPositionUnchanged",
            {
                session.currentEventPosition() == initialEventPosition,
                "The current event position changed while editing draft settings."
            }
        );

        recordRequired(
            report,
            "app/session",
            "draft isolation",
            "checkEventCountUnchanged",
            {
                session.eventCount() == initialEventCount,
                "The event count changed while editing draft settings."
            }
        );
    }

    // Dirty state is derived from draft-vs-loaded equality. Restoring the draft
    // should therefore restore clean status without a separate reset operation.
    void runVisualizerSessionDirtyStateReversibilityTest(TestReport& report)
    {
        const SortRunSpec initialSettings = createInitialRunSpec();

        VisualizerSession session(initialSettings);

        recordRequired(
            report,
            "app/session",
            "dirty state reversibility",
            "checkInitialSettingsClean",
            {
                !session.settingsDirty(),
                "Initial settings were incorrectly marked as dirty."
            }
        );
        
        session.setDraftAlgorithm(Algorithm::Bubble);

        recordRequired(
            report,
            "app/session",
            "dirty state reversibility",
            "checkSettingsMarkedDirty",
            {
                session.settingsDirty(),
                "Settings remain clean after changing the draft algorithm."
            }
        );

        session.setDraftAlgorithm(initialSettings.algorithm);

        recordRequired(
            report,
            "app/session",
            "dirty state reversibility",
            "checkSettingsDirtyReversible",
            {
                !session.settingsDirty(),
                "Settings remain dirty after restoring the initial draft algorithm."
            }
        );

        recordRequired(
            report,
            "app/session",
            "dirty state reversibility",
            "checkLoadedSettingsUnchanged",
            {
                session.loadedSettings() == initialSettings,
                "Loaded settings changed while editing and restoring the draft algorithm."
            }
        );
    }

    // Item-count edits are clamped at the session boundary while remaining
    // isolated from the loaded run until Apply.
    void runVisualizerSessionItemCountBoundsTest(TestReport& report)
    {
        const SortRunSpec initialSettings = createInitialRunSpec();

        VisualizerSession session(initialSettings);

        // Snapshot the active trace to prove that clamping affects only draft
        // configuration, not current playback.
        const std::size_t initialEventPosition = session.currentEventPosition();

        const std::size_t initialEventCount = session.eventCount();

        const std::vector<SortItem> initialItems = generateInput(initialSettings.inputSpec);

        session.setDraftItemCount(0);

        recordRequired(
            report,
            "app/session",
            "item count bounds",
            "checkMinimumItemCountClamped",
            {
                session.draftSettings().inputSpec.itemCount == VisualizerSession::minimumItemCount,
                "An item count below the minimum was not clamped correctly."
            }
        );

        session.setDraftItemCount(VisualizerSession::maximumItemCount + 1);

        recordRequired(
            report,
            "app/session",
            "item count bounds",
            "checkMaximumItemCountClamped",
            {
                session.draftSettings().inputSpec.itemCount == VisualizerSession::maximumItemCount,
                "An item count above the maximum was not clamped correctly."
            }
        );

        recordRequired(
            report,
            "app/session",
            "item count bounds",
            "checkLoadedSettingsUnchanged",
            {
                session.loadedSettings() == initialSettings,
                "Loaded settings changed while clamping the draft item count."
            }
        );

        recordRequired(
            report,
            "app/session",
            "item count bounds",
            "checkEventPositionUnchanged",
            {
                session.currentEventPosition() == initialEventPosition,
                "The current event position changed while clamping the draft item count."
            }
        );

        recordRequired(
            report,
            "app/session",
            "item count bounds",
            "checkEventCountUnchanged",
            {
                session.eventCount() == initialEventCount,
                "The event count changed while clamping the draft item count."
            }
        );

        recordRequired(
            report,
            "app/session",
            "item count bounds",
            "checkStateItemsUnchanged",
            checkStateItems(session.currentSortState(), initialItems)
        );
    }

    // Reducing item count must keep all fields of the draft FewUniqueValueSpec
    // mutually compatible without applying the draft.
    void runVisualizerSessionFewUniqueCompatibilityTest(TestReport& report)
    {
        const SortRunSpec initialSettings{
            Algorithm::Insertion,
            SortInputSpec{
                12,
                FewUniqueValueSpec{1, 10, 5},
                RandomOrderSpec{},
                12345
            }
        };

        VisualizerSession session(initialSettings);

        session.setDraftItemCount(3);

        recordRequired(
            report,
            "app/session",
            "few unique compatibility",
            "checkDraftItemCount",
            {
                session.draftSettings().inputSpec.itemCount == 3,
                "The draft item count differs from the requested value."
            }
        );

        const ValueSpec& draftValueSpec =
            session.draftSettings().inputSpec.valueSpec;

        // get_if returns nullptr instead of throwing if the variant contains a
        // different spec type. Record that contract before accessing its fields.
        const FewUniqueValueSpec* fewUniqueSpec =
            std::get_if<FewUniqueValueSpec>(&draftValueSpec);

        recordRequired(
            report,
            "app/session",
            "few unique compatibility",
            "checkFewUniqueSpecActive",
            {
                fewUniqueSpec != nullptr,
                "The draft value spec is not FewUniqueValueSpec."
            }
        );

        if (fewUniqueSpec == nullptr) {
            // The remaining checks require FewUniqueValueSpec fields. The type
            // failure above already records the actionable cause.
            return;
        }

        recordRequired(
            report,
            "app/session",
            "few unique compatibility",
            "checkMinimumValue",
            {
                fewUniqueSpec->minValue == 1,
                "FewUniqueValueSpec minimum value was not updated correctly."
            }
        );

        recordRequired(
            report,
            "app/session",
            "few unique compatibility",
            "checkMaximumValue",
            {
                fewUniqueSpec->maxValue == 3,
                "FewUniqueValueSpec maximum value was not updated correctly."
            }
        );

        recordRequired(
            report,
            "app/session",
            "few unique compatibility",
            "checkUniqueValueCount",
            {
                fewUniqueSpec->uniqueValueCount == 3,
                "FewUniqueValueSpec unique-value count was not updated correctly."
            }
        );
    }

    // Apply is the atomic transition from edited settings to a newly generated,
    // paused trace at event position zero.
    void runVisualizerSessionApplyTest(TestReport& report)
    {
        const SortRunSpec initialSettings = createInitialRunSpec();
        VisualizerSession session(initialSettings);

        // Make reset-to-start and pause observable postconditions instead of
        // checking a session that was already at event position zero.
        session.update(session.secondsPerEvent() * 3.5f);

        recordRequired(
            report,
            "app/session",
            "apply draft settings",
            "checkPreApplyPlaybackAdvanced",
            {
                session.currentEventPosition() > 0 && session.isPlaying(),
                "The pre-Apply session did not advance while playing."
            }
        );

        const SortRunSpec newSettings{
            Algorithm::Bubble,
            SortInputSpec{
                12,
                AllEqualValueSpec{7},
                DescendingOrderSpec{},
                12345
            }
        };

        const std::vector<SortItem> expectedItems =
            generateInput(newSettings.inputSpec);
        const SortTrace expectedTrace =
            runSort(newSettings.algorithm, expectedItems);

        session.setDraftAlgorithm(newSettings.algorithm);
        session.setDraftItemCount(newSettings.inputSpec.itemCount);
        session.setDraftValueSpec(newSettings.inputSpec.valueSpec);
        session.setDraftOrderSpec(newSettings.inputSpec.initialOrderSpec);

        recordRequired(
            report,
            "app/session",
            "apply draft settings",
            "checkSettingsMarkedDirty",
            {
                session.settingsDirty(),
                "Settings remain clean after changing the draft."
            }
        );

        session.applyDraftSettings();

        recordRequired(
            report,
            "app/session",
            "apply draft settings",
            "checkSettingsClean",
            {
                !session.settingsDirty(),
                "Settings remain dirty after applying the draft."
            }
        );

        recordRequired(
            report,
            "app/session",
            "apply draft settings",
            "checkLoadedSettingsApplied",
            {
                session.loadedSettings() == newSettings,
                "Loaded settings differ from the applied draft settings."
            }
        );

        recordRequired(
            report,
            "app/session",
            "apply draft settings",
            "checkPlaybackPaused",
            {
                !session.isPlaying(),
                "Playback remains active after applying the draft."
            }
        );

        recordRequired(
            report,
            "app/session",
            "apply draft settings",
            "checkEventPositionReset",
            {
                session.currentEventPosition() == 0,
                "Applying the draft did not reset the event position to zero."
            }
        );

        recordRequired(
            report,
            "app/session",
            "apply draft settings",
            "checkStateItemsApplied",
            checkStateItems(session.currentSortState(), expectedItems)
        );

        recordRequired(
            report,
            "app/session",
            "apply draft settings",
            "checkEventTraceApplied",
            {
                session.eventCount() == expectedTrace.events.size(),
                "The loaded event count differs from the expected applied trace."
            }
        );
    }

    // Playback toggling must alternate between the initial playing state and a
    // paused state without changing the loaded run.
    void runVisualizerSessionPlaybackToggleTest(TestReport& report)
    {
        const SortRunSpec initialSettings = createInitialRunSpec();
        VisualizerSession session(initialSettings);

        // Advancing first proves that the initial playing flag produces real
        // event progress, rather than checking the Boolean in isolation.
        session.update(session.secondsPerEvent() * 3.5f);

        const SortRunSpec loadedSettingsBeforeToggles =
            session.loadedSettings();
        const std::size_t eventPositionBeforeToggles =
            session.currentEventPosition();
        const std::size_t eventCountBeforeToggles = session.eventCount();

        recordRequired(
            report,
            "app/session",
            "playback toggle",
            "checkIfPlaying",
            {
                session.currentEventPosition() > 0 && session.isPlaying(),
                "The session did not advance while playing."
            }
        );
        session.togglePlayback();

        recordRequired(
            report,
            "app/session",
            "playback toggle",
            "checkInitialTogglePauses",
            {
                !session.isPlaying(),
                "togglePlayback failed to update isPlaying()."
            }
        );

        recordRequired(
            report,
            "app/session",
            "playback toggle",
            "checkTogglePreservesLoadedRun",
            {
                session.loadedSettings() == loadedSettingsBeforeToggles &&
                    session.currentEventPosition() == eventPositionBeforeToggles &&
                    session.eventCount() == eventCountBeforeToggles,
                "A playback toggle changed the loaded settings or event trace."
            }
        );

        session.togglePlayback();

        recordRequired(
            report,
            "app/session",
            "playback toggle",
            "checkSecondTogglePlays",
            {
                session.isPlaying(),
                "togglePlayback failed to update isPlaying()."
            }
        );
    }

    // Manual stepping changes exactly one event while paused, respects the
    // beginning boundary, and does nothing during automatic playback.
    void runVisualizerSessionManualStepTest(TestReport& report)
    {
        const SortRunSpec initialSettings = createInitialRunSpec();
        VisualizerSession session(initialSettings);

        // VisualizerSession starts in automatic playback mode. Manual commands
        // are enabled only while paused, so establish that precondition first.
        session.togglePlayback();

        session.stepBackward();

        recordRequired(
            report,
            "app/session",
            "manual step test",
            "checkStepBackwardAtBeginning",
            {
                session.currentEventPosition() == 0,
                "Manual backward stepping moved before event position zero."
            }
        );

        const std::size_t initialEventPosition = session.currentEventPosition();

        session.stepForward();

        recordRequired(
            report,
            "app/session",
            "manual step test",
            "checkStepForwardAdvancesOnce",
            {
                session.currentEventPosition() == initialEventPosition + 1,
                "Manual forward stepping did not advance exactly one event."
            }
        );

        session.stepBackward();

        recordRequired(
            report,
            "app/session",
            "manual step test",
            "checkStepBackwardRetreatsOnce",
            {
                session.currentEventPosition() == initialEventPosition,
                "Manual backward stepping did not retreat exactly one event."
            }
        );

        // Move away from the lower boundary before testing the playing-state
        // guard. Otherwise an ignored backward step and a boundary-clamped
        // backward step would be indistinguishable.
        session.stepForward();
        const std::size_t positionBeforePlayingSteps =
            session.currentEventPosition();

        session.togglePlayback();
        session.stepForward();

        recordRequired(
            report,
            "app/session",
            "manual step test",
            "checkStepForwardIgnoredWhilePlaying",
            {
                session.currentEventPosition() == positionBeforePlayingSteps,
                "Manual forward stepping changed position during automatic playback."
            }
        );

        // Check backward independently. Combining both commands before one
        // assertion could hide a defect if an incorrect forward step were then
        // canceled by an incorrect backward step.
        session.stepBackward();

        recordRequired(
            report,
            "app/session",
            "manual step test",
            "checkStepBackwardIgnoredWhilePlaying",
            {
                session.currentEventPosition() == positionBeforePlayingSteps,
                "Manual backward stepping changed position during automatic playback."
            }
        );
    }

    // Seeking reaches valid positions, pauses playback, restores position zero,
    // and clamps requests beyond the loaded trace.
    void runVisualizerSessionSeekTest(TestReport& report)
    {
        const SortRunSpec initialSettings = createInitialRunSpec();
        VisualizerSession session(initialSettings);

        const std::size_t eventCount = session.eventCount();

        // Derive a valid target from the loaded trace instead of assuming a
        // particular algorithm always emits at least a fixed number of events.
        const std::size_t targetPosition = eventCount / 2;

        session.seekToEventPosition(targetPosition);

        recordRequired(
            report,
            "app/session",
            "event seek test",
            "checkSeekReachesTargetPosition",
            {
                session.currentEventPosition() == targetPosition,
                "Seeking did not reach the requested event position."
            }
        );

        recordRequired(
            report,
            "app/session",
            "event seek test",
            "checkSeekPausesPlayback",
            {
                !session.isPlaying(),
                "Event seek failed to pause session."
            }
        );

        session.seekToEventPosition(0);

        recordRequired(
            report,
            "app/session",
            "event seek test",
            "checkSeekReturnsToBeginning",
            {
                session.currentEventPosition() == 0,
                "Seeking to event position zero did not return to the beginning."
            }
        );

        // Positions beyond the trace clamp to eventCount, which means every
        // loaded event has been applied.
        session.seekToEventPosition(eventCount + 1);

        recordRequired(
            report,
            "app/session",
            "event seek test",
            "checkSeekClampsToEnd",
            {
                session.currentEventPosition() == eventCount,
                "Seeking beyond the trace did not clamp to the final position."
            }
        );

        recordRequired(
            report,
            "app/session",
            "event seek test",
            "checkSeekPastEndCompletesPlayback",
            {
                session.isComplete(),
                "Seeking to the final event position did not complete playback."
            }
        );
    }

    // Realtime updates accumulate partial intervals, advance due events up to
    // the per-frame safety cap, and ignore elapsed time while paused or complete.
    void runVisualizerSessionUpdateTest(TestReport& report)
    {
        const SortRunSpec initialSettings = createInitialRunSpec();

        // Use an isolated session so accumulated time cannot affect the other
        // update scenarios in this test.
        {
            VisualizerSession session(initialSettings);
            const float eventInterval = session.secondsPerEvent();

            session.update(eventInterval * 0.4f);

            recordRequired(
                report,
                "app/session",
                "session update test",
                "checkPartialIntervalDoesNotAdvance",
                {
                    session.currentEventPosition() == 0,
                    "A partial event interval advanced playback."
                }
            );

            session.update(eventInterval * 0.4f);

            recordRequired(
                report,
                "app/session",
                "session update test",
                "checkAccumulatedTimeBelowIntervalDoesNotAdvance",
                {
                    session.currentEventPosition() == 0,
                    "Accumulated time below one event interval advanced playback."
                }
            );

            session.update(eventInterval * 0.4f);

            recordRequired(
                report,
                "app/session",
                "session update test",
                "checkAccumulatedTimeAdvancesOneEvent",
                {
                    session.currentEventPosition() == 1,
                    "Accumulated time above one event interval did not advance exactly one event."
                }
            );
        }

        // A long frame may consume several complete event intervals in one
        // update rather than limiting playback to one event per rendered frame.
        {
            VisualizerSession session(initialSettings);
            const float eventInterval = session.secondsPerEvent();

            session.update(eventInterval * 3.5f);

            recordRequired(
                report,
                "app/session",
                "session update test",
                "checkOneFrameCanApplyMultipleEvents",
                {
                    session.currentEventPosition() == 3,
                    "One update did not apply all complete event intervals."
                }
            );
        }

        {
            VisualizerSession session(initialSettings);
            const float eventInterval = session.secondsPerEvent();

            session.togglePlayback();
            const std::size_t positionWhenPaused =
                session.currentEventPosition();

            session.update(eventInterval * 5.4f);

            recordRequired(
                report,
                "app/session",
                "session update test",
                "checkPausedSessionIgnoresElapsedTime",
                {
                    session.currentEventPosition() == positionWhenPaused,
                    "A paused session advanced when update received elapsed time."
                }
            );
        }

        {
            VisualizerSession session(initialSettings);
            const float eventInterval = session.secondsPerEvent();
            const std::size_t eventCount = session.eventCount();

            session.seekToEventPosition(eventCount);

            // Seeking pauses playback. Resume it so this scenario exercises the
            // completed-state guard rather than passing through the paused guard.
            session.togglePlayback();
            session.update(eventInterval * 5.4f);

            recordRequired(
                report,
                "app/session",
                "session update test",
                "checkCompleteSessionIgnoresElapsedTime",
                {
                    session.currentEventPosition() == eventCount,
                    "A complete session advanced beyond its final event position."
                }
            );

            recordRequired(
                report,
                "app/session",
                "session update test",
                "checkCompleteSessionRemainsComplete",
                {
                    session.isComplete(),
                    "A complete session lost its completion state after update."
                }
            );
        }
    }

    // Speed changes preserve the inverse seconds-per-event meaning and clamp
    // extreme deltas to valid positive durations.
    void runVisualizerSessionSpeedTest(TestReport& report)
    {
        const SortRunSpec initialSettings = createInitialRunSpec();
        VisualizerSession session(initialSettings);

        const float initialSecondsPerEvent = session.secondsPerEvent();

        // A smaller duration means events are applied more frequently.
        session.changePlaybackSpeed(-initialSecondsPerEvent * 0.5f);
        const float fasterSecondsPerEvent = session.secondsPerEvent();

        recordRequired(
            report,
            "app/session",
            "playback speed",
            "checkNegativeDeltaIncreasesSpeed",
            {
                fasterSecondsPerEvent > 0.0f &&
                    fasterSecondsPerEvent < initialSecondsPerEvent,
                "A negative speed delta did not produce a smaller positive event interval."
            }
        );

        session.changePlaybackSpeed(initialSecondsPerEvent * 0.25f);

        recordRequired(
            report,
            "app/session",
            "playback speed",
            "checkPositiveDeltaDecreasesSpeed",
            {
                session.secondsPerEvent() > fasterSecondsPerEvent,
                "A positive speed delta did not produce a larger event interval."
            }
        );

        // Extreme changes should saturate at private bounds. Reapplying the
        // same extreme delta must therefore leave the clamped value unchanged.
        session.changePlaybackSpeed(-1000.0f);
        const float minimumInterval = session.secondsPerEvent();
        session.changePlaybackSpeed(-1000.0f);

        recordRequired(
            report,
            "app/session",
            "playback speed",
            "checkMinimumSpeedIntervalClamp",
            {
                minimumInterval > 0.0f &&
                    session.secondsPerEvent() == minimumInterval,
                "The minimum event interval was not positive and stable under repeated clamping."
            }
        );

        session.changePlaybackSpeed(1000.0f);
        const float maximumInterval = session.secondsPerEvent();
        session.changePlaybackSpeed(1000.0f);

        recordRequired(
            report,
            "app/session",
            "playback speed",
            "checkMaximumSpeedIntervalClamp",
            {
                maximumInterval > minimumInterval &&
                    session.secondsPerEvent() == maximumInterval,
                "The maximum event interval was not stable under repeated clamping."
            }
        );
    }

    // Reset pauses at position zero, restores the initial item order, and keeps
    // the loaded trace available for subsequent playback.
    void runVisualizerSessionResetTest(TestReport& report)
    {
        const SortRunSpec initialSettings = createInitialRunSpec();
        const std::vector<SortItem> initialItems =
            generateInput(initialSettings.inputSpec);

        VisualizerSession session(initialSettings);
        const std::size_t eventCount = session.eventCount();
        const float eventInterval = session.secondsPerEvent();

        // Advance several events so returning to frame zero and restoring the
        // initial animation state are observable reset postconditions.
        session.update(eventInterval * 3.5f);
        session.resetPlayback();

        recordRequired(
            report,
            "app/session",
            "playback reset",
            "checkResetReturnsToBeginning",
            {
                session.currentEventPosition() == 0,
                "Reset did not return to event position zero."
            }
        );

        recordRequired(
            report,
            "app/session",
            "playback reset",
            "checkResetPausesPlayback",
            {
                !session.isPlaying(),
                "Reset did not pause playback."
            }
        );

        recordRequired(
            report,
            "app/session",
            "playback reset",
            "checkResetRestoresInitialItems",
            checkStateItems(session.currentSortState(), initialItems)
        );

        const std::vector<ItemVisualState> expectedVisualStates(
            initialItems.size(),
            ItemVisualState::Normal);

        recordRequired(
            report,
            "app/session",
            "playback reset",
            "checkResetClearsVisualStates",
            checkVisualStates(session.currentSortState(), expectedVisualStates)
        );

        recordRequired(
            report,
            "app/session",
            "playback reset",
            "checkResetPreservesLoadedTrace",
            {
                session.eventCount() == eventCount,
                "Reset replaced or discarded the loaded event trace."
            }
        );

        // Completion must be established before reset so the completion check
        // cannot pass merely because the session was already incomplete.
        VisualizerSession completedSession(initialSettings);
        completedSession.seekToEventPosition(completedSession.eventCount());

        recordRequired(
            report,
            "app/session",
            "playback reset from completion",
            "checkResetPreconditionComplete",
            {
                completedSession.isComplete(),
                "The reset-completion scenario did not reach the final event position."
            }
        );

        completedSession.resetPlayback();

        recordRequired(
            report,
            "app/session",
            "playback reset from completion",
            "checkResetClearsCompletion",
            {
                !completedSession.isComplete() &&
                    completedSession.currentEventPosition() == 0,
                "Reset did not clear completion and return to event position zero."
            }
        );
    }

    TestReport runVisualizerSessionTests()
    {
        TestReport report;

        runVisualizerSessionConstructionTest(report);
        runVisualizerSessionDraftIsolationTest(report);
        runVisualizerSessionDirtyStateReversibilityTest(report);
        runVisualizerSessionItemCountBoundsTest(report);
        runVisualizerSessionFewUniqueCompatibilityTest(report);
        runVisualizerSessionApplyTest(report);
        runVisualizerSessionPlaybackToggleTest(report);
        runVisualizerSessionManualStepTest(report);
        runVisualizerSessionSeekTest(report);
        runVisualizerSessionUpdateTest(report);
        runVisualizerSessionSpeedTest(report);
        runVisualizerSessionResetTest(report);

        return report;
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

    // Runs the full input-generation test battery for one SortInputSpec.
    //
    // runInputGeneratorTests chooses which specs to test.
    // runInputCase defines what every generated input must be checked against.
    //
    // This keeps the rule "every generated input gets every applicable test" in
    // one place instead of duplicating the same sequence for every test case.
    void runInputCase(
        const SortInputSpec& inputSpec,
        TestReport& report,
        const char* caseName)
    {
        std::vector<SortItem> items = generateInput(inputSpec);

        recordRequired(report, "input", caseName, "checkItemCount", checkItemCount(inputSpec, items));
        recordRequired(report, "input", caseName, "checkIdsValid", checkIdsValid(inputSpec, items));
        recordRequired(report, "input", caseName, "checkDeterministicGeneration", checkDeterministicGeneration(inputSpec));

        std::visit(
            [&](const auto& valueSpec) {
                recordRequired(report, "input", caseName, "checkValueSpec", checkValueSpec(valueSpec, inputSpec, items));
            },
            inputSpec.valueSpec);

        std::visit(
            [&](const auto& orderSpec) {
                recordRequired(report, "input", caseName, "checkInitialOrderSpec", checkInitialOrderSpec(orderSpec, inputSpec, items));
            },
            inputSpec.initialOrderSpec);
    }

    TestReport runInputGeneratorTests()
    {
        // These SortInputSpecs are fixed test cases, not user-facing app inputs.
        // Each case is passed through runInputCase, which applies the universal,
        // ValueSpec, and InitialOrderSpec checks.
        TestReport report;
        
        runInputCase(
            SortInputSpec{
                10,
                PermutationValueSpec{},
                AscendingOrderSpec{},
                12345
            },
            report,
            "permutation ascending"
        );

        // Covers the case where a full period does not fit and itemCount is smaller
        // than periodLength. This exercises the partial-period branch in the
        // PeriodicValueSpec check.
        runInputCase(
            SortInputSpec{
                3,
                PeriodicValueSpec{5},
                RandomOrderSpec{},
                12345
            },
            report,
            "periodic partial random"
        );

        runInputCase(
            SortInputSpec{
                10,
                PermutationValueSpec{},
                DescendingOrderSpec{},
                12345
            },
            report,
            "permutation descending"
        );

        runInputCase(
            SortInputSpec{
                20,
                RangeValueSpec{1, 100, DuplicatePolicy::AllowDuplicates},
                RandomOrderSpec{},
                12345
            },
            report,
            "range random duplicates"
        );

        runInputCase(
            SortInputSpec{
                10,
                RangeValueSpec{1, 10, DuplicatePolicy::RequireUnique},
                RandomOrderSpec{},
                12345
            },
            report,
            "range random unique"
        );

        runInputCase(
            SortInputSpec{
                12,
                AllEqualValueSpec{7},
                RandomOrderSpec{},
                12345
            },
            report,
            "allEqual random"
        );

        runInputCase(
            SortInputSpec{
                20,
                FewUniqueValueSpec{1, 100, 5},
                RandomOrderSpec{},
                12345
            },
            report,
            "fewUnique random"
        );

        runInputCase(
            SortInputSpec{
                10,
                PeriodicValueSpec{3},
                RandomOrderSpec{},
                12345
            },
            report,
            "periodic random"
        );

        runInputCase(
            SortInputSpec{
                10,
                PeriodicValueSpec{3},
                AscendingOrderSpec{},
                12345
            },
            report,
            "periodic ascending"
        );

        runInputCase(
            SortInputSpec{
                10,
                PermutationValueSpec{},
                NearlyAscendingOrderSpec{0.0},
                12345
            },
            report,
            "permutation nearlyAscending zero"
        );

        runInputCase(
            SortInputSpec{
                20,
                PermutationValueSpec{},
                NearlyAscendingOrderSpec{0.25},
                12345
            },
            report,
            "permutation nearlyAscending partial"
        );

        return report;
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

    TestReport runAnimationTests()
    {
        TestReport report;

        runAnimationLoadWithNoEventsTest(report);
        runAnimationLoadWithPendingEventTest(report);
        runAnimationCompareEventTest(report);
        runAnimationSwapEventTest(report);
        runAnimationMoveEventTest(report);
        runAnimationMarkSortedPersistsTest(report);
        runAnimationStepAfterCompleteTest(report);
        runAnimationEventPositionTest(report);
        runAnimationStepBackwardAtBeginningTest(report);
        runAnimationStepBackwardAfterMoveTest(report);
        runAnimationSeekToEventPositionTest(report);
        runAnimationSeekClampsToEndTest(report);
        runAnimationSortTraceReplayTest(report);

        return report;
    }
}

// =================================================================================
// Public test runner
// =================================================================================

TestReport runAllTests()
{
    TestReport report;

    addReport(report, runInputGeneratorTests());
    addReport(report, runSortingTests());
    addReport(report, runAnimationTests());
    addReport(report, runVisualizerSessionTests());

    return report;
}
