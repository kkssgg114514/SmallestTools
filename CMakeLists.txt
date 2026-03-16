cmake_minimum_required(VERSION 3.28)
project(SmallestTools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# 先构建底层库，再构建依赖它们的上层工具模块
add_subdirectory(FileTools)
add_subdirectory(ImageTools)
add_subdirectory(LogTools)
