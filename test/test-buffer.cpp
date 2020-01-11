
#include <sysmakeshift/buffer.hpp>

#include <gsl-lite/gsl-lite.hpp> // for gsl_CPP17_OR_GREATER

#include <catch2/catch.hpp>


namespace {


namespace gsl = ::gsl_lite;


TEST_CASE("aligned_buffer<> properly aligns elements")
{
    constexpr auto alignment = sysmakeshift::alignment(4 * sizeof(int));
    std::size_t numElements = GENERATE(range(0, 9));

    auto bufNI = sysmakeshift::aligned_buffer<int, alignment>(numElements);
    auto buf42A = sysmakeshift::aligned_buffer<int, alignment>(numElements, 42);
#if gsl_CPP17_OR_GREATER
    auto buf42B = sysmakeshift::aligned_buffer<int, alignment>(numElements, std::in_place, 42);
#endif // gsl_CPP17_OR_GREATER

    // TODO: add checks
}


TEST_CASE("aligned_row_buffer<> properly aligns elements")
{
    constexpr auto alignment = sysmakeshift::alignment(4 * sizeof(int));
    std::size_t numRows = GENERATE(range(0, 2));
    std::size_t numCols = GENERATE(range(0, 9));

    auto bufNI = sysmakeshift::aligned_row_buffer<int, alignment>(numRows, numCols);
    auto buf42A = sysmakeshift::aligned_row_buffer<int, alignment>(numRows, numCols, 42);
#if gsl_CPP17_OR_GREATER
    auto buf42B = sysmakeshift::aligned_row_buffer<int, alignment>(numRows, numCols, std::in_place, 42);
#endif // gsl_CPP17_OR_GREATER

    // TODO: add checks
}


} // anonymous namespace
