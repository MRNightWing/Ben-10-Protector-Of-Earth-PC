cmake_minimum_required(VERSION 3.20)
project(ben10_pc VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SDL2_DIR "C:/tools/SDL2/x86_64-w64-mingw32" CACHE PATH "Path to SDL2")

include_directories(
    include
    ${SDL2_DIR}/include
)

link_directories(
    ${SDL2_DIR}/lib
)

set(SOURCES
    src/main.cpp
    src/core/engine.cpp
    src/platform/psp_ctrl.cpp
    src/platform/psp_display.cpp
    src/platform/psp_kernel.cpp
    src/video/renderer.cpp
    src/quantum/r4m_parser.cpp
    src/quantum/streamer.cpp
)

add_executable(ben10_pc ${SOURCES})

target_link_libraries(ben10_pc PRIVATE
    mingw32
    SDL2main
    SDL2
)

add_custom_command(TARGET ben10_pc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${SDL2_DIR}/bin/SDL2.dll"
    $<TARGET_FILE_DIR:ben10_pc>
)
