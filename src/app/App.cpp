#include "App.hpp"

#include <algorithm>
#include <type_traits>
#include <variant>

#include <raylib.h>

#include "VisualizerSession.hpp"
#include "domain/Algorithm.hpp"
#include "domain/SortSpec.hpp"
#include "rendering/RaylibRenderer.hpp"

namespace {

    // App-level policy constants. Keeping these named makes it clearer which
    // numbers are keyboard/UI choices rather than domain rules.
    constexpr float secondsPerEventStep = 0.005f;

    constexpr bool showLayoutDebug = false;

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
// Keyboard-specific settings policy
//
// VisualizerSession owns settings validity and state transitions. These helpers
// only translate keyboard-friendly changes into session commands.
// =================================================================================

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
    void nudgeDraftItemCount(VisualizerSession& session, int direction)
    {
        const unsigned int currentItemCount =
            session.draftSettings().inputSpec.itemCount;
        const unsigned int itemStride = keyboardItemCountStride(currentItemCount);
        unsigned int nextItemCount = currentItemCount;

        if (direction < 0) {
            // itemCount is unsigned, so subtracting past zero would wrap to a
            // very large number. Clamp before subtracting to avoid underflow.
            if (currentItemCount <= VisualizerSession::minimumItemCount + itemStride) {
                nextItemCount = VisualizerSession::minimumItemCount;
            }
            else {
                nextItemCount = currentItemCount - itemStride;
            }
        }
        else if (direction > 0) {
            if (currentItemCount + itemStride >= VisualizerSession::maximumItemCount) {
                nextItemCount = VisualizerSession::maximumItemCount;
            }
            else {
                nextItemCount = currentItemCount + itemStride;
            }
        }

        session.setDraftItemCount(nextItemCount);
    }

// =================================================================================
// Input polling
//
// These helpers translate raylib keyboard state into app actions. They should stay
// thin so future mouse/UI controls can issue the same session commands.
// =================================================================================

    void handleItemCountInput(VisualizerSession& session)
    {
        if (IsKeyPressed(KEY_A)) {
            nudgeDraftItemCount(session, -1);
        }
        
        if (IsKeyPressed(KEY_D)) {
            nudgeDraftItemCount(session, 1);
        }
    }

    void handleAlgorithmInput(VisualizerSession& session)
    {
        if (IsKeyPressed(KEY_ONE)) {
            session.setDraftAlgorithm(Algorithm::Bubble);
        }

        if (IsKeyPressed(KEY_TWO)) {
            session.setDraftAlgorithm(Algorithm::Insertion);
        }

        if (IsKeyPressed(KEY_THREE)) {
            session.setDraftAlgorithm(Algorithm::Selection);
        }
    }

    void handleValueSpecInput(VisualizerSession& session)
    {
        if (IsKeyPressed(KEY_FOUR)) {
            // Permutation has no fields: itemCount alone defines 1..itemCount.
            session.setDraftValueSpec(PermutationValueSpec{});
        }

        if (IsKeyPressed(KEY_FIVE)) {
            // FewUniqueValueSpec needs a valid range and a valid unique count.
            // These are app-level defaults for the keyboard UI, not input-layer
            // rules. uniqueValueCount is clamped to itemCount so small inputs
            // still produce a valid spec.
            const unsigned int itemCount =
                session.draftSettings().inputSpec.itemCount;
            const unsigned int uniqueValueCount =
                itemCount < VisualizerSession::defaultFewUniqueValueCount
                    ? itemCount
                    : VisualizerSession::defaultFewUniqueValueCount;

            session.setDraftValueSpec(
                FewUniqueValueSpec{
                    1,
                    static_cast<int>(itemCount),
                    static_cast<int>(uniqueValueCount)
                });
        }

        if (IsKeyPressed(KEY_SIX)) {
            // AllEqualValueSpec needs the value that every item should receive.
            session.setDraftValueSpec(AllEqualValueSpec{1});
        }
    }

    void handleOrderSpecInput(VisualizerSession& session)
    {
        if (IsKeyPressed(KEY_SEVEN)) {
            session.setDraftOrderSpec(RandomOrderSpec{});
        }

        if (IsKeyPressed(KEY_EIGHT)) {
            session.setDraftOrderSpec(AscendingOrderSpec{});
        }

        if (IsKeyPressed(KEY_NINE)) {
            session.setDraftOrderSpec(DescendingOrderSpec{});
        }

        if (IsKeyPressed(KEY_ZERO)) {
            // Keyboard policy: "near" means 10% swap pressure after ascending
            // order. A parameter control can eventually replace this fixed value.
            session.setDraftOrderSpec(NearlyAscendingOrderSpec{0.10});
        }
    }

    void handleApplyInput(VisualizerSession& session)
    {
        if (IsKeyPressed(KEY_ENTER) && session.settingsDirty()) {
            session.applyDraftSettings();
        }
    }

    // Keyboard controls for editing draft run settings.
    //
    // These controls do not touch the active animation trace directly. They only
    // edit draft settings; ENTER is the explicit apply/regenerate action.
    void handleSettingsInput(VisualizerSession& session)
    {
        handleItemCountInput(session);
        handleAlgorithmInput(session);
        handleValueSpecInput(session);
        handleOrderSpecInput(session);
        handleApplyInput(session);
    }

    // Keyboard controls for playback of the currently loaded run.
    //
    // IsKeyPressed is true only on the frame the key changes from up to down.
    // That keeps SPACE and RIGHT from toggling/stepping many times from one held
    // key press.
    void handlePlaybackInput(VisualizerSession& session)
    {
        if (IsKeyPressed(KEY_SPACE)) {
            session.togglePlayback();
        }

        if (IsKeyPressed(KEY_R)) {
            session.resetPlayback();
        }

        if (IsKeyPressed(KEY_UP)) {
            session.changePlaybackSpeed(-secondsPerEventStep);
        }

        if (IsKeyPressed(KEY_DOWN)) {
            session.changePlaybackSpeed(secondsPerEventStep);
        }

        if (IsKeyPressed(KEY_RIGHT)) {
            session.stepForward();
        }

        if (IsKeyPressed(KEY_LEFT)) {
            session.stepBackward();
        }
    }

    struct AppLayout {
        Rectangle headerBounds;
        Rectangle titleBounds;
        Rectangle controlsBounds;
        Rectangle playbackStatusBounds;
        Rectangle chartBounds;
        Rectangle footerBounds;
    };
    AppLayout calculateAppLayout(int screenWidth, int screenHeight)
    {

        const float width = static_cast<float>(screenWidth);
        const float height = static_cast<float>(screenHeight);

        float padding = 24.0f;

        // Header area.
        // Roughly 15% of screen height.
        float headerWidth = width - 2.0f * padding;
        float headerHeight = 0.15f * height - padding;
        Rectangle header = {padding, padding, headerWidth, headerHeight};

        // Header subdivision.
        // The title gets the left side. The right side is split vertically so
        // controls and playback status do not rely on a hidden agreement about
        // which part of the header is safe to use.
        float titleWidth = 0.25f * headerWidth;
        Rectangle title = {header.x, header.y, titleWidth, headerHeight};

        float controlsWidth = headerWidth - titleWidth ;
        float controlsHeight = 0.70f * headerHeight;
        Rectangle controls = {
            header.x + titleWidth,
            header.y,
            controlsWidth,
            controlsHeight
        };

        Rectangle playbackStatus = {
            controls.x,
            controls.y + controls.height,
            controls.width,
            headerHeight - controlsHeight
        };

        // Footer / Status-settings area.
        // Roughly 15% of screen height.
        float footerWidth = width - 2.0f * padding;
        float footerHeight = 0.15f * height - padding;
        Rectangle footer = {padding, height - footerHeight - padding, footerWidth, footerHeight};

        // Chart area
        // Header and footer determine chart height
        // Left and right padding determine chart width.
        float chartY = header.y + header.height + padding;
        float chartBottom = footer.y - padding;
        float chartHeight = chartBottom - chartY;
        float chartWidth = width - 2.0f * padding;

        Rectangle chart = {padding, chartY, chartWidth, chartHeight};

        return AppLayout{header, title, controls, playbackStatus, chart, footer};
    }

    void drawTitle(Rectangle titleBounds)
    {
        const char* titleText = "Sorting Visualizer";

        // Start from the title area's height, but leave vertical breathing room.
        // raylib's font size is not exactly "text height in pixels", so using
        // the full rectangle height tends to make the title look cramped.
        int fontSize = static_cast<int>(titleBounds.height * 0.85f);
        fontSize = std::clamp(fontSize, 18, 96);

        // Height chooses the first guess. Width gets the final veto so the
        // title cannot spill into the controls area on narrower windows.
        while (fontSize > 1 && MeasureText(titleText, fontSize) > titleBounds.width) {
            --fontSize;
        }

        // Vertically center the title inside its rectangle. This keeps the title
        // anchored to the layout box instead of to a guessed pixel coordinate.
        const float textHeight = static_cast<float>(fontSize);
        const int textX = static_cast<int>(titleBounds.x);
        const int textY = static_cast<int>(
            titleBounds.y + (titleBounds.height - textHeight) / 2.0f);

        DrawText(titleText, textX, textY, fontSize, DARKGRAY);
    }

    float textYForRow(Rectangle row, int fontSize)
    {
        // DrawText uses y as the top of the text. Centering inside a row means
        // moving down by half of the unused vertical space.
        return row.y + (row.height - static_cast<float>(fontSize)) / 2.0f;
    }

    Rectangle insetRectangle(Rectangle bounds, float padding)
    {
        // Create a smaller rectangle inside bounds. Drawing helpers use this so
        // text does not sit directly on panel borders.
        return Rectangle{
            bounds.x + padding,
            bounds.y + padding,
            bounds.width - 2.0f * padding,
            bounds.height - 2.0f * padding
        };
    }

    float drawTextAndAdvance(const char* text, float x, float y, int fontSize, Color color, float spacing)
    {
        // Layout math is easier as float, but raylib's DrawText wants integer
        // pixel coordinates. MeasureText returns the width of the drawn text,
        // so the caller can continue drawing immediately after it.
        DrawText(text, static_cast<int>(x), static_cast<int>(y), fontSize, color);
        return x + static_cast<float>(MeasureText(text, fontSize)) + spacing;
    }

    float drawKeyHint(
        const char* label,
        const char* key,
        float x,
        float y,
        int fontSize,
        float labelSpacing,
        float groupSpacing)
    {
        // Keyboard hints repeat the same visual rule everywhere: label in dark
        // text, key in blue, then return the next x position.
        x = drawTextAndAdvance(label, x, y, fontSize, DARKGRAY, labelSpacing);
        x = drawTextAndAdvance(key, x, y, fontSize, BLUE, groupSpacing);
        return x;
    }

    void drawLabelValue(
        const char* label,
        const char* value,
        float x,
        float y,
        int fontSize,
        Color valueColor)
    {
        // Status rows use label/value pairs rather than keyboard hints. This
        // helper keeps footer drawing focused on what is displayed.
        const float labelSpacing = static_cast<float>(fontSize) * 0.55f;

        x = drawTextAndAdvance(label, x, y, fontSize, DARKGRAY, labelSpacing);
        drawTextAndAdvance(value, x, y, fontSize, valueColor, 0.0f);
    }

    void drawControls(Rectangle controlsBounds)
    {
        const float padding = controlsBounds.height * 0.10f;
        Rectangle innerBounds = insetRectangle(controlsBounds, padding);

        float rowHeight = innerBounds.height / 3.0f;

        Rectangle row1 = {innerBounds.x, innerBounds.y, innerBounds.width, rowHeight};
        Rectangle row2 = {innerBounds.x, innerBounds.y + rowHeight, innerBounds.width, rowHeight};
        Rectangle row3 = {innerBounds.x, innerBounds.y + 2 * rowHeight, innerBounds.width, rowHeight};

        const int fontSize = std::clamp(static_cast<int>(rowHeight * 0.65f), 10, 48);

        const float row1TextY = textYForRow(row1, fontSize);
        const float row2TextY = textYForRow(row2, fontSize);
        const float row3TextY = textYForRow(row3, fontSize);

        float currentX = row1.x;

        float smallSpacing = 20.0f;
        float largeSpacing = 50.0f;
        float hugeSpacing = 150.0f;

        currentX = drawTextAndAdvance("Playback Controls:", currentX, row1TextY, fontSize, MAROON, largeSpacing);

        currentX = drawKeyHint("Pause/Play:", "SPACE", currentX, row1TextY, fontSize, smallSpacing, largeSpacing);
        currentX = drawKeyHint("Back:", "LEFT", currentX, row1TextY, fontSize, smallSpacing, largeSpacing);
        currentX = drawKeyHint("Forward:", "RIGHT", currentX, row1TextY, fontSize, smallSpacing, largeSpacing);
        currentX = drawKeyHint("Speed +:", "UP", currentX, row1TextY, fontSize, smallSpacing, largeSpacing);
        currentX = drawKeyHint("Speed -:", "DOWN", currentX, row1TextY, fontSize, smallSpacing, largeSpacing);
        currentX = drawKeyHint("Reset:", "R", currentX, row1TextY, fontSize, smallSpacing, largeSpacing);

        currentX = row2.x;

        currentX = drawTextAndAdvance("Run Setup:", currentX, row2TextY, fontSize, MAROON, largeSpacing);

        currentX = drawKeyHint("Item -:", "A", currentX, row2TextY, fontSize, smallSpacing, largeSpacing);
        currentX = drawKeyHint("Item +:", "D", currentX, row2TextY, fontSize, smallSpacing, largeSpacing);
        currentX = drawKeyHint("Apply:", "ENTER", currentX, row2TextY, fontSize, smallSpacing, hugeSpacing);

        currentX = drawTextAndAdvance("Algorithm Selection:", currentX, row2TextY, fontSize, MAROON, largeSpacing);

        currentX = drawKeyHint("Bubble:", "1", currentX, row2TextY, fontSize, smallSpacing, largeSpacing);
        currentX = drawKeyHint("Insertion:", "2", currentX, row2TextY, fontSize, smallSpacing, largeSpacing);
        currentX = drawKeyHint("Selection:", "3", currentX, row2TextY, fontSize, smallSpacing, largeSpacing);

        currentX = row3.x;

        currentX = drawTextAndAdvance("Values:", currentX, row3TextY, fontSize, MAROON, largeSpacing);

        currentX = drawKeyHint("Perm:", "4", currentX, row3TextY, fontSize, smallSpacing, largeSpacing);
        currentX = drawKeyHint("Few:", "5", currentX, row3TextY, fontSize, smallSpacing, largeSpacing);
        currentX = drawKeyHint("Equal:", "6", currentX, row3TextY, fontSize, smallSpacing, hugeSpacing);

        currentX = drawTextAndAdvance("Order:", currentX, row3TextY, fontSize, MAROON, largeSpacing);

        currentX = drawKeyHint("Rand:", "7", currentX, row3TextY, fontSize, smallSpacing, largeSpacing);
        currentX = drawKeyHint("Asc:", "8", currentX, row3TextY, fontSize, smallSpacing, largeSpacing);
        currentX = drawKeyHint("Desc:", "9", currentX, row3TextY, fontSize, smallSpacing, largeSpacing);
        currentX = drawKeyHint("Near:", "0", currentX, row3TextY, fontSize, smallSpacing, largeSpacing);
    }

    void drawPlaybackStatus(
        const VisualizerSession& session,
        Rectangle playbackStatusBounds)
    {
        const float padding = playbackStatusBounds.height * 0.12f;
        const Rectangle innerBounds = insetRectangle(playbackStatusBounds, padding);

        const int fontSize = std::clamp(
            static_cast<int>(innerBounds.height * 0.60f),
            10,
            32);

        const float textY = textYForRow(innerBounds, fontSize);
        const float smallSpacing = static_cast<float>(fontSize) * 0.70f;
        const float largeSpacing = static_cast<float>(fontSize) * 2.0f;

        const char* playbackText = "Paused";
        Color playbackColor = RED;

        if (session.isComplete()) {
            playbackText = "Complete";
            playbackColor = BLUE;
        }
        else if (session.isPlaying()) {
            playbackText = "Playing";
            playbackColor = GREEN;
        }

        float currentX = innerBounds.x;

        currentX = drawTextAndAdvance("Playback:", currentX, textY, fontSize, MAROON, largeSpacing);
        currentX = drawTextAndAdvance(playbackText, currentX, textY, fontSize, playbackColor, largeSpacing);
        currentX = drawTextAndAdvance("Seconds/Event:", currentX, textY, fontSize, DARKGRAY, smallSpacing);
        currentX = drawTextAndAdvance(TextFormat("%.4f", session.secondsPerEvent()), currentX, textY, fontSize, GREEN, largeSpacing);
    }

    void drawSettingsStatus(
        const VisualizerSession& session,
        Rectangle footerBounds)
    {
        const SortRunSpec& loadedSettings = session.loadedSettings();
        const SortRunSpec& draftSettings = session.draftSettings();
        const bool settingsDirty = session.settingsDirty();

        const float padding = footerBounds.height * 0.12f;
        const Rectangle innerBounds = insetRectangle(footerBounds, padding);

        // The footer is a status panel, not a control surface. Keep its local
        // layout simple: loaded settings on the left, draft settings in the
        // middle, and the apply/dirty message on the right.
        const float columnGap = innerBounds.width * 0.035f;
        const float messageColumnWidth = innerBounds.width * 0.28f;
        const float settingsColumnWidth =
            (innerBounds.width - messageColumnWidth - 2.0f * columnGap) / 2.0f;

        const Rectangle loadedColumn = {
            innerBounds.x,
            innerBounds.y,
            settingsColumnWidth,
            innerBounds.height
        };

        const Rectangle draftColumn = {
            loadedColumn.x + loadedColumn.width + columnGap,
            innerBounds.y,
            settingsColumnWidth,
            innerBounds.height
        };

        const Rectangle messageColumn = {
            draftColumn.x + draftColumn.width + columnGap,
            innerBounds.y,
            messageColumnWidth,
            innerBounds.height
        };

        const float rowHeight = innerBounds.height / 4.0f;
        const int fontSize = std::clamp(
            static_cast<int>(rowHeight * 0.68f),
            10,
            28);

        const Color draftColor = settingsDirty ? RED : GREEN;

        for (int rowIndex = 0; rowIndex < 4; ++rowIndex) {
            const Rectangle row = {
                innerBounds.x,
                innerBounds.y + static_cast<float>(rowIndex) * rowHeight,
                innerBounds.width,
                rowHeight
            };

            const float rowTextY = textYForRow(row, fontSize);

            if (rowIndex == 0) {
                drawLabelValue(
                    "Loaded algorithm:",
                    algorithmName(loadedSettings.algorithm),
                    loadedColumn.x,
                    rowTextY,
                    fontSize,
                    BLUE);

                drawLabelValue(
                    "Draft algorithm:",
                    algorithmName(draftSettings.algorithm),
                    draftColumn.x,
                    rowTextY,
                    fontSize,
                    draftColor);
            }
            else if (rowIndex == 1) {
                drawLabelValue(
                    "Loaded values:",
                    valueSpecName(loadedSettings.inputSpec.valueSpec),
                    loadedColumn.x,
                    rowTextY,
                    fontSize,
                    BLUE);

                drawLabelValue(
                    "Draft values:",
                    valueSpecName(draftSettings.inputSpec.valueSpec),
                    draftColumn.x,
                    rowTextY,
                    fontSize,
                    draftColor);
            }
            else if (rowIndex == 2) {
                drawLabelValue(
                    "Loaded order:",
                    orderSpecName(loadedSettings.inputSpec.initialOrderSpec),
                    loadedColumn.x,
                    rowTextY,
                    fontSize,
                    BLUE);

                drawLabelValue(
                    "Draft order:",
                    orderSpecName(draftSettings.inputSpec.initialOrderSpec),
                    draftColumn.x,
                    rowTextY,
                    fontSize,
                    draftColor);
            }
            else {
                drawLabelValue(
                    "Loaded item count:",
                    TextFormat("%u", loadedSettings.inputSpec.itemCount),
                    loadedColumn.x,
                    rowTextY,
                    fontSize,
                    BLUE);

                drawLabelValue(
                    "Draft item count:",
                    TextFormat("%u", draftSettings.inputSpec.itemCount),
                    draftColumn.x,
                    rowTextY,
                    fontSize,
                    draftColor);
            }
        }

        const int messageFontSize = std::clamp(
            static_cast<int>(messageColumn.height * 0.18f),
            10,
            28);
        const float messageY = textYForRow(messageColumn, messageFontSize);

        if (settingsDirty) {
            drawTextAndAdvance(
                "Settings changed: press ENTER to apply",
                messageColumn.x,
                messageY,
                messageFontSize,
                RED,
                0.0f);
        }
        else {
            drawTextAndAdvance(
                "Settings applied",
                messageColumn.x,
                messageY,
                messageFontSize,
                GREEN,
                0.0f);
        }
    }

    // Draw the current app frame.
    //
    // This is still app-level drawing, not renderer internals: it draws title,
    // controls, and status text, then asks RaylibRenderer to draw the SortState.
    //
    // Design note: this helper is allowed to know app status text, but it
    // should not grow into custom UI widgets. If the controls become interactive
    // mouse elements, move that behavior behind smaller helpers instead of adding
    // more logic here.
    void drawApp(
        const VisualizerSession& session,
        const RaylibRenderer& renderer)
    {
        BeginDrawing();

        ClearBackground(RAYWHITE);

        AppLayout layout = calculateAppLayout(GetScreenWidth(), GetScreenHeight());

        if (showLayoutDebug) {
            DrawRectangleRec(layout.headerBounds, Fade(LIGHTGRAY, 0.35f));
            DrawRectangleRec(layout.titleBounds, Fade(RED, 0.35f));
            DrawRectangleRec(layout.controlsBounds, Fade(PURPLE, 0.35f));
            DrawRectangleRec(layout.playbackStatusBounds, Fade(ORANGE, 0.30f));
            DrawRectangleRec(layout.chartBounds, Fade(SKYBLUE, 0.25f));
            DrawRectangleRec(layout.footerBounds, Fade(BEIGE, 0.35f));

            DrawRectangleLinesEx(layout.headerBounds, 2.0f, GRAY);
            DrawRectangleLinesEx(layout.titleBounds, 2.0f, RED);
            DrawRectangleLinesEx(layout.controlsBounds, 2.0f, PURPLE);
            DrawRectangleLinesEx(layout.playbackStatusBounds, 2.0f, ORANGE);
            DrawRectangleLinesEx(layout.chartBounds, 2.0f, BLUE);
            DrawRectangleLinesEx(layout.footerBounds, 2.0f, BROWN);
        }

        drawTitle(layout.titleBounds);

        drawControls(layout.controlsBounds);
        drawPlaybackStatus(session, layout.playbackStatusBounds);
        drawSettingsStatus(session, layout.footerBounds);

        renderer.drawSortState(session.currentSortState(), layout.chartBounds);

        EndDrawing();
    }
}

void App::run()
{
    // A resizable window exercises the same layout path across display sizes.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Sorting Visualizer");
    SetWindowMinSize(800, 450);

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

    VisualizerSession session(initialSettings);
    RaylibRenderer renderer;

    while (!WindowShouldClose()) {
        handleSettingsInput(session);
        handlePlaybackInput(session);
        session.update(GetFrameTime());
        drawApp(session, renderer);
    }

    CloseWindow();
}
