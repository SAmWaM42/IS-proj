cmake_minimum_required(VERSION 4.2.3)
project(IS-2)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(program)
target_sources(program
  PRIVATE
    main.c++
)
target_link_libraries(program
    PRIVATE
        glfw
        GL)
target_include_directories(program PRIVATE
    inc
    Components/Environemnt
    Components/baselineAgent
    Components/Memory
    Components/proposedAgent
    Components/State-Action

)
target_compile_options(program PRIVATE
    -Wall
    -Wextra
)
