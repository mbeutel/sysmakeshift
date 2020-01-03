
#include <sysmakeshift/buffer.hpp>

#include <catch2/catch.hpp>


namespace {



TEST_CASE("aligned_buffer<> properly aligns elements")
{
    constexpr auto alignment = sysmakeshift::alignment(4 * sizeof(int));
    std::size_t numElements = GENERATE(range(0, 9));

    auto bufNI = sysmakeshift::aligned_buffer<int, alignment>(numElements);
    auto buf42A = sysmakeshift::aligned_buffer<int, alignment>(numElements, 42);
    auto buf42B = sysmakeshift::aligned_buffer<int, alignment>(numElements, std::in_place, 42);

    // TODO: add tests
}


} // anonymous namespace
