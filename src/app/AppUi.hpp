#pragma once

class VisualizerSession;
class RaylibRenderer;

// Raylib-facing user-interface boundary.
//
// AppUi translates user input into VisualizerSession commands and draws the
// app-level interface. Window and frame lifetime remain owned by App.
class AppUi {
public:
    // Poll keyboard and future mouse controls once per frame.
    void handleInput(VisualizerSession& session);

    // Draw app-level UI and delegate chart drawing within an active frame.
    void draw(
        const VisualizerSession& session,
        const RaylibRenderer& renderer) const;
};
