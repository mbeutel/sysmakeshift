
#include <thread> // for thread::hardware_concurrency()

#include <sysmakeshift/thread.hpp>

#include <iostream>

#include <catch2/catch.hpp>


TEST_CASE("physical_concurrency() returns correct value")
{
    unsigned physicalConcurrency = sysmakeshift::physical_concurrency();
    std::cout << "Hardware concurrency: " << std::thread::hardware_concurrency() << " cores\n";
    std::cout << "Physical concurrency: " << physicalConcurrency << " cores\n";

    CHECK(physicalConcurrency != 0);
    CHECK(physicalConcurrency <= std::thread::hardware_concurrency());
}
