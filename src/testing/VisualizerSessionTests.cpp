#include "TestSupport.hpp"

#include "app/VisualizerSession.hpp"
#include "input/InputGenerator.hpp"
#include "sorting/SortRunner.hpp"

#include <cstddef>
#include <variant>
#include <vector>

namespace {

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
