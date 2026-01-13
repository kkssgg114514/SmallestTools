cmake_minimum_required(VERSION 3.28)
project(FileTools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

include_directories(Sources/public)
aux_source_directory(Sources/private SOURCES)

add_executable(FileTools main.cpp ${SOURCES})
