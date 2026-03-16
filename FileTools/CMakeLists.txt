cmake_minimum_required(VERSION 3.28)
project(FileTools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# FileTools 作为底层文件访问库提供给其他工具模块复用
add_library(FileTools STATIC)
target_sources(FileTools
    PRIVATE
        Sources/private/FileOperation.cpp
)
target_include_directories(FileTools
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/public
)
target_compile_features(FileTools PUBLIC cxx_std_17)

if (MSVC)
    target_compile_options(FileTools PRIVATE /utf-8)
endif()

# 保留一个独立示例程序，用来验证 FileOperation 的基础行为
add_executable(FileToolsDemo main.cpp)
target_link_libraries(FileToolsDemo PRIVATE FileTools)
target_compile_features(FileToolsDemo PRIVATE cxx_std_17)

if (MSVC)
    target_compile_options(FileToolsDemo PRIVATE /utf-8)
endif()
