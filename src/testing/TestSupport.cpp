#include "TestSupport.hpp"

#include <cstddef>

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
