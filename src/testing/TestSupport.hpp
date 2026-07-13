#pragma once

#include "domain/SortItem.hpp"
#include "domain/SortState.hpp"
#include "testing/SortTests.hpp"

#include <vector>

// Internal result type shared by test-suite implementations. This header is
// not part of the public testing interface; external callers use runAllTests().
struct TestResult {
    bool passed;
    const char* message;
};

void recordRequired(
    TestReport& report,
    const char* suiteName,
    const char* caseName,
    const char* checkName,
    const TestResult& result);

void recordDiagnostic(
    TestReport& report,
    const char* suiteName,
    const char* caseName,
    const char* checkName,
    const TestResult& result);

TestResult checkStateItems(
    const SortState& state,
    const std::vector<SortItem>& expectedItems);

TestResult checkVisualStates(
    const SortState& state,
    const std::vector<ItemVisualState>& expectedVisualStates);
