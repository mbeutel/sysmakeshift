
#include <sysmakeshift/thread_squad.hpp>

#include <catch2/catch.hpp>

#include "params.hpp"


#if defined(_WIN32) || defined(__linux__)
# define THREAD_PINNING_SUPPORTED
#endif // defined(_WIN32) || defined(__linux__)


TEST_CASE("thread_squad: create-run-destroy")
{
    auto params = sysmakeshift::thread_squad::params{
        /*.num_threads = */ global_benchmark_params.num_threads
    };
#ifdef THREAD_PINNING_SUPPORTED
    params.pin_to_hardware_threads = true;
#endif // !THREAD_PINNING_SUPPORTED

    auto action = []
    (sysmakeshift::thread_squad::task_context /*ctx*/)
    {
    };

    BENCHMARK("create-run-destroy")
    {
        sysmakeshift::thread_squad(params).run(action);
    };
}

TEST_CASE("thread_squad: run")
{
    auto params = sysmakeshift::thread_squad::params{
        /*.num_threads = */ global_benchmark_params.num_threads
    };
#ifdef THREAD_PINNING_SUPPORTED
    params.pin_to_hardware_threads = true;
#endif // !THREAD_PINNING_SUPPORTED

    auto action = []
    (sysmakeshift::thread_squad::task_context /*ctx*/)
    {
    };

    auto threadSquad = sysmakeshift::thread_squad(params);

    BENCHMARK("run")
    {
        threadSquad.run(action);
    };
}
