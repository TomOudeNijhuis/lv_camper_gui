cmake_minimum_required(VERSION 3.10)
project(lvgl)


set(CONF_PATH "${PROJECT_SOURCE_DIR}/lv_conf.h")

foreach(BACKEND_NAME "SDL" "LINUX_DRM" "LINUX_FBDEV" "X11" "WAYLAND" "OPENGLES" "EVDEV")

    execute_process(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                    COMMAND "scripts/backend_conf.sh" ${BACKEND_NAME} ${CONF_PATH} OUTPUT_VARIABLE IS_BACKEND_ENABLED)
    set("LV_USE_${BACKEND_NAME}" ${IS_BACKEND_ENABLED})

endforeach()

# Uncomment if the program needs debugging
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -ggdb")

set(CMAKE_C_STANDARD 99) # LVGL officially supports C99 and above
set(CMAKE_CXX_STANDARD 17) #C17
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)

message("Including EVDEV support")
find_package(PkgConfig REQUIRED)
pkg_check_modules(EVDEV REQUIRED libevdev)

list(APPEND PKG_CONFIG_LIB ${EVDEV_LIBRARIES})
list(APPEND PKG_CONFIG_INC ${EVDEV_INCLUDE_DIRS})

message("Including SDL2 support")
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)
list(APPEND PKG_CONFIG_LIB ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES})
list(APPEND PKG_CONFIG_INC ${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS})
list(APPEND LV_LINUX_BACKEND_SRC src/lib/lv_sdl_disp.c)

message("Including DRM support")
pkg_check_modules(DRM REQUIRED libdrm)
include_directories(${DRM_INCLUDE_DIRS})
list(APPEND PKG_CONFIG_LIB ${DRM_LIBRARIES})
list(APPEND PKG_CONFIG_INC ${DRM_INCLUDE_DIRS})

message("Including libcurl support")
find_package(CURL REQUIRED)
list(APPEND PKG_CONFIG_LIB ${CURL_LIBRARIES})
list(APPEND PKG_CONFIG_INC ${CURL_INCLUDE_DIRS})

message("Including JSON-C support")
pkg_check_modules(JSONC REQUIRED json-c)
list(APPEND PKG_CONFIG_LIB ${JSONC_LIBRARIES})
list(APPEND PKG_CONFIG_INC ${JSONC_INCLUDE_DIRS})

foreach(arg ${PKG_CONFIG_LIB})
    string(APPEND LVGL_PKG_CONFIG_EXT_LIB " -l${arg}")
endforeach()

string(APPEND LVGL_PKG_CONFIG_LIB "-llvgl_linux")

file(GLOB LV_LINUX_LIB_SRC src/lib/*.c)
file(GLOB LV_LINUX_UI_SRC src/ui/*.c)
set(LV_LINUX_INC src/lib)

add_subdirectory(lvgl)
target_include_directories(lvgl PUBLIC ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src/lib ${PKG_CONFIG_INC})
add_library(lvgl_linux STATIC ${LV_LINUX_LIB_SRC} ${LV_LINUX_UI_SRC} ${LV_LINUX_BACKEND_SRC})
target_include_directories(lvgl_linux PRIVATE ${LV_LINUX_INC} ${PROJECT_SOURCE_DIR})

add_executable(camper-gui src/main.c ${LV_LINUX_LIB_SRC} ${LV_LINUX_UI_SRC} ${LV_LINUX_BACKEND_SRC})
target_link_libraries(camper-gui lvgl_linux lvgl lvgl::thorvg m pthread ${PKG_CONFIG_LIB})

# Install the lvgl_linux library and its headers
install(DIRECTORY src/lib/
    DESTINATION include/lvgl
    FILES_MATCHING
    PATTERN "backends*" EXCLUDE
    PATTERN "*.h")

install(TARGETS lvgl_linux
    ARCHIVE DESTINATION lib
)

add_custom_target (run COMMAND ${EXECUTABLE_OUTPUT_PATH}/camper-gui DEPENDS camper-gui)
