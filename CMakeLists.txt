cmake_minimum_required(VERSION 3.16)
project(cpu8 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(cpu8core
    src/alu.cpp
    src/memory.cpp
    src/registers.cpp
    src/instruction.cpp
    src/cpu.cpp
    src/assembler.cpp)
target_include_directories(cpu8core PUBLIC include)

add_executable(cpu8 src/main.cpp)
target_link_libraries(cpu8 PRIVATE cpu8core)

enable_testing()
foreach(suite alu cpu assembler)
    add_executable(test_${suite} tests/test_${suite}.cpp)
    target_link_libraries(test_${suite} PRIVATE cpu8core)
    target_include_directories(test_${suite} PRIVATE tests)
    add_test(NAME ${suite} COMMAND test_${suite})
endforeach()
