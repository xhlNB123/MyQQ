# =====================================================================
# MyQQ —— 顶层 CMake 构建
# 构建 common 库、服务端、控制台测试客户端（均为 Windows 控制台程序）。
# MFC 客户端（GUI）请在 Visual Studio 中单独创建，见 src/client/README.md。
#
# 用法（在 build/ 下）：
#   cmake .. -G "Visual Studio 17 2022"
#   cmake --build . --config Debug
# =====================================================================
cmake_minimum_required(VERSION 3.15)

# MSVC 发布版统一静态链接 CRT（Release=/MT，Debug=/MTd），
# 生成的服务端复制到未安装 Visual Studio/VC++ 运行库的机器也能启动。
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()
project(MyQQ LANGUAGES C CXX)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
    message(WARNING "MyQQ 依赖 Winsock，仅支持 Windows 平台。")
endif()

# ---------- 公共库 ----------
add_library(myqq_common
    src/common/Message.cpp
    src/common/Socket.cpp
    src/common/ChatLogger.cpp
)
target_include_directories(myqq_common PUBLIC src/common)
if(MSVC)
    target_compile_options(myqq_common PRIVATE /utf-8)
endif()
if(WIN32)
    target_link_libraries(myqq_common PUBLIC ws2_32)
endif()

# ---------- SQLite（第三方，单文件）----------
add_library(sqlite3 STATIC third_party/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC third_party/sqlite)

# ---------- 服务端 ----------
add_executable(MyQQServer
    src/server/ServerMain.cpp
    src/server/Database.cpp
    src/server/SessionManager.cpp
)
target_link_libraries(MyQQServer PRIVATE myqq_common sqlite3)
if(MSVC)
    target_compile_options(MyQQServer PRIVATE /utf-8)
endif()

# ---------- 控制台测试客户端 ----------
add_executable(MyQQConsoleClient src/client/ConsoleClient.cpp)
target_link_libraries(MyQQConsoleClient PRIVATE myqq_common)
if(MSVC)
    target_compile_options(MyQQConsoleClient PRIVATE /utf-8)
endif()
