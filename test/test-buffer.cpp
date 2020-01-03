
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

    // TODO: add checks
}


TEST_CASE("aligned_row_buffer<> properly aligns elements")
{
    constexpr auto alignment = sysmakeshift::alignment(4 * sizeof(int));
    std::size_t numRows = GENERATE(range(0, 2));
    std::size_t numCols = GENERATE(range(0, 9));

    auto bufNI = sysmakeshift::aligned_row_buffer<int, alignment>(numRows, numCols);
    auto buf42A = sysmakeshift::aligned_row_buffer<int, alignment>(numRows, numCols, 42);
    auto buf42B = sysmakeshift::aligned_row_buffer<int, alignment>(numRows, numCols, std::in_place, 42);

    // TODO: add checks
}


} // anonymous namespace
