
# sysmakeshift C++ library
# Author: Moritz Beutel


cmake_minimum_required(VERSION 3.14)

# Define the project.
project(sysmakeshift
    VERSION 0.1.1
    LANGUAGES CXX)

# Define build options.
option(SYSMAKESHIFT_BUILD_TESTS "Build tests" OFF)

# Include target definitions.
add_subdirectory(src)
if(SYSMAKESHIFT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# Install.
find_package(CMakeshift 3.7 REQUIRED)
include(CMakeshift/InstallBasicPackageFiles)
cmakeshift_install_basic_package_files(sysmakeshift
    COMPATIBILITY SameMajorVersion
    DEPENDENCIES
        "Threads"
        "gsl-lite 0.35.5")
