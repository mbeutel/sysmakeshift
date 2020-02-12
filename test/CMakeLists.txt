
# sysmakeshift C++ library
# Author: Moritz Beutel
# sysmakeshift unit tests


cmake_minimum_required(VERSION 3.14)

# dependencies
find_package(CMakeshift 3.8 REQUIRED)
find_package(Threads REQUIRED)
find_package(gsl-lite 0.36 REQUIRED)
find_package(Catch2 2.9 REQUIRED)

include(CMakeshift/TargetCompileSettings)

# test runner target
add_library(test-sysmakeshift-runner OBJECT "main.cpp")
target_compile_features(test-sysmakeshift-runner PRIVATE cxx_std_14)
cmakeshift_target_compile_settings(test-sysmakeshift-runner PRIVATE default)
target_link_libraries(test-sysmakeshift-runner
    PRIVATE
        Catch2::Catch2)

# test targets
add_executable(test-sysmakeshift
    "test-buffer.cpp"
    "test-fenv.cpp"
    "test-memory.cpp"
    "test-new.cpp"
    "test-thread.cpp"
    "test-thread_squad.cpp")

# compiler settings
cmakeshift_target_compile_settings(test-sysmakeshift
    PRIVATE
        default
        fp-model=strict) # to prevent our fenv tests from being broken by the optimizer
target_compile_features(test-sysmakeshift PUBLIC cxx_std_14)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
    target_precompile_headers(test-sysmakeshift
        PRIVATE
            # C standard library
            <cstddef>
            # C++ standard library
            <new>
            <memory>
            <string>
            <utility>
            <exception>
            <algorithm>
            <type_traits>
            <system_error>
            # other libraries
            <gsl-lite/gsl-lite.hpp>
            <catch2/catch.hpp>)
endif()
if(MSVC)
    if(CMAKE_CXX_FLAGS MATCHES "/EH[ascr]+")
        string(REGEX REPLACE "/EH[ascr]+" "" CMAKE_CXX_FLAGS_NEW "${CMAKE_CXX_FLAGS}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_NEW} /EHa") # /EHa needed for calling `_set_se_translator()`
    endif()
elseif(CMAKE_COMPILER_IS_GNUCXX)
    target_compile_options(test-sysmakeshift PRIVATE -fnon-call-exceptions) # needed to permit throwing exceptions in signal handlers
endif()

# test properties
target_compile_definitions(test-sysmakeshift
    PRIVATE
        CATCH_CONFIG_CONSOLE_WIDTH=120
        CATCH_CLARA_CONFIG_CONSOLE_WIDTH=120
        gsl_CONFIG_CONTRACT_CHECKING_AUDIT # run audit-level checks
        gsl_CONFIG_CONTRACT_VIOLATION_THROWS) # this makes gsl-lite's precondition assertions throw `gsl::fail_fast` on failure

# test dependencies
target_link_libraries(test-sysmakeshift
    PRIVATE
        Threads::Threads
        gsl::gsl-lite-v1
        Catch2::Catch2
        sysmakeshift
        test-sysmakeshift-runner)

# register tests
add_test(NAME test-sysmakeshift COMMAND test-sysmakeshift)
set_property(TEST test-sysmakeshift PROPERTY FAIL_REGULAR_EXPRESSION "Sanitizer")
