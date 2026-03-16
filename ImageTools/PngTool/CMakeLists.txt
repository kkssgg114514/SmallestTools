cmake_minimum_required(VERSION 3.28)
project(PngTool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# PngTool 本体作为可复用静态库提供给其他模块链接
add_library(PngTool STATIC)
target_sources(PngTool
    PRIVATE
        Sources/private/ST_png.cpp
)
target_include_directories(PngTool
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/public
)
target_link_libraries(PngTool PUBLIC FileTools)
target_compile_features(PngTool PUBLIC cxx_std_17)

if (MSVC)
    target_compile_options(PngTool PRIVATE /utf-8)
endif()

# 单独保留一个演示程序，用来展示 PNG 读取、修改和重写流程
add_executable(PngToolDemo main.cpp)
target_link_libraries(PngToolDemo PRIVATE PngTool)
target_compile_features(PngToolDemo PRIVATE cxx_std_17)

if (MSVC)
    target_compile_options(PngToolDemo PRIVATE /utf-8)
endif()
