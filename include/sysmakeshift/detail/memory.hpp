
#ifndef INCLUDED_SYSMAKESHIFT_DETAIL_MEMORY_HPP_
#define INCLUDED_SYSMAKESHIFT_DETAIL_MEMORY_HPP_


#include <cstddef>     // for size_t, ptrdiff_t
#include <utility>     // for forward<>()
#include <type_traits> // for integral_constant<>
#include <memory>      // for allocator_traits<>

#include <gsl-lite/gsl-lite.hpp> // for void_t<>, negation<>

#include <sysmakeshift/detail/transaction.hpp>


namespace sysmakeshift
{


namespace gsl = ::gsl_lite;


enum class alignment : std::size_t;


namespace detail
{


template <typename T> struct remove_extent_only;
template <typename T> struct remove_extent_only<T[]> { using type = T; };
template <typename T, std::ptrdiff_t N> struct remove_extent_only<T[N]> { using type = T; };
template <typename T> using remove_extent_only_t = typename remove_extent_only<T>::type;

template <typename T> struct extent_only;
template <typename T> struct extent_only<T[]> : std::integral_constant<std::ptrdiff_t, 0> { };
template <typename T, std::ptrdiff_t N> struct extent_only<T[N]> : std::integral_constant<std::ptrdiff_t, N> { };
template <typename T> constexpr std::ptrdiff_t extent_only_v = extent_only<T>::value;


template <typename T, typename A, typename... ArgsT>
T* allocate(std::true_type /*isNothrowConstructible*/, A& alloc, ArgsT&&... args)
{
    T* ptr = std::allocator_traits<A>::allocate(alloc, 1);
    std::allocator_traits<A>::construct(alloc, ptr, std::forward<ArgsT>(args)...);
    return ptr;
}
template <typename T, typename A, typename... ArgsT>
T* allocate(A& alloc, ArgsT&&... args)
{
    T* ptr = std::allocator_traits<A>::allocate(alloc, 1);
    auto transaction = detail::make_transaction(
        gsl::negation<std::is_nothrow_constructible<T, ArgsT...>>{ },
        [&alloc, ptr]
        {
            std::allocator_traits<A>::deallocate(alloc, ptr, 1);
        });
    std::allocator_traits<A>::construct(alloc, ptr, std::forward<ArgsT>(args)...);
    transaction.commit();
    return ptr;
}

template <typename T, typename A, typename SizeC>
T* allocate_array_impl(std::true_type /*isNothrowDefaultConstructible*/, A& alloc, SizeC sizeC)
{
    T* ptr = std::allocator_traits<A>::allocate(alloc, std::size_t(sizeC));
    for (std::ptrdiff_t i = 0; i != std::ptrdiff_t(sizeC); ++i)
    {
        std::allocator_traits<A>::construct(alloc, &ptr[i]);
    }
    return ptr;
}
template <typename T, typename A, typename SizeC>
T* allocate_array_impl(std::false_type /*isNothrowDefaultConstructible*/, A& alloc, SizeC sizeC)
{
    T* ptr = std::allocator_traits<A>::allocate(alloc, std::size_t(sizeC));
    std::ptrdiff_t i = 0;
    auto transaction = detail::make_transaction(
        [&alloc, ptr, &i, sizeC]
        {
            for (std::ptrdiff_t j = i - 1; j >= 0; --j)
            {
                std::allocator_traits<A>::destroy(alloc, &ptr[j]);
            }
            std::allocator_traits<A>::deallocate(alloc, ptr, std::size_t(sizeC));
        });
    for (; i != std::ptrdiff_t(sizeC); ++i)
    {
        std::allocator_traits<A>::construct(alloc, &ptr[i]);
    }
    transaction.commit();
    return ptr;
}
template <typename T, typename A, typename SizeC>
T* allocate_array(A& alloc, SizeC sizeC)
{
    return detail::allocate_array_impl<T>(std::is_nothrow_default_constructible<T>{ }, alloc, sizeC);
}


void* aligned_alloc(std::size_t size, std::size_t alignment);
void aligned_free(void* data, std::size_t size, std::size_t alignment) noexcept;

//#ifdef __linux__
//void advise_large_pages(void* addr, std::size_t size);
//#endif // __linux__

std::size_t alignment_in_bytes(alignment a) noexcept;

template <typename T>
constexpr bool is_alignment_power_of_2(T value) noexcept
{
    return value > 0
        && (value & (value - 1)) == 0;
}


} // namespace detail

} // namespace sysmakeshift


#endif // INCLUDED_SYSMAKESHIFT_DETAIL_MEMORY_HPP_
