cmake_minimum_required(VERSION 3.25)

project(cpp20_course LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

option(CPP20_COURSE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(CPP20_COURSE_BUILD_PROJECT "Build the capstone project" ON)
option(CPP20_COURSE_BUILD_CAREER "Build the career bootcamp labs" ON)
option(CPP20_COURSE_SANITIZE "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

add_library(course_warnings INTERFACE)
if(MSVC)
    target_compile_options(course_warnings INTERFACE /W4 /permissive- /Zc:__cplusplus)
    if(CPP20_COURSE_WARNINGS_AS_ERRORS)
        target_compile_options(course_warnings INTERFACE /WX)
    endif()
else()
    target_compile_options(course_warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Wnon-virtual-dtor
    )
    if(CPP20_COURSE_WARNINGS_AS_ERRORS)
        target_compile_options(course_warnings INTERFACE -Werror)
    endif()
endif()

if(CPP20_COURSE_SANITIZE)
    if(MSVC)
        target_compile_options(course_warnings INTERFACE
            /fsanitize=address
            /Zi
        )
        target_link_options(course_warnings INTERFACE /fsanitize=address)
    else()
        target_compile_options(course_warnings INTERFACE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(course_warnings INTERFACE
            -fsanitize=address,undefined
        )
    endif()
endif()

enable_testing()

add_executable(labs_starter
    labs/tests.cpp
    labs/starter/labs.cpp
)
target_include_directories(labs_starter PRIVATE labs)
target_link_libraries(labs_starter PRIVATE course_warnings)
target_compile_definitions(labs_starter PRIVATE CPP20_COURSE_STARTER=1)
# The starter is intentionally incomplete. Run it manually while learning;
# CTest checks the reference implementation and the capstone project.

add_executable(labs_solution
    labs/tests.cpp
    labs/solutions/labs.cpp
)
target_include_directories(labs_solution PRIVATE labs)
target_link_libraries(labs_solution PRIVATE
    course_warnings
    Threads::Threads
)
add_test(NAME labs_solution COMMAND labs_solution)

add_executable(example_container_mutation examples/container_mutation.cpp)
target_link_libraries(example_container_mutation PRIVATE course_warnings)
add_test(NAME example_container_mutation COMMAND example_container_mutation)

add_executable(example_compatibility_probe examples/compatibility_probe.cpp)
target_link_libraries(example_compatibility_probe PRIVATE
    course_warnings
    Threads::Threads
)
add_test(NAME example_compatibility_probe COMMAND example_compatibility_probe)

add_executable(example_blocking_queue examples/blocking_queue.cpp)
target_link_libraries(example_blocking_queue PRIVATE
    course_warnings
    Threads::Threads
)
add_test(NAME example_blocking_queue COMMAND example_blocking_queue)

add_executable(example_file_io examples/file_io.cpp)
target_link_libraries(example_file_io PRIVATE course_warnings)
add_test(NAME example_file_io COMMAND example_file_io)

if(CPP20_COURSE_BUILD_CAREER AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/career_bootcamp/labs")
    add_subdirectory(career_bootcamp/labs)
endif()

if(CPP20_COURSE_BUILD_PROJECT)
    add_subdirectory(projects/log_analyzer)
endif()

if(MSVC)
    target_compile_options(labs_starter PRIVATE /utf-8)
    target_compile_options(labs_solution PRIVATE /utf-8)
endif()
