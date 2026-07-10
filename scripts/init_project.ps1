$ErrorActionPreference = "Stop"

Write-Host "Creating project folders..."

$directories = @(
    "src",
    "src/app",
    "src/domain",
    "src/input",
    "src/sorting",
    "src/animation",
    "src/rendering",
    "src/testing",
    "assets",
    "scripts"
)

foreach ($dir in $directories) {
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
}

function New-FileIfMissing {
    param (
        [string]$Path,
        [string]$Content = ""
    )

    if (-not (Test-Path $Path)) {
        New-Item -ItemType File -Path $Path -Force | Out-Null

        if ($Content -ne "") {
            Set-Content -Path $Path -Value $Content
        }

        Write-Host "Created $Path"
    }
    else {
        Write-Host "Already exists: $Path"
    }
}

Write-Host "Creating source files..."

New-FileIfMissing "src/main.cpp" @'
#include "app/App.hpp"

int main()
{
    App app;
    app.run();

    return 0;
}
'@

New-FileIfMissing "src/app/App.hpp" @'
#pragma once

class App {
public:
    void run();
};
'@

New-FileIfMissing "src/app/App.cpp" @'
#include "App.hpp"

void App::run()
{
    // Main application loop will go here.
}
'@

New-FileIfMissing "src/domain/SortItem.hpp" @'
#pragma once

struct SortItem {
    int value;
    int id;
};
'@

New-FileIfMissing "src/domain/SortEvent.hpp"
New-FileIfMissing "src/domain/SortSpec.hpp"
New-FileIfMissing "src/domain/SortState.hpp"

New-FileIfMissing "src/input/InputGenerator.hpp"
New-FileIfMissing "src/input/InputGenerator.cpp"

New-FileIfMissing "src/sorting/Algorithm.hpp"
New-FileIfMissing "src/sorting/SortRunner.hpp"
New-FileIfMissing "src/sorting/SortRunner.cpp"
New-FileIfMissing "src/sorting/SelectionSort.cpp"
New-FileIfMissing "src/sorting/BubbleSort.cpp"
New-FileIfMissing "src/sorting/InsertionSort.cpp"

New-FileIfMissing "src/animation/AnimationPlayer.hpp"
New-FileIfMissing "src/animation/AnimationPlayer.cpp"

New-FileIfMissing "src/rendering/RaylibRenderer.hpp"
New-FileIfMissing "src/rendering/RaylibRenderer.cpp"

New-FileIfMissing "src/testing/SortTests.hpp"
New-FileIfMissing "src/testing/SortTests.cpp"

New-FileIfMissing "CMakeLists.txt" @'
cmake_minimum_required(VERSION 3.16)

project(sort_visualizer_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(sort_visualizer
    src/main.cpp
    src/app/App.cpp
    src/input/InputGenerator.cpp
    src/sorting/SortRunner.cpp
    src/sorting/SelectionSort.cpp
    src/sorting/BubbleSort.cpp
    src/sorting/InsertionSort.cpp
    src/animation/AnimationPlayer.cpp
    src/rendering/RaylibRenderer.cpp
    src/testing/SortTests.cpp
)

target_include_directories(sort_visualizer PRIVATE src)

find_package(raylib CONFIG REQUIRED)
target_link_libraries(sort_visualizer PRIVATE raylib)
'@

New-FileIfMissing "scripts/build.ps1" @'
$ErrorActionPreference = "Stop"

param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug"
)

cmake -S . -B build
cmake --build build --config $Config
'@

Write-Host ""
Write-Host "Project scaffold complete."
Write-Host "Next:"
Write-Host "  .\scripts\build.ps1"