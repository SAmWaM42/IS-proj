cmake_minimum_required(VERSION 3.16)
project(IS-2 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SFML 3.0 REQUIRED COMPONENTS Graphics Window System)

add_executable(IS-2
   main.c++
   Components/State-Action/state-action.cpp
   Components/Memory/memory.cpp
   Components/Training/training.cpp
   Components/Study/study.cpp
   Components/Simulation/companion-ui.cpp
   Components/Simulation/backyard-environment.cpp
)

target_include_directories(IS-2 PRIVATE
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_SOURCE_DIR}/Inc
   ${CMAKE_SOURCE_DIR}/Components/Memory
   ${CMAKE_SOURCE_DIR}/Components/State-Action
   ${CMAKE_SOURCE_DIR}/Components/Training
   ${CMAKE_SOURCE_DIR}/Components/Study
   ${CMAKE_SOURCE_DIR}/Components/Simulation
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

enable_testing()

add_executable(backyard_environment_test
   tests/backyard_environment_test.cpp
   Components/Simulation/backyard-environment.cpp
)
target_include_directories(backyard_environment_test PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_options(backyard_environment_test PRIVATE -Wall -Wextra)
add_test(NAME backyard_environment_test COMMAND backyard_environment_test)

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
