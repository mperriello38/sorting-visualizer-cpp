#pragma once

#include <vector>

enum class TestSeverity {
    Required,
    Diagnostic
};

// Aggregate counts for a full test run.
// This stays separate from failure details so callers can quickly check the
// high-level result without parsing individual failures.
struct TestSummary {
    int requiredPassed = 0;
    int requiredFailed = 0;
    int diagnosticPassed = 0;
    int diagnosticFailed = 0;
};

// One failed check with enough context for the console runner to print a useful
// line. Individual check functions do not build this; the case runners add the
// suite/case/check context when they record a result.
struct TestFailure {
    TestSeverity severity;
    const char* suiteName;
    const char* caseName;
    const char* checkName;
    const char* message;
};

// Complete public result from a test run.
// summary gives counts; failures gives the details for checks that failed.
struct TestReport {
    TestSummary summary;
    std::vector<TestFailure> failures;
};

// Public test runner used by development code.
// These tests are not part of normal input generation, sorting, animation, or rendering.
TestReport runAllTests();
