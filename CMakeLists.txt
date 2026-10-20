cmake_minimum_required(VERSION 3.16)
project(IS-2 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SFML 3.0 REQUIRED COMPONENTS Graphics Window System)

add_executable(IS-2
   main.c++
   Components/State-Action/state-action.cpp
   Components/Memory/memory.cpp
   Components/Environment/environment.cpp
   Components/Training/training.cpp
)

target_include_directories(IS-2 PRIVATE
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_SOURCE_DIR}/Inc
   ${CMAKE_SOURCE_DIR}/Components/Environment
   ${CMAKE_SOURCE_DIR}/Components/Memory
   ${CMAKE_SOURCE_DIR}/Components/State-Action
   ${CMAKE_SOURCE_DIR}/Components/Training
)

target_link_libraries(IS-2 PRIVATE
   sfml-graphics
   sfml-window
   sfml-system
)

target_compile_options(IS-2 PRIVATE
   -Wall
   -Wextra
)

add_executable(tokenizer_probe
   tests/tokenizer_probe.cpp
)

target_include_directories(tokenizer_probe PRIVATE
   ${CMAKE_SOURCE_DIR}/Inc
)

target_compile_options(tokenizer_probe PRIVATE
   -Wall
   -Wextra
)
