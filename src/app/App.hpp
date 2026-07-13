#pragma once

// Top-level composition root for the desktop application.
// App owns process-level raylib lifetime and delegates UI and session behavior.
class App {
public:
    // Initialize the window, run frames until close is requested, and shut down.
    void run();
};
