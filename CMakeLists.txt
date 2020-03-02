
# sysmakeshift C++ library
# Author: Moritz Beutel


cmake_minimum_required(VERSION 3.14)

# Define the project.
project(sysmakeshift
    VERSION 0.2.1
    LANGUAGES CXX)

# Define build options.
option(SYSMAKESHIFT_BUILD_TESTS "Build tests" OFF)
option(SYSMAKESHIFT_BUILD_BENCHMARKS "Build benchmarks" OFF)

# Include target definitions.
add_subdirectory(src)
if(SYSMAKESHIFT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
if(SYSMAKESHIFT_BUILD_BENCHMARKS)
    add_subdirectory(test/benchmark)
endif()

# Install.
find_package(CMakeshift 3.8 REQUIRED)
include(CMakeshift/InstallBasicPackageFiles)
cmakeshift_install_basic_package_files(sysmakeshift
    COMPATIBILITY SameMajorVersion
    DEPENDENCIES
        "Threads"
        "gsl-lite 0.36")
