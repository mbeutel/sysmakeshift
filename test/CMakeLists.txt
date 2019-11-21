
# sysmakeshift C++ library
# Author: Moritz Beutel
# sysmakeshift unit tests


cmake_minimum_required(VERSION 3.15)

# dependencies
find_package(Catch2 2.9 REQUIRED)
find_package(CMakeshift 3.7 REQUIRED)
find_package(gsl-lite 0.35.3 REQUIRED)

# test targets
add_executable(test-sysmakeshift
    "main.cpp"
    "test-fenv.cpp"
    "test-memory.cpp"
    "test-new.cpp")

# compiler settings
include(CMakeshift/TargetCompileSettings)
cmakeshift_target_compile_settings(test-sysmakeshift
    PRIVATE
        default
        fp-model=strict)
target_compile_features(test-sysmakeshift
    PUBLIC
        cxx_std_17)

if(MSVC)
    if(CMAKE_CXX_FLAGS MATCHES "/EH[ascr]+")
        string(REGEX REPLACE "/EH[ascr]+" "" CMAKE_CXX_FLAGS_NEW "${CMAKE_CXX_FLAGS}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_NEW}")
    endif()
    target_compile_options(test-sysmakeshift
        PRIVATE
            /EHa) # needed for calling `_set_se_translator()`
elseif(CMAKE_COMPILER_IS_GNUCXX)
    target_compile_options(test-sysmakeshift
        PRIVATE
            -fnon-call-exceptions) # needed to permit throwing exceptions in signal handlers
endif()

# test properties
target_compile_definitions(test-sysmakeshift
    PRIVATE
        CATCH_CONFIG_CONSOLE_WIDTH=120
        CATCH_CLARA_CONFIG_CONSOLE_WIDTH=120
        gsl_CONFIG_CONTRACT_VIOLATION_THROWS) # this makes GSL's Expects()/Ensures() throw `gsl::fail_fast` on failure

# test dependencies
target_link_libraries(test-sysmakeshift
    PRIVATE
        gsl::gsl-lite-v1
        Catch2::Catch2
        sysmakeshift)

# register tests
add_test(NAME test-sysmakeshift COMMAND test-sysmakeshift)
