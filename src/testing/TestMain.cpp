#include "testing/SortTests.hpp"

#include <iostream>

namespace {

    const char* severityName(TestSeverity severity)
    {
        switch (severity) {
            case TestSeverity::Required:
                return "REQUIRED";

            case TestSeverity::Diagnostic:
                return "DIAGNOSTIC";
        }

        return "UNKNOWN";
    }

}

int main()
{
    TestReport report = runAllTests();
    const TestSummary& summary = report.summary;

    std::cout << "Required passed: " << summary.requiredPassed << '\n';
    std::cout << "Required failed: " << summary.requiredFailed << '\n';
    std::cout << "Diagnostic passed: " << summary.diagnosticPassed << '\n';
    std::cout << "Diagnostic failed: " << summary.diagnosticFailed << '\n';

    if (!report.failures.empty()) {
        std::cout << "\nFailures:\n";

        for (const TestFailure& failure : report.failures) {
            std::cout
                << severityName(failure.severity) << " failed: "
                << failure.suiteName << " / "
                << failure.caseName << " / "
                << failure.checkName << " - "
                << failure.message << '\n';
        }
    }

    if (summary.requiredFailed > 0) {
        return 1;
    }

    return 0;
}
