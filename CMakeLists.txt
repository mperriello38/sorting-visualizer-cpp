cmake_minimum_required(VERSION 3.20)

project(sorting-visualizer-cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
)

FetchContent_MakeAvailable(raylib)

add_executable(sorting_visualizer
    src/main.cpp

    src/app/App.cpp
    src/app/VisualizerSession.cpp
    src/app/AppUi.cpp

    src/input/InputGenerator.cpp

    src/sorting/SortRunner.cpp
    src/sorting/SelectionSort.cpp
    src/sorting/BubbleSort.cpp
    src/sorting/InsertionSort.cpp

    src/animation/AnimationPlayer.cpp

    src/rendering/RaylibRenderer.cpp
)

add_executable(sorting_tests
    src/testing/TestMain.cpp
    src/testing/SortTests.cpp
    src/testing/TestSupport.cpp
    src/testing/InputGeneratorTests.cpp
    src/testing/SortingTests.cpp
    src/testing/AnimationPlayerTests.cpp
    src/testing/VisualizerSessionTests.cpp

    src/app/VisualizerSession.cpp

    src/input/InputGenerator.cpp

    src/sorting/SortRunner.cpp
    src/sorting/SelectionSort.cpp
    src/sorting/BubbleSort.cpp
    src/sorting/InsertionSort.cpp

    src/animation/AnimationPlayer.cpp
)

target_include_directories(sorting_visualizer PRIVATE src)
target_include_directories(sorting_tests PRIVATE src)

target_link_libraries(sorting_visualizer PRIVATE raylib)

if (MSVC)
    target_compile_options(sorting_visualizer PRIVATE /W4)
    target_compile_options(sorting_tests PRIVATE /W4)
else()
    target_compile_options(sorting_visualizer PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(sorting_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
