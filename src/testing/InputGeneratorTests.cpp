#include "TestSupport.hpp"

#include "domain/SortSpec.hpp"
#include "input/InputGenerator.hpp"

#include <cstddef>
#include <unordered_map>
#include <variant>
#include <vector>

namespace {

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
