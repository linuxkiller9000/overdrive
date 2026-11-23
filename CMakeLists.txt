cmake_minimum_required(VERSION 3.16)
project(overdrive CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT RAYLIB_DIR)
    set(RAYLIB_DIR $ENV{HOME}/raylib-5.5/build/raylib)
endif()
if(NOT SDK_DIR)
    set(SDK_DIR $ENV{HOME}/sdk/usr)
endif()

add_compile_options(-O2)
include_directories(${RAYLIB_DIR}/include ${SDK_DIR}/include)
link_directories(${RAYLIB_DIR} ${SDK_DIR}/lib/x86_64-linux-gnu)

add_executable(overdrive src/main.cpp src/game.cpp src/sim.cpp tests/sim_tests.cpp)
if(WIN32)
    target_link_libraries(overdrive raylib opengl32 winmm gdi32 -static -static-libstdc++ -static-libgcc)
else()
    target_link_libraries(overdrive raylib GL m pthread dl rt X11 /usr/lib/x86_64-linux-gnu/libXtst.so.6 -static-libstdc++ -static-libgcc)
endif()

add_executable(overdrive_tests tests/main_tests.cpp tests/sim_tests.cpp src/sim.cpp)
target_link_libraries(overdrive_tests m)

enable_testing()
add_test(NAME sim COMMAND overdrive_tests)
