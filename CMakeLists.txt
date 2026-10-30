cmake_minimum_required(VERSION 3.16)
project(IS-2 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SFML 3.0 REQUIRED COMPONENTS Graphics Window System)

execute_process(
   COMMAND git rev-parse --short HEAD
   WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
   OUTPUT_VARIABLE IS2_GIT_REVISION
   OUTPUT_STRIP_TRAILING_WHITESPACE
   ERROR_QUIET
)
if(NOT IS2_GIT_REVISION)
   set(IS2_GIT_REVISION "unknown")
endif()

add_executable(IS-2
   main.c++
   Components/State-Action/state-action.cpp
   Components/Memory/memory.cpp
   Components/Training/training.cpp
   Components/Study/study.cpp
   Components/Simulation/companion-ui.cpp
   Components/Simulation/backyard-environment.cpp
)

target_compile_definitions(IS-2 PRIVATE IS2_SOURCE_DIR="${CMAKE_SOURCE_DIR}" IS2_GIT_REVISION="${IS2_GIT_REVISION}")

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
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME nlp_service_smoke_test
   COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/nlp_service_smoke_test.py
)

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

add_executable(paired_agent_scenarios_test
   tests/paired_agent_scenarios_test.cpp
   Components/State-Action/state-action.cpp
   Components/Memory/memory.cpp
   Components/Training/training.cpp
   Components/Simulation/backyard-environment.cpp
)
target_include_directories(paired_agent_scenarios_test PRIVATE
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_SOURCE_DIR}/Inc
   ${CMAKE_SOURCE_DIR}/Components/Memory
   ${CMAKE_SOURCE_DIR}/Components/State-Action
   ${CMAKE_SOURCE_DIR}/Components/Simulation
)
target_compile_options(paired_agent_scenarios_test PRIVATE -Wall -Wextra)
add_test(NAME paired_agent_scenarios_test COMMAND paired_agent_scenarios_test)

add_test(NAME believability_fixture_validation
   COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/validate_study_fixtures.py
)
add_test(NAME legacy_believability_study_pipeline_tests
   COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_believability_pipeline.py
)

add_test(NAME companion_believability_analysis_tests
   COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_companion_believability_analysis.py
)
