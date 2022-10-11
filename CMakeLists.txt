
# sysmakeshift C++ library
# Author: Moritz Beutel


cmake_minimum_required(VERSION 3.20)

# Define the project.
project(sysmakeshift
    VERSION 0.2.2
    LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Define build options.
option(BUILD_TESTING "Build tests" OFF)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)

# Include target definitions.
add_subdirectory(src)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()
if(BUILD_BENCHMARKS)
    add_subdirectory(test/benchmark)
endif()

# Install.
include(InstallBasicPackageFiles)
cmakeshift_install_basic_package_files(sysmakeshift
    COMPATIBILITY SameMajorVersion
    DEPENDENCIES
        "Threads"
        "gsl-lite 0.40"
)
