cmake_minimum_required(VERSION 3.10)

project(cp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

include_directories("src/", "src/FileStat/", "src/util/")

add_executable(
        cp
        main.cpp
        src/FileStat/FileStat.cpp
        src/FileStat/FileStat.h
        src/util/util.cpp
        src/util/util.h
)