cmake_minimum_required(VERSION 3.20)
project(cabinet_ui_preview C)
set(CMAKE_C_STANDARD 11)
get_filename_component(CAB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
file(GLOB_RECURSE LVGL_SOURCES "${CAB_ROOT}/lvgl/src/*.c")
add_executable(cabinet_ui_preview main.c ${LVGL_SOURCES}
    ${CAB_ROOT}/APP/UI/bsp_cabinet_ui.c ${CAB_ROOT}/APP/UI/cabinet_font_24.c
    ${CAB_ROOT}/Common/Src/cabinet_core.c)
target_include_directories(cabinet_ui_preview PRIVATE ${CAB_ROOT}/lvgl ${CAB_ROOT}/Common/Inc
    ${CAB_ROOT}/APP/SYSTEM ${CAB_ROOT}/APP/UI)
target_compile_definitions(cabinet_ui_preview PRIVATE CABINET_UI_HOST LV_CONF_INCLUDE_SIMPLE LV_LVGL_H_INCLUDE_SIMPLE)
target_compile_options(cabinet_ui_preview PRIVATE -O2 -UNDEBUG -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
target_link_libraries(cabinet_ui_preview PRIVATE m)
