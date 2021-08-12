cmake_minimum_required (VERSION 3.1)
project(lgtbot-mirai LANGUAGES CXX C)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++2a")

# 不要构建 mirai-cpp 自带的 examples
set(MIRAI_CPP_BUILD_EXAMPLES OFF CACHE BOOL "Build examples" FORCE)

add_subdirectory(mirai-cpp)
add_subdirectory(lgtbot)

# 源码以 UTF-8 编码保存，使用 MSVC 编译器时需要加编译参数提供支持
if(MSVC)
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    message(STATUS "optional: /UTF-8")   
endif(MSVC)

find_package(gflags REQUIRED)

add_executable(${PROJECT_NAME} src/main.cpp)

include_directories(include ${CMAKE_CURRENT_SOURCE_DIR}/lgtbot)

target_link_libraries(${PROJECT_NAME} PRIVATE mirai-cpp gflags bot_core_static)
target_compile_definitions(${PROJECT_NAME} PRIVATE MIRAICPP_STATICLIB)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(${PROJECT_NAME} PRIVATE pthread)
endif()
