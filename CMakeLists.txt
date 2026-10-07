cmake_minimum_required(VERSION 3.16)
project(lunar_trail
    VERSION 0.1.0
    DESCRIPTION "The Oregon Trail, reflight. A lunar south-pole survival logistics game."
    HOMEPAGE_URL "https://github.com/ep150de/nbnw-lunar-trail-game"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

option(LUNAR_TRAIL_BUILD_TESTS "Build the headless test suite" ON)

# ---------------------------------------------------------------- SDL2
# Preferred: system packages.
#   Debian/Ubuntu : sudo apt install libsdl2-dev libsdl2-image-dev \
#                               libsdl2-ttf-dev libsdl2-mixer-dev
#   Fedora        : sudo dnf install SDL2-devel SDL2_image-devel \
#                               SDL2_ttf-devel SDL2_mixer-devel
#   Arch          : sudo pacman -S sdl2 sdl2_image sdl2_ttf sdl2_mixer
#   macOS         : brew install sdl2 sdl2_image sdl2_ttf sdl2_mixer
#   Windows       : vcpkg install sdl2 sdl2_image sdl2_ttf sdl2_mixer
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(SDL2_mixer REQUIRED)

message(STATUS "SDL2        ${SDL2_VERSION}")
message(STATUS "SDL2_image  ${SDL2_IMAGE_VERSION}")
message(STATUS "SDL2_ttf    ${SDL2_TTF_VERSION}")
message(STATUS "SDL2_mixer  ${SDL2_MIXER_VERSION}")

# ---------------------------------------------------------------- core sources
# NOTE: deliberately zero network build dependencies beyond SDL2. The content
# files under content/ are read by the small vendored JSON reader in
# src/core/json.h rather than by a fetched third-party library, so that a
# fresh clone configures offline.
set(LUNAR_TRAIL_SOURCES
    src/main.cpp

    src/core/rng.cpp
    src/core/renderer.cpp
    src/core/textwrap.cpp
    src/core/input.cpp
    src/core/persist.cpp
    src/core/audio.cpp

    src/sim/convoy.cpp
    src/sim/balance.cpp
    src/sim/health.cpp
    src/sim/hazards.cpp
    src/sim/trail.cpp
    src/sim/depot.cpp
    src/sim/daystep.cpp
    src/core/json.cpp
    src/game/ui.cpp

    src/game/game.cpp
    src/game/descent.cpp
    src/game/scenes_ch0.cpp
    src/game/scenes_ch1.cpp
    src/game/scenes_ch1_descent.cpp
    src/game/scenes_ch2.cpp
    src/game/scenes_manual.cpp
    src/game/manual.cpp
    src/game/scenes_end.cpp
)

add_executable(lunar_trail ${LUNAR_TRAIL_SOURCES})

target_include_directories(lunar_trail PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/gen
)

target_link_libraries(lunar_trail PRIVATE
    SDL2::SDL2
    SDL2_image::SDL2_image
    SDL2_ttf::SDL2_ttf
    SDL2_mixer::SDL2_mixer)

if(MSVC)
    target_compile_options(lunar_trail PRIVATE /W4 /permissive-)
else()
    target_compile_options(lunar_trail PRIVATE
        -Wall -Wextra -Wpedantic
        -Wshadow -Wconversion -Wsign-conversion
        -Wno-unused-parameter)
endif()

# Assets live next to the binary so the game runs from build/ directly.
add_custom_command(TARGET lunar_trail POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets
            ${CMAKE_BINARY_DIR}/assets
    COMMENT "Staging assets/ into the build directory")

# ---------------------------------------------------------------- tests
if(LUNAR_TRAIL_BUILD_TESTS)
    enable_testing()
    add_executable(lunar_trail_tests
        tests/test_main.cpp
        src/core/rng.cpp
        src/core/json.cpp
        src/core/textwrap.cpp
        src/core/persist.cpp
        src/sim/convoy.cpp
        src/sim/balance.cpp
        src/sim/health.cpp
        src/sim/hazards.cpp
        src/sim/trail.cpp
        src/sim/depot.cpp
        src/sim/daystep.cpp
        src/game/descent.cpp
        src/game/manual.cpp
    )
    target_include_directories(lunar_trail_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/gen
    )
    target_compile_options(lunar_trail_tests PRIVATE
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>)
    add_test(NAME lunar_trail_tests COMMAND lunar_trail_tests)
endif()

# ---------------------------------------------------------------- balance harness
# A headless player that measures whether the economy is coherent. Not part of
# the game; see the comment at the top of the file.
add_executable(balance_harness EXCLUDE_FROM_ALL
    tools/balance_harness.cpp
    src/sim/convoy.cpp
    src/sim/balance.cpp
    src/sim/health.cpp
    src/sim/hazards.cpp
    src/sim/trail.cpp
    src/sim/depot.cpp
    src/sim/daystep.cpp
    src/core/rng.cpp
    src/core/json.cpp
)
target_include_directories(balance_harness PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(balance_harness PRIVATE
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>)

add_executable(lunar_trail_ui_capture EXCLUDE_FROM_ALL
    tools/ui_capture.cpp
    src/core/renderer.cpp
    src/core/input.cpp
    src/core/rng.cpp
    src/core/json.cpp
    src/core/textwrap.cpp
    src/game/ui.cpp
    src/sim/convoy.cpp
    src/sim/balance.cpp
    src/sim/health.cpp
    src/sim/hazards.cpp
    src/sim/trail.cpp
    src/sim/depot.cpp
    src/sim/daystep.cpp
)
target_include_directories(lunar_trail_ui_capture PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/gen)
target_link_libraries(lunar_trail_ui_capture PRIVATE
    SDL2::SDL2 SDL2_image::SDL2_image SDL2_ttf::SDL2_ttf)

install(TARGETS lunar_trail RUNTIME DESTINATION bin)
install(DIRECTORY content/ DESTINATION share/lunar_trail/content)
install(DIRECTORY assets/ DESTINATION share/lunar_trail/assets)
