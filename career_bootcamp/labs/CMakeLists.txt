cmake_minimum_required(VERSION 3.25)

project(edgebot_career_labs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(edgebot_lab_core
    src/labs.cpp
)
target_include_directories(edgebot_lab_core PUBLIC include)

if(MSVC)
    target_compile_options(edgebot_lab_core PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
    target_compile_options(edgebot_lab_core PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Wnon-virtual-dtor
    )
endif()

add_executable(edgebot_lab_tests
    tests/labs_test.cpp
)
target_link_libraries(edgebot_lab_tests PRIVATE edgebot_lab_core)

enable_testing()
add_test(NAME edgebot_lab_tests COMMAND edgebot_lab_tests)

