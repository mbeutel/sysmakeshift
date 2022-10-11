
# sysmakeshift C++ library
# Author: Moritz Beutel
# sysmakeshift benchmarks


cmake_minimum_required(VERSION 3.20)

# dependencies
find_package(Threads REQUIRED)
find_package(gsl-lite 0.4 REQUIRED)
find_package(Catch2 3 REQUIRED)

include(TargetCompileSettings)

# benchmark runner target
add_library(benchmark-sysmakeshift-runner OBJECT "main.cpp")
target_compile_features(benchmark-sysmakeshift-runner PRIVATE cxx_std_14)
target_compile_definitions(benchmark-sysmakeshift-runner PRIVATE CATCH_CONFIG_ENABLE_BENCHMARKING)
cmakeshift_target_compile_settings(benchmark-sysmakeshift-runner
    SOURCE_FILE_ENCODING "UTF-8"
)
target_link_libraries(benchmark-sysmakeshift-runner
    PRIVATE
        Catch2::Catch2
)

# benchmark target
add_executable(benchmark-sysmakeshift
    "benchmark-thread_squad.cpp"
)

# compiler settings
target_include_directories(benchmark-sysmakeshift
    PRIVATE
        "${PROJECT_SOURCE_DIR}/test/benchmark"
)
cmakeshift_target_compile_settings(benchmark-sysmakeshift
    SOURCE_FILE_ENCODING "UTF-8"
)
target_compile_features(benchmark-sysmakeshift PRIVATE cxx_std_14)

# benchmark properties
#target_compile_definitions(benchmark-sysmakeshift
#    PRIVATE
#        #CATCH_CONFIG_CONSOLE_WIDTH=120
#        #CATCH_CLARA_CONFIG_CONSOLE_WIDTH=120
#)

# benchmark dependencies
target_link_libraries(benchmark-sysmakeshift
    PRIVATE
        gsl::gsl-lite-v1
        Catch2::Catch2
        sysmakeshift
        benchmark-sysmakeshift-runner
)
