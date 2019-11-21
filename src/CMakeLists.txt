
# sysmakeshift C++ library
# Author: Moritz Beutel
# sysmakeshift library target


cmake_minimum_required(VERSION 3.15)

# dependencies
find_package(CMakeshift 3.7 REQUIRED)
find_package(gsl-lite 0.35.3 REQUIRED)

# library target
add_library(sysmakeshift STATIC
    "fenv.cpp"
    "memory.cpp"
    "new.cpp")

# library properties
target_include_directories(sysmakeshift
    PUBLIC
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>")
target_compile_features(sysmakeshift
    INTERFACE
        cxx_std_14
    PRIVATE
        cxx_std_17)

# compiler settings
include(CMakeshift/TargetCompileSettings)
CMakeshift_target_compile_settings(sysmakeshift
    PRIVATE
        default)

if (NOT MSVC AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Sized deallocation must be explicitly enabled on Clang, cf. https://bcain-llvm.readthedocs.io/projects/clang/en/release_37/ReleaseNotes/#new-compiler-flags.
    target_compile_options(sysmakeshift
        PRIVATE
            -fsized-deallocation)
endif()

# library dependencies
target_link_libraries(sysmakeshift
    PUBLIC
        gsl::gsl-lite-v1)

# installation: public header files, target
install(
    DIRECTORY
        "${PROJECT_SOURCE_DIR}/include/" # the trailing '/' is significant
    TYPE INCLUDE)
install(
    TARGETS sysmakeshift
    EXPORT sysmakeshift
    ARCHIVE INCLUDES)
