#include "SortTests.hpp"

// Suite runners are implemented in separate .cpp files. They stay out of the
// public header because production code and the console runner need only
// runAllTests().
TestReport runInputGeneratorTests();
TestReport runSortingTests();
TestReport runAnimationTests();
TestReport runVisualizerSessionTests();

namespace {

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

}

TestReport runAllTests()
{
    TestReport report;

    addReport(report, runInputGeneratorTests());
    addReport(report, runSortingTests());
    addReport(report, runAnimationTests());
    addReport(report, runVisualizerSessionTests());

    return report;
}
