cmake_minimum_required(VERSION 3.10)
project(ChatServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 导出 compile_commands.json，供 IDE (VSCode C/C++) 解析 include 路径
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(src third_party)

set(SOURCES
    src/main.cpp
    src/TcpServer.cpp
    src/Config.cpp
    src/ThreadPool.cpp
    src/HttpParser.cpp
    src/HttpResponse.cpp
    src/UserManager.cpp
    src/WebSocket.cpp
    src/ChatRoom.cpp
    src/Database.cpp
    src/Crypto.cpp
)

add_executable(chat_server ${SOURCES})

target_link_libraries(chat_server 
    pthread 
    ssl 
    crypto 
    mysqlclient
)