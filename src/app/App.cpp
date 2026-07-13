#include "App.hpp"

#include <raylib.h>

#include "AppUi.hpp"
#include "VisualizerSession.hpp"
#include "domain/Algorithm.hpp"
#include "domain/SortSpec.hpp"
#include "rendering/RaylibRenderer.hpp"

void App::run()
{
    // App owns window and frame lifetime. AppUi owns the contents drawn inside
    // each frame, while VisualizerSession owns non-graphical behavior.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Sorting Visualizer");
    SetWindowMinSize(800, 450);
    SetTargetFPS(60);

    // Startup defaults are app composition policy, not domain constraints.
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
    AppUi ui;

    while (!WindowShouldClose()) {
        ui.handleInput(session);
        session.update(GetFrameTime());

        BeginDrawing();
        ClearBackground(RAYWHITE);
        ui.draw(session, renderer);
        EndDrawing();
    }

    CloseWindow();
}
