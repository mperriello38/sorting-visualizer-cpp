#include "TestSupport.hpp"

#include "animation/AnimationPlayer.hpp"
#include "sorting/SortRunner.hpp"

#include <cstddef>
#include <vector>

namespace {

    // =================================================================================
    // Required animation tests
    // =================================================================================

    // These helpers are animation-specific, so they live near the animation
    // tests instead of in the shared helper section at the top of the file.
    //
    // If another test section later needs the same checks, that will be the
    // signal to move them upward into "Shared test helpers."


    // Completion and event-position checks are local to animation tests. Item
    // and visual-state comparisons are shared with session tests through
    // TestSupport.

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
