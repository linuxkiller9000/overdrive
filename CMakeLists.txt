cmake_minimum_required(VERSION 3.16)
project(overdrive CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Pass -DRAYLIB_DIR=/path/to/raylib-5.5/build/raylib (the folder holding libraylib.a and include/).
# SDK_DIR is optional: a prefix with extra headers/libs (for example a local X11 dev sysroot).
if(NOT RAYLIB_DIR)
    set(RAYLIB_DIR $ENV{HOME}/raylib-5.5/build/raylib)
endif()
if(NOT EXISTS "${RAYLIB_DIR}")
    message(FATAL_ERROR "RAYLIB_DIR not found: ${RAYLIB_DIR}. Build raylib 5.5 and pass -DRAYLIB_DIR=<its build/raylib folder>.")
endif()

add_compile_options(-O2)
include_directories(${RAYLIB_DIR}/include)
link_directories(${RAYLIB_DIR})
if(SDK_DIR)
    include_directories(${SDK_DIR}/include)
    link_directories(${SDK_DIR}/lib/x86_64-linux-gnu)
endif()

add_executable(overdrive src/main.cpp src/game.cpp src/sim.cpp src/vehicles.cpp src/tracks.cpp tests/sim_tests.cpp)
if(WIN32)
    target_link_libraries(overdrive raylib opengl32 winmm gdi32 -static -static-libstdc++ -static-libgcc)
else()
    find_library(XTST_LIB NAMES Xtst libXtst.so.6 HINTS /usr/lib/x86_64-linux-gnu)
    if(NOT XTST_LIB)
        set(XTST_LIB Xtst)
    endif()
    target_link_libraries(overdrive raylib GL m pthread dl rt X11 ${XTST_LIB} -static-libstdc++ -static-libgcc)
endif()

add_executable(overdrive_tests tests/main_tests.cpp tests/sim_tests.cpp src/sim.cpp src/vehicles.cpp src/tracks.cpp)
target_link_libraries(overdrive_tests m)

enable_testing()
add_test(NAME sim COMMAND overdrive_tests)
