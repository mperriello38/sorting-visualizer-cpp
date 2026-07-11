#include "VisualizerSession.hpp"

#include "input/InputGenerator.hpp"
#include "sorting/SortRunner.hpp"

#include <algorithm>
#include <vector>

namespace {

constexpr float maximumSecondsPerEvent = 1.0f;
constexpr float minimumSecondsPerEvent = 0.0005f;

constexpr unsigned int maximumEventsPerFrame = 50;

}

VisualizerSession::VisualizerSession(const SortRunSpec& initialSettings)
    : draftSettings_(initialSettings),
      loadedSettings_(initialSettings)
{
    // A newly constructed session should already contain a usable sort trace.
    // Keeping initialization here prevents callers from having to remember a
    // separate load step after construction.
    loadRun();
}

void VisualizerSession::loadRun()
{
    // Regenerate input and sorting events from the settings that describe the
    // active run. This helper is the only place where the session needs to know
    // the sequence connecting input generation, sorting, and animation loading.
    std::vector<SortItem> items = generateInput(loadedSettings_.inputSpec);
    
    SortTrace trace = runSort(loadedSettings_.algorithm, items);
    
    player_.load(items, trace.events);
}

// =============================================================================
// Read-only session state
// =============================================================================

const SortRunSpec& VisualizerSession::draftSettings() const
{
    // Return a const reference so the UI can display draft settings without
    // copying them or bypassing the session's setting-change functions.
    return draftSettings_;
}

const SortRunSpec& VisualizerSession::loadedSettings() const
{
    // Loaded settings describe the input and algorithm used by the active
    // animation trace.
    return loadedSettings_;
}

bool VisualizerSession::settingsDirty() const
{
    // Dirty state is derived instead of stored. It therefore cannot become
    // inconsistent with the actual draft and loaded settings.
    return !(draftSettings_ == loadedSettings_);
}

const SortState& VisualizerSession::currentSortState() const
{
    // AnimationPlayer owns replay state; the session only exposes its current
    // visual snapshot as read-only data for rendering.
    return player_.currentState();
}

bool VisualizerSession::isPlaying() const
{
    return playing_;
}

bool VisualizerSession::isComplete() const
{
    return player_.isComplete();
}

float VisualizerSession::secondsPerEvent() const
{
    return secondsPerEvent_;
}

std::size_t VisualizerSession::currentEventPosition() const
{
    return player_.currentEventPosition();
}

std::size_t VisualizerSession::eventCount() const
{
    return player_.eventCount();
}

// =============================================================================
// Draft settings and run loading
// =============================================================================

void VisualizerSession::setDraftAlgorithm(Algorithm algorithm)
{
    // Change only the draft algorithm. The loaded trace must remain unchanged
    // until applyDraftSettings() is called.
    draftSettings_.algorithm = algorithm;
}

void VisualizerSession::setDraftValueSpec(ValueSpec valueSpec)
{
    // Replace only the draft value specification. This function should not
    // generate input or modify the currently loaded animation.
    draftSettings_.inputSpec.valueSpec = valueSpec;
}

void VisualizerSession::setDraftOrderSpec(InitialOrderSpec orderSpec)
{
    // Replace only the draft ordering specification. Applying the draft later
    // will regenerate the active run with this ordering.
    draftSettings_.inputSpec.initialOrderSpec = orderSpec;
}

void VisualizerSession::setDraftItemCount(unsigned int itemCount)
{
    const unsigned int clampedItemCount = std::clamp(
        itemCount,
        VisualizerSession::minimumItemCount,
        VisualizerSession::maximumItemCount
    );

    if (draftSettings_.inputSpec.itemCount != clampedItemCount) {
        draftSettings_.inputSpec.itemCount = clampedItemCount;
        keepDraftValueSpecCompatibleWithItemCount();
    }
}

void VisualizerSession::keepDraftValueSpecCompatibleWithItemCount()
{
    // FewUniqueValueSpec is currently the only UI-exposed value mode whose
    // fields are derived from item count. Keep its range and unique-value count
    // valid when the draft item count changes.
    if (FewUniqueValueSpec* valueSpec =
            std::get_if<FewUniqueValueSpec>(&draftSettings_.inputSpec.valueSpec)) {
        const int itemCount =
            static_cast<int>(draftSettings_.inputSpec.itemCount);
        const int uniqueValueCount = std::min(
            itemCount,
            static_cast<int>(VisualizerSession::defaultFewUniqueValueCount));

        valueSpec->minValue = 1;
        valueSpec->maxValue = itemCount;
        valueSpec->uniqueValueCount = uniqueValueCount;
    }
}

void VisualizerSession::applyDraftSettings()
{
    // Promote the draft to the loaded settings, build a new run, and leave the
    // new trace paused at its starting position with no accumulated frame time.
    loadedSettings_ = draftSettings_;
    loadRun();

    playing_ = false;
    eventTimer_ = 0.0f;
}

// =============================================================================
// Playback commands
// =============================================================================

void VisualizerSession::togglePlayback()
{
    // Switch between playing and paused, and clear accumulated timing so an old
    // partial interval cannot cause an immediate event after the transition.
    playing_ = !playing_;
    eventTimer_ = 0.0f;
}

void VisualizerSession::resetPlayback()
{
    // Return the player to event position zero, clear timing, and leave playback
    // paused so reset has a predictable visible result.
    player_.reset();
    playing_ = false;
    eventTimer_ = 0.0f;
}

void VisualizerSession::stepForward()
{
    // Apply exactly one event when manual stepping is allowed. The final policy
    // should make the behavior while already playing explicit.
    if (!playing_) {
        player_.stepForward();
    }
}

void VisualizerSession::stepBackward()
{
    // Move exactly one event position backward through AnimationPlayer's seek
    // behavior. Do not attempt to reverse individual SortEvents here.
    if (!playing_) {
        player_.stepBackward();
    }
}

void VisualizerSession::seekToEventPosition(std::size_t position)
{
    // Ask AnimationPlayer to seek to the requested event position. This is the
    // command that a future timeline slider will use.
    playing_ = false;
    eventTimer_ = 0.0f;
    player_.seekToEventPosition(position);
}

void VisualizerSession::changePlaybackSpeed(float delta)
{
    secondsPerEvent_ = std::clamp(
        secondsPerEvent_ + delta,
        minimumSecondsPerEvent,
        maximumSecondsPerEvent);
}

// =============================================================================
// Realtime playback update
// =============================================================================

void VisualizerSession::update(float frameTime)
{
    unsigned int eventsAppliedThisFrame = 0;

    if (!playing_ || player_.isComplete()) {
        return;
    }

    eventTimer_ += frameTime;

    while (
        eventTimer_ >= secondsPerEvent_ &&
        !player_.isComplete() &&
        eventsAppliedThisFrame < maximumEventsPerFrame) {
        player_.stepForward();
        eventTimer_ -= secondsPerEvent_;
        ++eventsAppliedThisFrame;
    }
}
