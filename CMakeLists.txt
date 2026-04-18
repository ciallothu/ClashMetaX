# ClashMetaX - Windows系统代理控制工具
cmake_minimum_required(VERSION 3.15)
project(ClashMetaX VERSION 3.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/release/v${PROJECT_VERSION})

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(GENERATED_ICON_ON ${CMAKE_SOURCE_DIR}/src/res/icons/proxy-on.ico)
set(GENERATED_ICON_OFF ${CMAKE_SOURCE_DIR}/src/res/icons/proxy-off.ico)
add_custom_command(
    OUTPUT ${GENERATED_ICON_ON} ${GENERATED_ICON_OFF}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/generate_icons.py
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/generate_icons.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating ClashMetaX icon assets"
)
add_custom_target(generate_icons ALL DEPENDS ${GENERATED_ICON_ON} ${GENERATED_ICON_OFF})

set(SOURCES
    src/main.cpp
    src/proxy_manager.cpp
    src/tray_icon.cpp
    src/settings_dialog.cpp
    src/http_server.cpp
    src/mihomo_manager.cpp
)

set(HEADERS
    src/proxy_manager.h
    src/tray_icon.h
    src/settings_dialog.h
    src/resource.h
    src/http_server.h
    src/webui_resource.h
    src/mihomo_manager.h
)

set(RESOURCES
    src/resource.rc
    src/webui.rc
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS} ${RESOURCES})
add_dependencies(${PROJECT_NAME} generate_icons)

target_link_libraries(${PROJECT_NAME}
    advapi32
    wininet
    shell32
    gdi32
    comctl32
    ws2_32
    urlmon
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /utf-8)
    target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)
elseif(MINGW)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -fexec-charset=GBK)
    target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE __USE_MINGW_ANSI_STDIO=1)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
endif()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)

set(CPACK_PACKAGE_NAME "ClashMetaX")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR "ZIP;NSIS")
include(CPack)
