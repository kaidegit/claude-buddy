cmake_minimum_required(VERSION 3.16)

project(buddy_lvgl_pc_sim C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(BUDDY_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(LV_PORT_DIR "${BUDDY_ROOT}/third_party/lv_port_pc_vscode")

if(NOT EXISTS "${LV_PORT_DIR}/lvgl/lvgl.h")
    message(FATAL_ERROR "Missing nested LVGL submodule. Run: git submodule update --init --recursive third_party/lv_port_pc_vscode")
endif()

find_program(SDL2_CONFIG sdl2-config)
if(SDL2_CONFIG)
    execute_process(COMMAND ${SDL2_CONFIG} --cflags
        OUTPUT_VARIABLE BUDDY_SIM_SDL_CFLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${SDL2_CONFIG} --libs
        OUTPUT_VARIABLE BUDDY_SIM_SDL_LDFLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    separate_arguments(BUDDY_SIM_SDL_CFLAGS_LIST NATIVE_COMMAND "${BUDDY_SIM_SDL_CFLAGS}")
    separate_arguments(BUDDY_SIM_SDL_LDFLAGS_LIST NATIVE_COMMAND "${BUDDY_SIM_SDL_LDFLAGS}")
    set(BUDDY_SIM_SDL_LIBS ${BUDDY_SIM_SDL_LDFLAGS_LIST})
else()
    find_package(SDL2 QUIET)
    if(TARGET SDL2::SDL2)
        set(BUDDY_SIM_SDL_LIBS SDL2::SDL2)
    elseif(SDL2_FOUND)
        set(BUDDY_SIM_SDL_LIBS ${SDL2_LIBRARIES})
        set(BUDDY_SIM_SDL_INCLUDES ${SDL2_INCLUDE_DIRS})
    else()
        message(FATAL_ERROR "SDL2 was not found. Install it first, for example: brew install sdl2, or apt install libsdl2-dev.")
    endif()
endif()

set(LV_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/include/lv_conf.h" CACHE FILEPATH "LVGL simulator config" FORCE)
set(LV_CONF_INCLUDE_SIMPLE ON CACHE BOOL "Use simple lv_conf include" FORCE)
set(LV_LVGL_H_INCLUDE_SIMPLE ON CACHE BOOL "Use simple lvgl include" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static simulator dependencies" FORCE)

add_subdirectory("${LV_PORT_DIR}/lvgl" "${CMAKE_BINARY_DIR}/lvgl")
if(TARGET lvgl_examples)
    set_target_properties(lvgl_examples PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()
if(TARGET lvgl_demos)
    set_target_properties(lvgl_demos PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()

add_library(lv_drivers_sdl STATIC
    "${LV_PORT_DIR}/lv_drivers/sdl/sdl.c"
    "${LV_PORT_DIR}/lv_drivers/sdl/sdl_common.c"
)
target_compile_definitions(lv_drivers_sdl PUBLIC
    LV_CONF_INCLUDE_SIMPLE
    LV_LVGL_H_INCLUDE_SIMPLE
    USE_SDL=1
    USE_X11=0
)
target_include_directories(lv_drivers_sdl SYSTEM PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/include"
    "${LV_PORT_DIR}/lv_drivers"
    ${BUDDY_SIM_SDL_INCLUDES}
)
target_compile_options(lv_drivers_sdl PUBLIC ${BUDDY_SIM_SDL_CFLAGS_LIST})
target_link_libraries(lv_drivers_sdl PUBLIC lvgl ${BUDDY_SIM_SDL_LIBS})

file(GLOB BUDDY_ASCII_SOURCES CONFIGURE_DEPENDS
    "${BUDDY_ROOT}/app/src/ascii/*.cpp"
    "${BUDDY_ROOT}/app/src/ascii/buddies/*.cpp"
)

add_executable(buddy_lvgl_pc_sim
    "src/main.c"
    "src/rtthread_stub.c"
    "src/littlevgl2rtt_stub.c"
    "src/buddy_ui_data_mock.c"
    "${BUDDY_ROOT}/app/src/ui/buddy_ui_390x450.c"
    ${BUDDY_ASCII_SOURCES}
)

target_compile_definitions(buddy_lvgl_pc_sim PRIVATE
    BUDDY_UI_SIMULATOR
    LV_CONF_INCLUDE_SIMPLE
    LV_LVGL_H_INCLUDE_SIMPLE
    USE_SDL=1
    USE_X11=0
)
target_include_directories(buddy_lvgl_pc_sim PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/include"
    "${CMAKE_CURRENT_LIST_DIR}/src"
    "${BUDDY_ROOT}/app/src"
    "${BUDDY_ROOT}/app/src/ascii"
    "${BUDDY_ROOT}/app/src/bridge"
    "${BUDDY_ROOT}/app/src/storage"
    "${BUDDY_ROOT}/app/src/ui"
    "${LV_PORT_DIR}/lvgl/src/extra/libs/gif"
    "${LV_PORT_DIR}/lv_drivers"
    ${BUDDY_SIM_SDL_INCLUDES}
)
target_compile_options(buddy_lvgl_pc_sim PRIVATE ${BUDDY_SIM_SDL_CFLAGS_LIST})
target_link_libraries(buddy_lvgl_pc_sim PRIVATE lvgl lv_drivers_sdl ${BUDDY_SIM_SDL_LIBS} pthread m)

if(APPLE)
    target_link_libraries(buddy_lvgl_pc_sim PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()

add_custom_target(run
    COMMAND buddy_lvgl_pc_sim
    DEPENDS buddy_lvgl_pc_sim
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
)
