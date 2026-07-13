#pragma once

#include <cstddef>

#include "animation/AnimationPlayer.hpp"
#include "domain/Algorithm.hpp"
#include "domain/SortSpec.hpp"
#include "domain/SortState.hpp"

// Pure C++ application state for one interactive visualizer session.
//
// VisualizerSession owns draft-vs-loaded settings, run regeneration, playback
// commands, and realtime event advancement. It deliberately exposes no raylib
// types, so keyboard, mouse, layout, and drawing remain in AppUi.
class VisualizerSession {
public:
    // Public bounds are shared with controls that must present the same valid
    // range enforced by setDraftItemCount().
    static constexpr unsigned int minimumItemCount = 1;
    static constexpr unsigned int maximumItemCount = 1000;

    // Default used when item-count edits repair a FewUniqueValueSpec draft.
    static constexpr unsigned int defaultFewUniqueValueCount = 5;

    // Construct a loaded trace at event position zero with playback enabled.
    explicit VisualizerSession(const SortRunSpec& initialSettings);

    // Draft settings are edited by controls; loaded settings describe the
    // currently active trace. Neither accessor permits callers to mutate them.
    const SortRunSpec& draftSettings() const;
    const SortRunSpec& loadedSettings() const;

    // Report whether the draft differs from the settings behind the active run.
    bool settingsDirty() const;

    // Expose the player's current snapshot without exposing AnimationPlayer.
    const SortState& currentSortState() const;

    // Read-only playback state used by controls and status drawing.
    bool isPlaying() const;
    bool isComplete() const;
    float secondsPerEvent() const;
    std::size_t currentEventPosition() const;
    std::size_t eventCount() const;

    // Setting edits affect only the draft until applyDraftSettings() is called.
    void setDraftAlgorithm(Algorithm algorithm);
    void setDraftValueSpec(ValueSpec valueSpec);
    void setDraftOrderSpec(InitialOrderSpec orderSpec);

    // Clamp the count to the public bounds and keep dependent draft fields valid.
    void setDraftItemCount(unsigned int itemCount);

    // Promote the draft, generate a new trace, and pause at event position zero.
    void applyDraftSettings();

    void togglePlayback();

    // Return to event position zero and pause.
    void resetPlayback();

    // Manual steps are ignored while automatic playback is active.
    void stepForward();
    void stepBackward();

    // Pause and seek; AnimationPlayer clamps positions beyond the trace.
    void seekToEventPosition(std::size_t position);

    // Adjust seconds per event. Negative deltas are faster; positive are slower.
    void changePlaybackSpeed(float delta);

    // Advance automatic playback using nonnegative elapsed frame time. Work per
    // call is capped so a timing backlog cannot monopolize the UI thread.
    void update(float frameTime);

private:
    // Build and load a trace from loadedSettings_.
    void loadRun();

    // Repair draft value-spec fields whose valid range depends on item count.
    void keepDraftValueSpecCompatibleWithItemCount();

    SortRunSpec draftSettings_;
    SortRunSpec loadedSettings_;

    AnimationPlayer player_;

    bool playing_ = true;

    // Unconsumed elapsed time carried between frames.
    float eventTimer_ = 0.0f;
    float secondsPerEvent_ = 0.05f;
};
