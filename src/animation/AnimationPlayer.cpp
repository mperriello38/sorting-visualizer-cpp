#include "AnimationPlayer.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <variant>

namespace {

struct AppliedEventVisuals {
    std::vector<std::size_t> transientIndices;
    ItemVisualState transientState = ItemVisualState::Normal;
};

// Convert event indices into vector indices at the animation boundary.
// Sorting events store indices as int, while std::vector uses std::size_t.
// Checking here prevents negative or too-large event indices from turning into
// confusing out-of-bounds vector access later in the function.
std::size_t requireValidIndex(int index, std::size_t itemCount)
{
    if (index < 0 || static_cast<std::size_t>(index) >= itemCount) {
        throw std::out_of_range("SortEvent index is outside the current item range.");
    }

    return static_cast<std::size_t>(index);
}

// Build the output snapshot consumed by rendering.
//
// currentItems and sorted are the hidden playback state. transientIndices and
// transientState describe the event that was just applied. This helper keeps
// SortState as an output view instead of a second source of truth.
SortState rebuildCurrentState(
    const std::vector<SortItem>& currentItems,
    const std::vector<bool>& sorted,
    const std::vector<std::size_t>& transientIndices,
    ItemVisualState transientState,
    bool complete)
{
    SortState state;
    state.items.reserve(currentItems.size());

    for (std::size_t index = 0; index < currentItems.size(); ++index) {
        const SortItem& item = currentItems[index];

        ItemVisualState visualState = ItemVisualState::Normal;

        // Sorted is persistent: once an index is marked sorted, it stays sorted
        // until reset() or load() replaces the trace.
        if (sorted[index]) {
            visualState = ItemVisualState::Sorted;
        }

        // Transient event state wins for the current frame. On the next event,
        // this mark disappears unless the new event marks the same index again.
        if (std::find(transientIndices.begin(), transientIndices.end(), index) != transientIndices.end()) {
            visualState = transientState;
        }

        state.items.push_back(SortItemState{
            item,
            visualState
        });
    }

    state.complete = complete;

    return state;
}

// Apply one SortEvent to hidden animation state.
//
// currentItems and sorted are mutated because they are the source of truth for
// playback. The returned AppliedEventVisuals describes only the transient mark
// caused by this event. Keeping this event logic in one helper prevents
// stepForward() and seekToEventPosition() from developing different meanings for
// the same SortEvent.
AppliedEventVisuals applyEventToState(
    const SortEvent& event,
    std::vector<SortItem>& currentItems,
    std::vector<bool>& sorted)
{
    AppliedEventVisuals visuals;

    std::visit(
        [&](const auto& activeEvent) {
            using EventType = std::decay_t<decltype(activeEvent)>;

            if constexpr (std::is_same_v<EventType, CompareEvent>) {
                const std::size_t leftIndex = requireValidIndex(
                    activeEvent.leftIndex,
                    currentItems.size());
                const std::size_t rightIndex = requireValidIndex(
                    activeEvent.rightIndex,
                    currentItems.size());

                // A compare event only changes the visual mark for this frame.
                // It does not move items.
                visuals.transientState = ItemVisualState::Comparing;
                visuals.transientIndices = {
                    leftIndex,
                    rightIndex
                };
            }
            else if constexpr (std::is_same_v<EventType, SwapEvent>) {
                const std::size_t leftIndex = requireValidIndex(
                    activeEvent.leftIndex,
                    currentItems.size());
                const std::size_t rightIndex = requireValidIndex(
                    activeEvent.rightIndex,
                    currentItems.size());

                // A swap event changes the logical item order, then marks the
                // affected positions for rendering.
                std::swap(currentItems[leftIndex], currentItems[rightIndex]);

                visuals.transientIndices = {
                    leftIndex,
                    rightIndex
                };
                visuals.transientState = ItemVisualState::Swapping;
            }
            else if constexpr (std::is_same_v<EventType, MoveEvent>) {
                const std::size_t destinationIndex = requireValidIndex(
                    activeEvent.destinationIndex,
                    currentItems.size());

                // A move event places a whole SortItem at one destination.
                // This supports insertion-style shifts without pretending they
                // are swaps.
                currentItems[destinationIndex] = activeEvent.movedItem;

                visuals.transientIndices = {destinationIndex};
                visuals.transientState = ItemVisualState::Moving;
            }
            else if constexpr (std::is_same_v<EventType, MarkSortedEvent>) {
                const std::size_t index = requireValidIndex(
                    activeEvent.index,
                    currentItems.size());

                // Sorted is persistent state, so it is stored separately from
                // transient compare/swap/move marks.
                sorted[index] = true;
            }
        },
        event);

    return visuals;
}

}

void AnimationPlayer::load(
    const std::vector<SortItem>& initialItems,
    const std::vector<SortEvent>& events)
{
    initialItems_ = initialItems;
    events_ = events;

    reset();
}

const SortState& AnimationPlayer::currentState() const
{
    return currentState_;
}

bool AnimationPlayer::isComplete() const
{
    return nextEventIndex_ >= events_.size();
}

void AnimationPlayer::reset()
{
    seekToEventPosition(0);
}

void AnimationPlayer::stepForward()
{
    if (isComplete()) {
        return;
    }

    const SortEvent& event = events_[nextEventIndex_];
    const AppliedEventVisuals visuals = applyEventToState(
        event,
        currentItems_,
        sorted_);

    ++nextEventIndex_;

    currentState_ = rebuildCurrentState(
        currentItems_,
        sorted_,
        visuals.transientIndices,
        visuals.transientState,
        isComplete());
}

void AnimationPlayer::seekToEventPosition(std::size_t eventPosition)
{
    // Clamp out-of-range seek requests. This is friendly to future sliders,
    // where slightly overshooting the end should mean "go to completion," not
    // "throw an exception." Invalid indices inside SortEvents still throw when
    // applyEventToState validates them.
    const std::size_t targetPosition = std::min(eventPosition, events_.size());

    std::vector<SortItem> replayItems = initialItems_;
    std::vector<bool> replaySorted(initialItems_.size(), false);
    AppliedEventVisuals visuals;

    // Replay into local state first. If a malformed event throws, the current
    // player state is not left half-mutated.
    for (std::size_t eventIndex = 0; eventIndex < targetPosition; ++eventIndex) {
        visuals = applyEventToState(
            events_[eventIndex],
            replayItems,
            replaySorted);
    }

    currentItems_ = replayItems;
    sorted_ = replaySorted;
    nextEventIndex_ = targetPosition;

    currentState_ = rebuildCurrentState(
        currentItems_,
        sorted_,
        visuals.transientIndices,
        visuals.transientState,
        isComplete());
}

void AnimationPlayer::stepBackward()
{
    if (nextEventIndex_ == 0) {
        return;
    }

    seekToEventPosition(nextEventIndex_ - 1);
}

std::size_t AnimationPlayer::currentEventPosition() const
{
    return nextEventIndex_;
}

std::size_t AnimationPlayer::eventCount() const
{
    return events_.size();
}
