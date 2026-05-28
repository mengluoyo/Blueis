cmake_minimum_required(VERSION 3.15)
project(Blueis LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 收集各模块源文件
file(GLOB STORAGE_SRC  src/storage/*.cpp)
file(GLOB PROTOCOL_SRC src/protocol/*.cpp)
file(GLOB SERVER_SRC   src/server/*.cpp)
file(GLOB SQL_SRC       src/sql/*.cpp)
file(GLOB PERSIST_SRC   src/persistence/*.cpp)

add_executable(blueis
    main.cpp
    ${STORAGE_SRC}
    ${PROTOCOL_SRC}
    ${SERVER_SRC}
    ${SQL_SRC}
    ${PERSIST_SRC}
)
target_include_directories(blueis PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

if(WIN32)
    target_link_libraries(blueis PRIVATE ws2_32)
endif()
