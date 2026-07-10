#include "App.hpp"

#include <algorithm>
#include <type_traits>
#include <variant>

#include <raylib.h>
#include <vector>

#include "animation/AnimationPlayer.hpp"
#include "domain/Algorithm.hpp"
#include "domain/SortSpec.hpp"
#include "input/InputGenerator.hpp"
#include "rendering/RaylibRenderer.hpp"
#include "sorting/SortRunner.hpp"

namespace {

    struct PlaybackState {
        bool playing = true;
        float eventTimer = 0.0f;
        float secondsPerEvent = 0.05f;
    };

    // Private app state for the current keyboard-driven UI.
    //
    // Design warning: this is still small enough to keep in App.cpp. If settings,
    // playback, and drawing each grow their own rules, split those concepts
    // deliberately instead of letting AppState become a grab bag.
    struct AppState {
        SortRunSpec draftSettings;
        SortRunSpec loadedSettings;
        bool settingsDirty = false;

        PlaybackState playback;
        AnimationPlayer player;
    };

    // App-level policy constants. Keeping these named makes it clearer which
    // numbers are temporary UI/playback choices rather than domain rules.
    constexpr unsigned int minimumItemCount = 1;
    constexpr unsigned int maximumItemCount = 1000;
    constexpr unsigned int maximumFewUniqueDefaultCount = 5;

    constexpr float maximumSecondsPerEvent = 1.0f;
    constexpr float minimumSecondsPerEvent = 0.0005f;
    constexpr float secondsPerEventStep = 0.005f;

    constexpr unsigned int maximumEventsPerFrame = 50;

// =================================================================================
// Display-name helpers
// =================================================================================

    const char* algorithmName(Algorithm algorithm)
    {
        switch (algorithm) {
        case Algorithm::Insertion:
            return "Insertion";
        
        case Algorithm::Bubble:
            return "Bubble";

        case Algorithm::Selection:
            return "Selection";
        }

        // Defensive fallback for future Algorithm enum values.
        return "Unknown";
    }

    const char* valueSpecName(const ValueSpec& valueSpec)
    {
        // ValueSpec is a std::variant, not an enum. std::visit calls this lambda
        // with the actual contained spec type. if constexpr then chooses a branch
        // at compile time based on that type.
        return std::visit(
            [](const auto& spec) -> const char* {
                using Spec = std::decay_t<decltype(spec)>;

                if constexpr (std::is_same_v<Spec, PermutationValueSpec>) {
                    return "Permutation";
                }
                else if constexpr (std::is_same_v<Spec, RangeValueSpec>) {
                    return "Range";
                }
                else if constexpr (std::is_same_v<Spec, AllEqualValueSpec>) {
                    return "All Equal";
                }
                else if constexpr (std::is_same_v<Spec, FewUniqueValueSpec>) {
                    return "Few Unique";
                }
                else if constexpr (std::is_same_v<Spec, PeriodicValueSpec>) {
                    return "Periodic";
                }
                else {
                    return "Unknown";
                }
            },
            valueSpec);
    }

    const char* orderSpecName(const InitialOrderSpec& orderSpec)
    {
        return std::visit(
            [](const auto& spec) -> const char* {
                using Spec = std::decay_t<decltype(spec)>;

                if constexpr (std::is_same_v<Spec, RandomOrderSpec>) {
                    return "Random";
                }
                else if constexpr (std::is_same_v<Spec, AscendingOrderSpec>) {
                    return "Ascending";
                }
                else if constexpr (std::is_same_v<Spec, DescendingOrderSpec>) {
                    return "Descending";
                }
                else if constexpr (std::is_same_v<Spec, NearlyAscendingOrderSpec>) {
                    return "Nearly Ascending";
                }
                else {
                    return "Unknown";
                }
            },
        orderSpec);
    }

// =================================================================================
// App actions
//
// These helpers describe user intent without mentioning keyboard keys or mouse
// buttons. Keyboard handling and future custom UI controls should call these
// actions instead of duplicating state mutation.
// =================================================================================

    // Build one complete run from the current app settings and load it into the
    // animation player.
    //
    // This is the path a future item-count slider should use after it changes
    // SortInputSpec: generate new input, rerun the algorithm, then replace the
    // player's loaded trace. Keeping that sequence here prevents UI code from
    // learning the details of input generation and sorting.
    void loadRun(
        AnimationPlayer& player,
        const SortRunSpec& runSettings)
    {
        std::vector<SortItem> items = generateInput(runSettings.inputSpec);
        SortTrace trace = runSort(runSettings.algorithm, items);

        player.load(items, trace.events);
    }

    // Replace the active loaded run with the currently edited draft settings.
    //
    // This is the explicit boundary between "the user changed a setting" and
    // "the visualizer is now playing a new event trace." Keeping this transition
    // explicit prevents item-count edits from invalidating a sort mid-playback.
    void applyDraftSettings(AppState& state)
    {
        state.loadedSettings = state.draftSettings;
        loadRun(state.player, state.loadedSettings);

        state.settingsDirty = false;
        state.playback.playing = false;
        state.playback.eventTimer = 0.0f;
    }

    // Keep settingsDirty tied to the visible draft-vs-loaded model.
    //
    // Every editable setting lives inside SortRunSpec, and SortRunSpec has
    // defaulted value equality in the domain layer. If the user changes a draft
    // setting back to the loaded value, this returns settingsDirty to false.
    void refreshSettingsDirty(AppState& state)
    {
        state.settingsDirty = !(state.draftSettings == state.loadedSettings);
    }

    // Change only the draft algorithm. The active loaded run is not regenerated
    // here; ENTER applies the draft later through applyDraftSettings().
    void setDraftAlgorithm(AppState& state, Algorithm algorithm)
    {
        if (state.draftSettings.algorithm != algorithm) {
            state.draftSettings.algorithm = algorithm;
            refreshSettingsDirty(state);
        }
    }

    void setDraftValueSpec(AppState& state, ValueSpec valueSpec)
    {
        // Like algorithm, order, and item-count edits, this only changes the
        // draft run. ENTER is still responsible for applying the draft and
        // rebuilding the loaded trace.
        state.draftSettings.inputSpec.valueSpec = valueSpec;
        refreshSettingsDirty(state);
    }

    void setDraftOrderSpec(AppState& state, InitialOrderSpec orderSpec)
    {
        // Order selection changes how the next generated input is arranged.
        // It cannot be applied to the active trace without regenerating the run.
        state.draftSettings.inputSpec.initialOrderSpec = orderSpec;
        refreshSettingsDirty(state);
    }

    void keepDraftValueSpecCompatibleWithItemCount(AppState& state)
    {
        // FewUniqueValueSpec is currently the only value spec whose validity
        // depends on itemCount. For the temporary keyboard UI, "Few Unique"
        // means values come from 1..itemCount with at most five unique values.
        // Keep those derived fields synchronized when itemCount changes so the
        // draft spec cannot quietly preserve an old range.
        if (FewUniqueValueSpec* valueSpec =
                std::get_if<FewUniqueValueSpec>(&state.draftSettings.inputSpec.valueSpec)) {
            const int itemCount = static_cast<int>(state.draftSettings.inputSpec.itemCount);
            const int uniqueValueCount =
                itemCount < static_cast<int>(maximumFewUniqueDefaultCount)
                    ? itemCount
                    : static_cast<int>(maximumFewUniqueDefaultCount);

            valueSpec->minValue = 1;
            valueSpec->maxValue = itemCount;
            valueSpec->uniqueValueCount = uniqueValueCount;
        }
    }

    // Set the draft item count to an exact value. The active loaded run is not
    // regenerated here; ENTER applies the draft later through applyDraftSettings().
    //
    // A future slider should call this action directly. The keyboard nudge
    // helper below exists only to choose convenient step sizes for keys.
    void setDraftItemCount(AppState& state, unsigned int itemCount)
    {
        const unsigned int clampedItemCount = std::clamp(
            itemCount,
            minimumItemCount,
            maximumItemCount);

        if (state.draftSettings.inputSpec.itemCount != clampedItemCount) {
            state.draftSettings.inputSpec.itemCount = clampedItemCount;
            keepDraftValueSpecCompatibleWithItemCount(state);
            refreshSettingsDirty(state);
        }
    }

    unsigned int keyboardItemCountStride(unsigned int itemCount)
    {
        if (itemCount > 200) {
            return 25;
        }

        if (itemCount > 100) {
            return 10;
        }

        return 1;
    }

    // Move the draft item count up or down by a keyboard-friendly step.
    //
    // direction should be -1 to decrease or +1 to increase. Keeping direction as
    // an int is useful because unsigned item counts cannot represent "negative."
    void nudgeDraftItemCount(AppState& state, int direction)
    {
        const unsigned int currentItemCount = state.draftSettings.inputSpec.itemCount;
        const unsigned int itemStride = keyboardItemCountStride(currentItemCount);
        unsigned int nextItemCount = currentItemCount;

        if (direction < 0) {
            // itemCount is unsigned, so subtracting past zero would wrap to a
            // very large number. Clamp before subtracting to avoid underflow.
            if (currentItemCount <= minimumItemCount + itemStride) {
                nextItemCount = minimumItemCount;
            }
            else {
                nextItemCount = currentItemCount - itemStride;
            }
        }
        else if (direction > 0) {
            if (currentItemCount + itemStride >= maximumItemCount) {
                nextItemCount = maximumItemCount;
            }
            else {
                nextItemCount = currentItemCount + itemStride;
            }
        }

        setDraftItemCount(state, nextItemCount);
    }

    void togglePlayback(AppState& state)
    {
        state.playback.playing = !state.playback.playing;
        state.playback.eventTimer = 0.0f;
    }

    void resetPlayback(AppState& state)
    {
        state.player.reset();
        state.playback.eventTimer = 0.0f;
        state.playback.playing = false;
    }

    void stepPlaybackForwardWhilePaused(AppState& state)
    {
        if (!state.playback.playing) {
            state.player.stepForward();
        }
    }

    void stepPlaybackBackwardWhilePaused(AppState& state)
    {
        if (!state.playback.playing) {
            state.player.stepBackward();
        }
    }

    // Set the exact playback speed field. Smaller seconds-per-event means
    // faster playback. The clamp keeps UI controls from creating unusable speed
    // values.
    void setPlaybackSecondsPerEvent(AppState& state, float secondsPerEvent)
    {
        state.playback.secondsPerEvent = std::clamp(
            secondsPerEvent,
            minimumSecondsPerEvent,
            maximumSecondsPerEvent);
    }

    void changePlaybackSpeed(AppState& state, float secondsPerEventDelta)
    {
        setPlaybackSecondsPerEvent(
            state,
            state.playback.secondsPerEvent + secondsPerEventDelta);
    }

// =================================================================================
// Input polling
//
// These helpers translate raylib keyboard state into app actions. They should stay
// thin so future mouse/UI controls can reuse the same actions above.
// =================================================================================

    void handleItemCountInput(AppState& state)
    {
        if (IsKeyPressed(KEY_A)) {
            nudgeDraftItemCount(state, -1);
        }
        
        if (IsKeyPressed(KEY_D)) {
            nudgeDraftItemCount(state, 1);
        }
    }

    void handleAlgorithmInput(AppState& state)
    {
        if (IsKeyPressed(KEY_ONE)) {
            setDraftAlgorithm(state, Algorithm::Bubble);
        }

        if (IsKeyPressed(KEY_TWO)) {
            setDraftAlgorithm(state, Algorithm::Insertion);
        }

        if (IsKeyPressed(KEY_THREE)) {
            setDraftAlgorithm(state, Algorithm::Selection);
        }
    }

    void handleValueSpecInput(AppState& state)
    {
        if (IsKeyPressed(KEY_FOUR)) {
            // Permutation has no fields: itemCount alone defines 1..itemCount.
            setDraftValueSpec(state, PermutationValueSpec{});
        }

        if (IsKeyPressed(KEY_FIVE)) {
            // FewUniqueValueSpec needs a valid range and a valid unique count.
            // These are temporary app-level defaults for the keyboard UI, not
            // input-layer rules. uniqueValueCount is clamped to itemCount so
            // small inputs still produce a valid spec.
            const unsigned int itemCount = state.draftSettings.inputSpec.itemCount;
            const unsigned int uniqueValueCount =
                itemCount < maximumFewUniqueDefaultCount
                    ? itemCount
                    : maximumFewUniqueDefaultCount;

            setDraftValueSpec(
                state,
                FewUniqueValueSpec{
                    1,
                    static_cast<int>(itemCount),
                    static_cast<int>(uniqueValueCount)
                });
        }

        if (IsKeyPressed(KEY_SIX)) {
            // AllEqualValueSpec needs the value that every item should receive.
            setDraftValueSpec(state, AllEqualValueSpec{1});
        }
    }

    void handleOrderSpecInput(AppState& state)
    {
        if (IsKeyPressed(KEY_SEVEN)) {
            setDraftOrderSpec(state, RandomOrderSpec{});
        }

        if (IsKeyPressed(KEY_EIGHT)) {
            setDraftOrderSpec(state, AscendingOrderSpec{});
        }

        if (IsKeyPressed(KEY_NINE)) {
            setDraftOrderSpec(state, DescendingOrderSpec{});
        }

        if (IsKeyPressed(KEY_ZERO)) {
            // Temporary app default: "near" means 10% swap pressure after
            // ascending order. A future slider could edit this field directly.
            setDraftOrderSpec(state, NearlyAscendingOrderSpec{0.10});
        }
    }

    void handleApplyInput(AppState& state)
    {
        if (IsKeyPressed(KEY_ENTER) && state.settingsDirty) {
            applyDraftSettings(state);
        }
    }

    // Keyboard controls for editing draft run settings.
    //
    // These controls do not touch the active animation trace directly. They only
    // edit draft settings; ENTER is the explicit apply/regenerate action.
    void handleSettingsInput(AppState& state)
    {
        handleItemCountInput(state);
        handleAlgorithmInput(state);
        handleValueSpecInput(state);
        handleOrderSpecInput(state);
        handleApplyInput(state);
    }

    // Keyboard controls for playback of the currently loaded run.
    //
    // IsKeyPressed is true only on the frame the key changes from up to down.
    // That keeps SPACE and RIGHT from toggling/stepping many times from one held
    // key press.
    void handlePlaybackInput(AppState& state)
    {
        if (IsKeyPressed(KEY_SPACE)) {
            togglePlayback(state);
        }

        if (IsKeyPressed(KEY_R)) {
            resetPlayback(state);
        }

        if (IsKeyPressed(KEY_UP)) {
            changePlaybackSpeed(state, -secondsPerEventStep);
        }

        if (IsKeyPressed(KEY_DOWN)) {
            changePlaybackSpeed(state, secondsPerEventStep);
        }

        if (IsKeyPressed(KEY_RIGHT)) {
            stepPlaybackForwardWhilePaused(state);
        }

        if (IsKeyPressed(KEY_LEFT)) {
            stepPlaybackBackwardWhilePaused(state);
        }
    }

    void updatePlayback(AppState& state)
    {
        // Cap replay work per rendered frame so high item counts and very fast
        // playback do not make the app feel frozen. This is a responsiveness
        // policy, not a sorting rule; lowering it makes max speed smoother but
        // slower, raising it finishes traces faster but risks frame drops.
        unsigned int eventsAppliedThisFrame = 0;

        // Realtime playback is just repeated calls to stepForward() controlled
        // by elapsed frame time. While paused, time does not accumulate.
        if (state.playback.playing && !state.player.isComplete()) {
            state.playback.eventTimer += GetFrameTime();

            // Use a loop so very small secondsPerEvent values can advance more
            // than one event per rendered frame. Subtracting preserves leftover
            // fractional time instead of throwing it away each step.
            while (
                state.playback.eventTimer >= state.playback.secondsPerEvent &&
                !state.player.isComplete() &&
                eventsAppliedThisFrame < maximumEventsPerFrame) {
                state.player.stepForward();
                state.playback.eventTimer -= state.playback.secondsPerEvent;
                eventsAppliedThisFrame += 1;
            }
        }
    }

    Rectangle calculateAppLayout(const int& screenwidth, const int& screenheight)
    {
        // Header / Control Area
        // Roughly 10% of screen height

        // Chart area
        // Header and footer determine chart height
        // Left and right margines determine chart width

        // Footer / Status-settings Area
        // Roughly 5% of screen height
    }

    void drawControls()
    {
        DrawText("Controls:", 345, 50, 15, DARKGRAY);

        DrawText("Pause:", 417, 54, 10, DARKGRAY);
        DrawText("SPACE", 454, 54, 10, BLUE);

        DrawText("Back:", 499, 54, 10, DARKGRAY);
        DrawText("LEFT", 532, 54, 10, BLUE);

        DrawText("Forward:", 574, 54, 10, DARKGRAY);
        DrawText("RIGHT", 625, 54, 10, BLUE);

        DrawText("Speed +:", 668, 54, 10, DARKGRAY);
        DrawText("UP", 714, 54, 10, BLUE);

        DrawText("Speed -:", 742, 54, 10, DARKGRAY);
        DrawText("DOWN", 790, 54, 10, BLUE);

        DrawText("Reset:", 835, 54, 10, DARKGRAY);
        DrawText("R", 871, 54, 10, BLUE);

        DrawText("Item -:", 417, 74, 10, DARKGRAY);
        DrawText("A", 455, 74, 10, BLUE);

        DrawText("Item +:", 499, 74, 10, DARKGRAY);
        DrawText("D", 537, 74, 10, BLUE);

        DrawText("Apply:", 574, 74, 10, DARKGRAY);
        DrawText("ENTER", 615, 74, 10, BLUE);

        DrawText("Bubble:", 680, 74, 10, DARKGRAY);
        DrawText("1", 725, 74, 10, BLUE);

        DrawText("Insertion:", 745, 74, 10, DARKGRAY);
        DrawText("2", 807, 74, 10, BLUE);

        DrawText("Selection:", 827, 74, 10, DARKGRAY);
        DrawText("3", 887, 74, 10, BLUE);

        DrawText("Values:", 499, 94, 10, DARKGRAY);
        DrawText("4 Perm", 545, 94, 10, BLUE);
        DrawText("5 Few", 600, 94, 10, BLUE);
        DrawText("6 Equal", 648, 94, 10, BLUE);

        DrawText("Order:", 715, 94, 10, DARKGRAY);
        DrawText("7 Rand", 755, 94, 10, BLUE);
        DrawText("8 Asc", 812, 94, 10, BLUE);
        DrawText("9 Desc", 860, 94, 10, BLUE);
        DrawText("0 Near", 920, 94, 10, BLUE);
    }

    void drawPlaybackStatus(const AppState& state)
    {
        if (state.player.isComplete()) {
            DrawText("Complete", 40, 90, 20, BLUE);
        }
        else if (state.playback.playing) {
            DrawText("Playing", 40, 90, 20, GREEN);
        }
        else {
            DrawText("Paused", 40, 90, 20, RED);
        }

        DrawText("Seconds per Event:", 180, 90, 20, DARKGRAY);
        DrawText(TextFormat("%.4f", state.playback.secondsPerEvent), 385, 90, 20, GREEN);
    }

    void drawSettingsStatus(const AppState& state)
    {
        DrawText("Loaded algorithm:", 40, 600, 18, DARKGRAY);
        DrawText(
            algorithmName(state.loadedSettings.algorithm),
            205,
            600,
            18,
            BLUE);

        DrawText("Draft algorithm:", 310, 600, 18, DARKGRAY);
        DrawText(
            algorithmName(state.draftSettings.algorithm),
            465,
            600,
            18,
            state.settingsDirty ? RED : GREEN);

        DrawText("Loaded values:", 40, 620, 18, DARKGRAY);
        DrawText(
            valueSpecName(state.loadedSettings.inputSpec.valueSpec),
            205,
            620,
            18,
            BLUE);

        DrawText("Draft values:", 310, 620, 18, DARKGRAY);
        DrawText(
            valueSpecName(state.draftSettings.inputSpec.valueSpec),
            465,
            620,
            18,
            state.settingsDirty ? RED : GREEN);

        DrawText("Loaded order:", 40, 640, 18, DARKGRAY);
        DrawText(
            orderSpecName(state.loadedSettings.inputSpec.initialOrderSpec),
            205,
            640,
            18,
            BLUE);

        DrawText("Draft order:", 310, 640, 18, DARKGRAY);
        DrawText(
            orderSpecName(state.draftSettings.inputSpec.initialOrderSpec),
            465,
            640,
            18,
            state.settingsDirty ? RED : GREEN);

        DrawText("Loaded item count:", 40, 660, 18, DARKGRAY);
        DrawText(
            TextFormat("%u", state.loadedSettings.inputSpec.itemCount),
            215,
            660,
            18,
            BLUE);

        DrawText("Draft item count:", 310, 660, 18, DARKGRAY);
        DrawText(
            TextFormat("%u", state.draftSettings.inputSpec.itemCount),
            465,
            660,
            18,
            state.settingsDirty ? RED : GREEN);

        if (state.settingsDirty) {
            DrawText("Settings changed: press ENTER to apply", 560, 680, 18, RED);
        }
        else {
            DrawText("Settings applied", 560, 680, 18, GREEN);
        }
    }

    // Draw the current app frame.
    //
    // This is still app-level drawing, not renderer internals: it draws title,
    // controls, and status text, then asks RaylibRenderer to draw the SortState.
    //
    // Design warning: this helper is allowed to know app status text, but it
    // should not grow into custom UI widgets. If the controls become interactive
    // mouse elements, move that behavior behind smaller helpers instead of adding
    // more logic here.
    void drawApp(const AppState& state, const RaylibRenderer& renderer)
    {
        BeginDrawing();

        ClearBackground(RAYWHITE);

        DrawText("Sorting Visualizer", 40, 40, 32, DARKGRAY);

        drawControls();
        drawPlaybackStatus(state);
        drawSettingsStatus(state);

        renderer.drawSortState(state.player.currentState());

        EndDrawing();
    }
}

void App::run()
{
    const int screenWidth = 3440;
    const int screenHeight = 1760;

    InitWindow(screenWidth, screenHeight, "Sorting Visualizer");

    SetTargetFPS(60);

    const SortRunSpec initialSettings{
        Algorithm::Insertion,
        SortInputSpec{
            50,
            PermutationValueSpec{},
            RandomOrderSpec{},
            12345
        }
    };

    AppState state{
        initialSettings,
        initialSettings,
        false,
        PlaybackState{},
        AnimationPlayer{}
    };

    loadRun(state.player, state.loadedSettings);
    RaylibRenderer renderer;

    while (!WindowShouldClose()) {
        handleSettingsInput(state);
        handlePlaybackInput(state);
        updatePlayback(state);
        drawApp(state, renderer);
    }

    CloseWindow();
}
