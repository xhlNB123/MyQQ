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
project(MyQQ LANGUAGES CXX)

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

# ---------- 控制台测试客户端 ----------
add_executable(MyQQConsoleClient src/client/ConsoleClient.cpp)
target_link_libraries(MyQQConsoleClient PRIVATE myqq_common)
