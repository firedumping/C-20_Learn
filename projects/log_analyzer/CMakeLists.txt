add_library(log_core
    src/analyzer.cpp
    src/report.cpp
)

target_include_directories(log_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(log_core PUBLIC cxx_std_20)
target_link_libraries(log_core PUBLIC course_warnings)

add_executable(log_analyzer src/main.cpp)
target_link_libraries(log_analyzer PRIVATE log_core)

add_executable(log_analyzer_tests tests/tests.cpp)
target_link_libraries(log_analyzer_tests PRIVATE log_core)

add_test(NAME log_analyzer_tests COMMAND log_analyzer_tests)
add_test(
    NAME log_analyzer_smoke
    COMMAND log_analyzer ${CMAKE_CURRENT_SOURCE_DIR}/sample.log
)
set_tests_properties(log_analyzer_smoke PROPERTIES
    PASS_REGULAR_EXPRESSION "valid:   5"
)

if(MSVC)
    target_compile_options(log_core PRIVATE /utf-8)
    target_compile_options(log_analyzer PRIVATE /utf-8)
    target_compile_options(log_analyzer_tests PRIVATE /utf-8)
endif()
