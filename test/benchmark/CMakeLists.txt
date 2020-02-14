
# sysmakeshift C++ library
# Author: Moritz Beutel
# sysmakeshift benchmarks


cmake_minimum_required(VERSION 3.14)

# dependencies
find_package(CMakeshift 3.8 REQUIRED)
find_package(Threads REQUIRED)
find_package(gsl-lite 0.36 REQUIRED)
find_package(Catch2 2.9 REQUIRED)

include(CMakeshift/TargetCompileSettings)

# benchmark runner target
add_library(benchmark-sysmakeshift-runner OBJECT "main.cpp")
target_compile_features(benchmark-sysmakeshift-runner PRIVATE cxx_std_14)
target_compile_definitions(benchmark-sysmakeshift-runner PRIVATE CATCH_CONFIG_ENABLE_BENCHMARKING)
cmakeshift_target_compile_settings(benchmark-sysmakeshift-runner PRIVATE default)
target_link_libraries(benchmark-sysmakeshift-runner
    PRIVATE
        Catch2::Catch2)

# benchmark target
add_executable(benchmark-sysmakeshift
    "benchmark-thread_squad.cpp")

# compiler settings
target_include_directories(benchmark-sysmakeshift
    PRIVATE
        "${PROJECT_SOURCE_DIR}/test/benchmark")
cmakeshift_target_compile_settings(benchmark-sysmakeshift
    PRIVATE
        default)
target_compile_definitions(benchmark-sysmakeshift
    PRIVATE
        CATCH_CONFIG_ENABLE_BENCHMARKING)
target_compile_features(benchmark-sysmakeshift PRIVATE cxx_std_14)

# benchmark properties
target_compile_definitions(benchmark-sysmakeshift
    PRIVATE
        CATCH_CONFIG_CONSOLE_WIDTH=120
        CATCH_CLARA_CONFIG_CONSOLE_WIDTH=120)

# benchmark dependencies
target_link_libraries(benchmark-sysmakeshift
    PRIVATE
        gsl::gsl-lite-v1
        Catch2::Catch2
        sysmakeshift
        benchmark-sysmakeshift-runner)
