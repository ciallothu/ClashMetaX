# SysProxyBar - Windows系统代理控制工具
# 编译要求：CMake 3.15+, TDM-GCC 10.3.0 或 Visual Studio 2019+
cmake_minimum_required(VERSION 3.15)
project(SysProxyBar VERSION 2.3 LANGUAGES CXX)

# C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置输出目录为 release/v{VERSION}
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/release/v${PROJECT_VERSION})

# ============== 源文件 ==============
set(SOURCES
    src/main.cpp
    src/proxy_manager.cpp
    src/tray_icon.cpp
    src/settings_dialog.cpp
    src/http_server.cpp
    src/mihomo_manager.cpp
)

# 头文件
set(HEADERS
    src/proxy_manager.h
    src/tray_icon.h
    src/settings_dialog.h
    src/resource.h
    src/http_server.h
    src/webui_resource.h
    src/mihomo_manager.h
    src/mihomo_resource.h
)

# 资源文件（对话框、图标等）
set(RESOURCES
    src/resource.rc
    src/webui.rc
    src/mihomo.rc
)

# 创建可执行文件（Windows GUI程序）
add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS} ${RESOURCES})

# ============== 链接库 ==============
target_link_libraries(${PROJECT_NAME}
    wininet      # Internet设置API
    shell32      # Shell功能（托盘图标等）
    gdi32        # GDI图形功能
    comctl32     # 通用控件
    ws2_32       # Windows Sockets（HTTP服务器）
)

# ============== 编译选项 ==============
if(MSVC)
    # Visual Studio 编译选项
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /utf-8)
    target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)

elseif(MINGW)
    # MinGW/TDM-GCC 编译选项
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra           # 启用常用警告
        -fexec-charset=GBK      # 中文字符串使用GBK编码
    )

    target_compile_definitions(${PROJECT_NAME} PRIVATE
        UNICODE _UNICODE        # Unicode支持
        __USE_MINGW_ANSI_STDIO=1  # 使用MinGW的stdio
    )

    # 静态链接，避免运行时依赖DLL
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")

else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
endif()

# ============== 安装规则 ==============
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION .
)

# ============== 打包配置 ==============
set(CPACK_PACKAGE_NAME "SysProxyBar")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR "ZIP;NSIS")
include(CPack)
