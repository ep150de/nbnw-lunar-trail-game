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
# The SDL2 addons are looked for two ways.
#
# Distribution packages frequently ship the libraries and their pkg-config files
# but no CMake package config: Ubuntu 22.04's libsdl2-image-dev, for one, has
# `sdl2_image.pc` and no `sdl2_image-config.cmake`. Requiring the config file
# therefore fails on the most common Linux install of all, even though the
# development headers are present. So: try CONFIG first, and fall back to
# pkg-config, which is what those packages do provide.
find_package(SDL2 REQUIRED)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    # The .pc filenames differ between distributions: Debian and Ubuntu ship
    # `SDL2_image.pc`, Homebrew ships `sdl2_image.pc`. Probe both spellings so one
    # CMake configure works on either, rather than only on the machine the
    # maintainer happens to be using.
    foreach(_addon SDL2_image SDL2_ttf SDL2_mixer)
        string(TOLOWER "${_addon}" _lower)
        pkg_check_modules(${_addon} QUIET IMPORTED_TARGET ${_addon})
        if(NOT ${_addon}_FOUND)
            pkg_check_modules(${_addon} QUIET IMPORTED_TARGET ${_lower})
        endif()
    endforeach()
endif()

# The _FOUND variables are spelled exactly as the pkg module names: CMake keeps
# them case sensitive, and `SDL2_IMAGE_FOUND` is a different variable from the
# `SDL2_image_FOUND` that pkg_check_modules actually sets.
if(NOT SDL2_image_FOUND)
    find_package(SDL2_image REQUIRED)
endif()
if(NOT SDL2_ttf_FOUND)
    find_package(SDL2_ttf REQUIRED)
endif()
if(NOT SDL2_mixer_FOUND)
    find_package(SDL2_mixer REQUIRED)
endif()

if(TARGET PkgConfig::SDL2_image)
    add_library(SDL2_image::SDL2_image ALIAS PkgConfig::SDL2_image)
endif()
if(TARGET PkgConfig::SDL2_ttf)
    add_library(SDL2_ttf::SDL2_ttf ALIAS PkgConfig::SDL2_ttf)
endif()
if(TARGET PkgConfig::SDL2_mixer)
    add_library(SDL2_mixer::SDL2_mixer ALIAS PkgConfig::SDL2_mixer)
endif()

# The addon versions are empty when a package came from pkg-config, since
# pkg_check_modules does not fill the find_package version variables. That is
# expected, not a failure; report the path taken so it is diagnosable.
message(STATUS "SDL2        ${SDL2_VERSION}")
if(TARGET PkgConfig::SDL2_IMAGE)
    message(STATUS "SDL2_image  via pkg-config ${SDL2_image_LIBRARIES}")
else()
    message(STATUS "SDL2_image  ${SDL2_image_VERSION} via CMake config ${SDL2_image_LIBRARIES}")
endif()
message(STATUS "SDL2_ttf    ${SDL2_ttf_VERSION} ${SDL2_ttf_LIBRARY}")
message(STATUS "SDL2_mixer  ${SDL2_mixer_VERSION} ${SDL2_mixer_LIBRARY}")

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

# SDL2main provides the WinMain entry point that MSVC's linker needs to find
# `main`. It is a no-op on the other platforms, so it is linked unconditionally
# rather than behind a platform branch that would rot.
target_link_libraries(lunar_trail PRIVATE
    SDL2::SDL2
    SDL2::SDL2main
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
# SDL2main supplies the WinMain wrapper on Windows, for the same reason the game
# links it: MSVC will not find a plain `main` without it.
target_link_libraries(lunar_trail_ui_capture PRIVATE
    SDL2::SDL2 SDL2::SDL2main SDL2_image::SDL2_image SDL2_ttf::SDL2_ttf)

install(TARGETS lunar_trail RUNTIME DESTINATION bin)
install(DIRECTORY content/ DESTINATION share/lunar_trail/content)
install(DIRECTORY assets/ DESTINATION share/lunar_trail/assets)
