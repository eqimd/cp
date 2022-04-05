cmake_minimum_required(VERSION 3.10)

project(ldd-repl)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

include_directories("elf_file", "util")

add_executable(ldd-repl
        main.cpp elf_file/elf_file.cpp elf_file/elf_file.h util/util.cpp util/util.h)