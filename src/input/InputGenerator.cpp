#include "InputGenerator.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <random>
#include <stdexcept>
#include <variant>
#include <vector>

// Functions in this unnamed namespace are private to this .cpp file.
// The only public function in this file is generateInput at the bottom.
namespace {

// =================================================================================
// Small validation/count helper functions.
// These stay private to this file because range validation is an implementation detail
// of input generation, not part of the public domain model.
// =================================================================================

// Returns the number of integer values in the inclusive range [minValue, maxValue].
long long availableValueCount(int minValue, int maxValue)
{
    return static_cast<long long>(maxValue) - static_cast<long long>(minValue) + 1;
}

// Throws an error if the requested integer range is invalid.
void requireValidRange(int minValue, int maxValue)
{
    if (minValue > maxValue) {
        throw std::invalid_argument("minValue must be less than or equal to maxValue.");
    }
}

// =================================================================================
// Sampling helper functions
// =================================================================================

// Returns a randomized integer vector containing NO DUPLICATES.
// In statistics this is called sampling without replacement. It means when we draw a number from the set of allowed numbers
// and we do not replace it in the set. All subsequent draws now see a set of allowed numbers which is smaller.
// In this file it is used both for RequireUnique range inputs and for building the pool in FewUniqueValueSpec.
std::vector<int> sampleWithoutReplacement(
    int minValue,
    int maxValue,
    unsigned int itemCount,
    std::mt19937& generator)
{
    requireValidRange(minValue, maxValue);

    long long valueCount = availableValueCount(minValue, maxValue);
    if (static_cast<long long>(itemCount) > valueCount) {
        throw std::invalid_argument("itemCount is larger than the number of available unique values.");
    }

    std::vector<int> values;
    // reserve allocates enough space for later push_back calls, but it does not create elements.
    values.reserve(static_cast<std::size_t>(valueCount));

    // First build every possible value in the allowed range.
    for (long long value = minValue; value <= maxValue; ++value) {
        values.push_back(static_cast<int>(value));
    }

    // Then shuffle the pool and keep the requested number of values.
    // This is simpler than repeatedly drawing and checking for duplicates.
    std::shuffle(values.begin(), values.end(), generator);
    values.resize(itemCount);

    return values;
}

// Returns an integer vector of randomized values where duplicates are allowed.
// This is called sampling with replacement in statistics.
// Each draw is independent, so the same value can be chosen many times.
std::vector<int> sampleWithReplacement(
    int minValue,
    int maxValue,
    unsigned int itemCount,
    std::mt19937& generator)
{
    requireValidRange(minValue, maxValue);

    std::vector<int> values;
    // reserve is a performance hint. It does not change values.size().
    values.reserve(itemCount);

    // uniform_int_distribution includes both endpoints: minValue and maxValue are both possible.
    std::uniform_int_distribution<int> distribution(minValue, maxValue);

    for (unsigned int index = 0; index < itemCount; ++index) {
        values.push_back(distribution(generator));
    }

    return values;
}


// =================================================================================
// Overloaded functions to generate input values
// 
// Since I am not familiar with C++ here's some information:
//
// In C++ functions can be overloaded. This means multiple functions can share
// the same name, in this case generateValuesFromSpec.
//
// This works because while all the functions share the same name and return type (vector<int>),
// the first input is a different data type for each instance of the function.
// So, there is a function which expects an input of PermutationValueSpec, one which
// expects RangeValueSpec, etc. (all the structs are in domain/SortSpec.hpp)
//
// This is helpful because it lets us use the visit command. Using this command we
// can turn a function call "generateValuesFromSpec(...)" into one which visits the
// version of the overloaded function which accepts the datatype we chose to input.
// So if we are calling the function with an input type PermutationValueSpec, the
// version of the function which handles that type is called.
//
// Note:
//
// To do this, each instance of the function needs the same interface... that is,
// they need to expect the same inputs. In this case it expects a valueSpec, an
// itemCount, and a generator. However, some of the instances of generateValuesFromSpec
// do not actually use all three inputs. To get around this, we simply declare the
// input and do not assign it a variable.
//
// EX:
// 
// "std::mt19937&" does NOT assign a variable but "std::mt19937& generator" DOES.
//
// "const RangeValueSpec& valueSpec" means:
// - RangeValueSpec: the type of object this overload handles
// - &: pass by reference, so the function does not copy the whole object
// - const: this function is not allowed to modify valueSpec
// =================================================================================

// Generates the permutation value set: 1, 2, 3, ..., itemCount.
// The input order is applied later, so this function only decides which values exist.
std::vector<int> generateValuesFromSpec(
    const PermutationValueSpec&,
    unsigned int itemCount,
    std::mt19937&)
{
    std::vector<int> values;
    values.reserve(itemCount);

    for (unsigned int value = 1; value <= itemCount; ++value) {
        values.push_back(static_cast<int>(value));
    }

    return values;
}

std::vector<int> generateValuesFromSpec(
    const RangeValueSpec& valueSpec,
    unsigned int itemCount,
    std::mt19937& generator)
{
    // The RangeValueSpec overload only chooses the duplicate policy.
    // The mechanics of sampling are hidden in the helper functions above.
    switch (valueSpec.duplicatePolicy) {
    case DuplicatePolicy::AllowDuplicates:
        return sampleWithReplacement(
            valueSpec.minValue,
            valueSpec.maxValue,
            itemCount,
            generator);
        
    case DuplicatePolicy::RequireUnique:
        return sampleWithoutReplacement(
            valueSpec.minValue,
            valueSpec.maxValue,
            itemCount,
            generator);
    }

    throw std::invalid_argument("Unknown DuplicatePolicy.");
}

std::vector<int> generateValuesFromSpec(
    const AllEqualValueSpec& valueSpec,
    unsigned int itemCount,
    std::mt19937&)
{
    return std::vector<int>(itemCount, valueSpec.value);
}

std::vector<int> generateValuesFromSpec(
    const FewUniqueValueSpec& valueSpec,
    unsigned int itemCount,
    std::mt19937& generator)
{
    // Meaning:
    // Choose exactly uniqueValueCount distinct values from the range.
    // Guarantee each chosen value appears at least once.
    // Fill the remaining slots randomly from that same small pool.
    if (valueSpec.uniqueValueCount <= 0) {
        throw std::invalid_argument("uniqueValueCount must be greater than 0.");
    }

    if (static_cast<unsigned int>(valueSpec.uniqueValueCount) > itemCount) {
        throw std::invalid_argument("uniqueValueCount cannot be higher than the itemCount.");
    }

    std::vector<int> uniqueValues = sampleWithoutReplacement(
        valueSpec.minValue,
        valueSpec.maxValue,
        static_cast<unsigned int>(valueSpec.uniqueValueCount),
        generator);

    // This distribution chooses an index into uniqueValues, not a value from the original range.
    std::uniform_int_distribution<std::size_t> distribution(0, uniqueValues.size() - 1);

    std::vector<int> values;
    values.reserve(itemCount);

    // Add one copy of each chosen value first. This guarantees the final result
    // really contains exactly uniqueValueCount distinct values.
    for (int value : uniqueValues) {
        values.push_back(value);
    }

    // Fill the remaining slots by sampling from the small pool.
    while (values.size() < itemCount) {
        values.push_back(uniqueValues[distribution(generator)]);
    }

    // Shuffle at the end so the guaranteed values are not always clustered at the front.
    std::shuffle(values.begin(), values.end(), generator);

    return values;
}

std::vector<int> generateValuesFromSpec(
    const PeriodicValueSpec& valueSpec,
    unsigned int itemCount,
    std::mt19937&)
{
    if (valueSpec.periodLength <= 0) {
        throw std::invalid_argument("periodLength must be greater than 0.");
    }

    std::vector<int> values;
    values.reserve(itemCount);

    // Modulo creates the repeating pattern:
    // periodLength = 3 gives index % 3 as 0, 1, 2, 0, 1, 2, ...
    // Adding 1 turns that into values 1, 2, 3, 1, 2, 3, ...
    for (unsigned int index = 0; index < itemCount; ++index) {
        int value = static_cast<int>(index % static_cast<unsigned int>(valueSpec.periodLength)) + 1;
        values.push_back(value);
    }

    return values;
}

std::vector<int> generateValues(const SortInputSpec& spec, std::mt19937& generator)
{
    // spec.valueSpec is a std::variant, so it contains exactly one of the ValueSpec structs.
    // std::visit opens the variant and gives the active struct to the lambda below.
    return std::visit(
        // const auto& means "take whatever ValueSpec type is active, by const reference."
        // If the active type is RangeValueSpec, this calls the RangeValueSpec overload above.
        [&](const auto& valueSpec) {
            return generateValuesFromSpec(valueSpec, spec.itemCount, generator);
        },
        spec.valueSpec);

}

// ==========================================================================
// Functions to apply the ordering spec
// These are called by an overloaded function which dispatches the
// appropriate order application function
// ==========================================================================

void applyRandomOrder(std::vector<int>& values, std::mt19937& generator)
{
    std::shuffle(values.begin(), values.end(), generator);
}

void applyAscendingOrder(std::vector<int>& values)
{
    std::sort(values.begin(), values.end());
}

void applyDescendingOrder(std::vector<int>& values)
{
    std::sort(values.begin(), values.end(), std::greater<int>());
}

// Current definition of "nearly ascending":
// sort ascending, then apply a number of random swaps derived from disorderFraction.
//
// This does not guarantee that exactly disorderFraction of items end out of order.
// If the project later needs a stricter meaning, change the input generator and
// the matching tests together so the contract stays explicit.
void applyNearlyAscendingOrder(
    std::vector<int>& values,
    const NearlyAscendingOrderSpec& orderSpec,
    std::mt19937& generator)
{
    if (orderSpec.disorderFraction < 0.0 || orderSpec.disorderFraction > 1.0)
    {
        throw std::invalid_argument("disorderFraction must be between 0.0 and 1.0.");
    }

    std::sort(values.begin(), values.end());

    if (values.size() < 2)
    {
        return;
    }

    int swapCount = static_cast<int>(values.size() * orderSpec.disorderFraction / 2.0);
    std::uniform_int_distribution<std::size_t> distribution(0, values.size() - 1);

    for (int index = 0; index < swapCount; ++index)
    {
        std::size_t firstIndex = distribution(generator);
        std::size_t secondIndex = distribution(generator);

        std::swap(values[firstIndex], values[secondIndex]);
    }
}


void applyOrderFromSpec(
    const RandomOrderSpec&,
    std::vector<int>& values,
    std::mt19937& generator)
{
    applyRandomOrder(values, generator);
}

void applyOrderFromSpec(
    const AscendingOrderSpec&,
    std::vector<int>& values,
    std::mt19937&)
{
    applyAscendingOrder(values);
}

void applyOrderFromSpec(
    const DescendingOrderSpec&,
    std::vector<int>& values,
    std::mt19937&)
{
    applyDescendingOrder(values);
}

void applyOrderFromSpec(
    const NearlyAscendingOrderSpec& orderSpec,
    std::vector<int>& values,
    std::mt19937& generator)
{
    applyNearlyAscendingOrder(values, orderSpec, generator);
}

void applyInitialOrder(
    std::vector<int>& values,
    const InitialOrderSpec& initialOrderSpec,
    std::mt19937& generator)
{
    // This is the same variant pattern used for ValueSpec, but now for ordering.
    std::visit(
        [&](const auto& initialOrderSpec) {
            applyOrderFromSpec(initialOrderSpec, values, generator);
        },
        initialOrderSpec);
}

// Converts raw integer values into SortItems and assigns stable ids from 0 to N - 1.
std::vector<SortItem> makeSortItems(const std::vector<int>& values)
{
    std::vector<SortItem> items;
    items.reserve(values.size());

    for (std::size_t index = 0; index < values.size(); ++index) {
        SortItem item;
        item.value = values[index];
        item.id = static_cast<int>(index);

        items.push_back(item);
    }

    return items;
}

}

std::vector<SortItem> generateInput(const SortInputSpec& spec)
{
    // Create one seeded generator for this input run.
    std::mt19937 generator(spec.seed);

    // First generate values, then arrange them according to the requested initial order.
    std::vector<int> values = generateValues(spec, generator);

    applyInitialOrder(values, spec.initialOrderSpec, generator);

    return makeSortItems(values);
}
