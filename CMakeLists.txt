cmake_minimum_required(VERSION 3.16)
project(corenest C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${SDL2_INCLUDE_DIRS}
)

set(SOURCES
    src/main.c
    src/launcher.c
    src/core_loader.c
    src/environment.c
    src/video.c
    src/audio.c
    src/input.c
)

add_executable(corenest ${SOURCES})

target_link_libraries(corenest
    ${SDL2_LIBRARIES}
    mingw32
    SDL2main
    SDL2
    dinput8
    dxguid
    user32
    gdi32
    winmm
    imm32
    ole32
    oleaut32
    shell32
    setupapi
    version
    uuid
    cfgmgr32
    ${CMAKE_DL_LIBS}
)

if(WIN32)
    target_link_options(corenest PRIVATE -static -static-libgcc)
endif()

add_custom_command(TARGET corenest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/cores
        $<TARGET_FILE_DIR:corenest>/cores
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/roms
        $<TARGET_FILE_DIR:corenest>/roms
)
