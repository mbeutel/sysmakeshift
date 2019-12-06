
#ifndef INCLUDED_SYSMAKESHIFT_BUFFER_HPP_
#define INCLUDED_SYSMAKESHIFT_BUFFER_HPP_


#include <cstddef> // for size_t, ptrdiff_t
#include <memory>  // for unique_ptr<>, allocator<>
#include <utility> // for move(), exchange()

#include <gsl/gsl-lite.hpp> // for Expects(), gsl_NODISCARD

#include <sysmakeshift/memory.hpp> // for alignment, aligned_allocator<>


namespace sysmakeshift
{


template <typename T, alignment Alignment, typename A = std::allocator<T>>
class element_aligned_buffer
{
    // TODO: implement
};


template <typename T, alignment Alignment, typename A = std::allocator<T>>
class row_aligned_buffer
{
    // TODO: implement
};


} // namespace sysmakeshift


#endif // INCLUDED_SYSMAKESHIFT_BUFFER_HPP_
