#pragma once

#include "Algorithm.hpp"

#include <cstdint>
#include <variant>

// SortSpec.hpp contains configuration data only.
// It should describe a requested run, but it should not generate values,
// shuffle vectors, draw anything, or run a sorting algorithm.
//
// These structs are value objects: two specs with the same fields mean the same
// requested input/run. Defaulted equality supports app dirty-state checks
// without making the app manually compare every nested field.

// =================================================================================
// Initial order specs: how already-generated values are arranged at the start
// =================================================================================

// Empty structs like RandomOrderSpec are "tag" types.
// They carry no fields because the type name itself fully describes the choice.

// Completely random order.
struct RandomOrderSpec {
    bool operator==(const RandomOrderSpec&) const = default;
};

// Perfectly ascending order.
struct AscendingOrderSpec {
    bool operator==(const AscendingOrderSpec&) const = default;
};

// Perfectly descending order.
struct DescendingOrderSpec {
    bool operator==(const DescendingOrderSpec&) const = default;
};

// Ascending values with random swap attempts derived from disorderFraction.
// The fraction does not promise an exact final proportion of displaced items.
struct NearlyAscendingOrderSpec {
    double disorderFraction;

    bool operator==(const NearlyAscendingOrderSpec&) const = default;
};

// InitialOrderSpec is a variant of the possible orderings.
// A std::variant holds exactly one of these structs at a time.
using InitialOrderSpec = std::variant<
    RandomOrderSpec,
    AscendingOrderSpec,
    DescendingOrderSpec,
    NearlyAscendingOrderSpec
>;

// =================================================================================
// Value specs: what values exist before the initial order is applied
// =================================================================================

// DuplicatePolicy belongs to RangeValueSpec because ranges can either allow
// duplicates or require unique values. Other value modes have their own fixed rules.
enum class DuplicatePolicy {
    AllowDuplicates,
    RequireUnique
};

// Input values are exactly 1, 2, 3, ..., itemCount.
// This implies no duplicates and does not need min/max fields.
struct PermutationValueSpec {
    bool operator==(const PermutationValueSpec&) const = default;
};

// Input values are sampled from the inclusive range [minValue, maxValue].
// duplicatePolicy decides whether repeated values are allowed.
struct RangeValueSpec {
    int minValue;
    int maxValue;
    DuplicatePolicy duplicatePolicy;

    bool operator==(const RangeValueSpec&) const = default;
};

// All input values are equal to value.
struct AllEqualValueSpec {
    int value;

    bool operator==(const AllEqualValueSpec&) const = default;
};

// Choose exactly uniqueValueCount distinct values from [minValue, maxValue],
// then generate itemCount values using only those chosen values.
// Input generation guarantees every chosen unique value appears at least once.
struct FewUniqueValueSpec {
    int minValue;
    int maxValue;
    int uniqueValueCount;

    bool operator==(const FewUniqueValueSpec&) const = default;
};

// Repeat the value pattern 1, 2, ..., periodLength until itemCount values exist.
struct PeriodicValueSpec {
    int periodLength;

    bool operator==(const PeriodicValueSpec&) const = default;
};

// ValueSpec is a variant of the possible value specifications.
// The input layer uses std::visit to call the correct implementation for the
// active struct.
using ValueSpec = std::variant<
    PermutationValueSpec,
    RangeValueSpec,
    AllEqualValueSpec,
    FewUniqueValueSpec,
    PeriodicValueSpec
>;

// Complete, renderer-independent description of a generated input.
struct SortInputSpec {
    unsigned int itemCount;
    ValueSpec valueSpec;
    InitialOrderSpec initialOrderSpec;
    std::uint32_t seed;

    bool operator==(const SortInputSpec&) const = default;
};

struct SortRunSpec {
    Algorithm algorithm;
    SortInputSpec inputSpec;

    bool operator==(const SortRunSpec&) const = default;
};
