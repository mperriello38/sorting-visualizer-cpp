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
// types, so keyboard, mouse, layout, and drawing remain in App.cpp.
class VisualizerSession {
public:
    // Public bounds are shared with controls that must present the same valid
    // range enforced by setDraftItemCount().
    static constexpr unsigned int minimumItemCount = 1;
    static constexpr unsigned int maximumItemCount = 1000;
    static constexpr unsigned int defaultFewUniqueValueCount = 5;

    explicit VisualizerSession(const SortRunSpec& initialSettings);

    const SortRunSpec& draftSettings() const;
    const SortRunSpec& loadedSettings() const;
    bool settingsDirty() const;

    const SortState& currentSortState() const;

    bool isPlaying() const;
    bool isComplete() const;
    float secondsPerEvent() const;
    std::size_t currentEventPosition() const;
    std::size_t eventCount() const;

    void setDraftAlgorithm(Algorithm algorithm);
    void setDraftValueSpec(ValueSpec valueSpec);
    void setDraftOrderSpec(InitialOrderSpec orderSpec);
    void setDraftItemCount(unsigned int itemCount);
    void applyDraftSettings();

    void togglePlayback();
    void resetPlayback();
    void stepForward();
    void stepBackward();
    void seekToEventPosition(std::size_t position);
    void changePlaybackSpeed(float delta);

    void update(float frameTime);

private:
    void loadRun();
    void keepDraftValueSpecCompatibleWithItemCount();
    
    SortRunSpec draftSettings_;
    SortRunSpec loadedSettings_;

    AnimationPlayer player_;

    bool playing_ = true;
    float eventTimer_ = 0.0f;
    float secondsPerEvent_ = 0.05f;
};
