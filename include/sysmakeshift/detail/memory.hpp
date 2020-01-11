
#ifndef INCLUDED_SYSMAKESHIFT_DETAIL_MEMORY_HPP_
#define INCLUDED_SYSMAKESHIFT_DETAIL_MEMORY_HPP_


#include <limits>
#include <cstddef>     // for size_t, ptrdiff_t
#include <utility>     // for forward<>()
#include <type_traits> // for integral_constant<>, declval<>()
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

template <typename A, typename = void> struct has_member_provides_static_alignment : std::false_type { };
template <typename A> struct has_member_provides_static_alignment<A, gsl::void_t<decltype(A::provides_static_alignment(std::declval<alignment>()))>> : std::is_convertible<decltype(A::provides_static_alignment(std::declval<alignment>())), bool> { };


template <typename U>
constexpr int bit_scan_reverse(U mask)
{
    int result = 0;
    while (mask >>= 1)
    {
        ++result;
    }
    return result;
}

constexpr std::size_t special_alignments = std::numeric_limits<std::size_t>::max() & ~(std::numeric_limits<std::size_t>::max() >> 3); // = std::size_t(alignment::large_page | alignment::page | alignment::cache_line)

constexpr bool provides_static_alignment(alignment alignmentProvided, alignment alignmentRequested) noexcept
{
    std::size_t highestAlignmentProvided = std::size_t(1) << std::size_t(detail::bit_scan_reverse(std::size_t(alignmentProvided) & ~special_alignments)); // only most significant bit
    std::size_t allAlignmentsProvided = std::size_t(alignmentProvided) | (highestAlignmentProvided - 1);
    return (std::size_t(alignmentRequested) & allAlignmentsProvided) == std::size_t(alignmentRequested);
}


alignment lookup_special_alignments(alignment a) noexcept;

inline bool provides_dynamic_alignment(alignment alignmentProvided, alignment alignmentRequested) noexcept
{
    alignmentProvided = detail::lookup_special_alignments(alignmentProvided);
    alignmentRequested = detail::lookup_special_alignments(alignmentRequested);
    std::size_t highestAlignmentProvided = std::size_t(1) << std::size_t(detail::bit_scan_reverse(std::size_t(alignmentProvided))); // only most significant bit
    std::size_t allAlignmentsProvided = std::size_t(alignmentProvided) | (highestAlignmentProvided - 1);
    return (std::size_t(alignmentRequested) & allAlignmentsProvided) == std::size_t(alignmentRequested);
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


template <typename T, alignment Alignment, typename A, bool NeedAlignment>
class aligned_allocator_base;
template <typename T, alignment Alignment, typename A>
class aligned_allocator_base<T, Alignment, A, true> : public A
{
public:
    using A::A;

    gsl_NODISCARD static constexpr bool provides_static_alignment(alignment a) noexcept
    {
        return detail::provides_static_alignment(Alignment, a);
    }

    gsl_NODISCARD T* allocate(std::size_t n)
    {
        std::size_t a = detail::alignment_in_bytes(alignment(std::size_t(Alignment) | alignof(T)));
        if (n >= std::numeric_limits<std::size_t>::max() / sizeof(T)) throw std::bad_alloc{ }; // overflow
        std::size_t nbData = n * sizeof(T);
        std::size_t nbAlloc = nbData + a + sizeof(void*) - 1;
        if (nbAlloc < nbData) throw std::bad_alloc{ }; // overflow

        using ByteAllocator = typename std::allocator_traits<A>::template rebind_alloc<char>;
        auto byteAllocator = ByteAllocator(*this); // may not throw
        void* mem = std::allocator_traits<ByteAllocator>::allocate(byteAllocator, nbAlloc);
        void* alignedMem = mem;
        void* alignResult = std::align(a, nbData + sizeof(void*), alignedMem, nbAlloc);
        gsl_Expects(alignResult != nullptr && nbAlloc >= nbData + sizeof(void*)); // should not happen

            // Store pointer to actual allocation at end of buffer. Use `memcpy()` so we don't have to worry about alignment.
        std::memcpy(static_cast<char*>(alignResult) + nbData, &mem, sizeof(void*));

        return static_cast<T*>(alignResult);
    }
    void deallocate(T* ptr, std::size_t n) noexcept
    {
        std::size_t a = detail::alignment_in_bytes(alignment(std::size_t(Alignment) | alignof(T)));
        std::size_t nbData = n * sizeof(T); // cannot overflow due to preceding check in allocate()
        std::size_t nbAlloc = nbData + a + sizeof(void*) - 1; // cannot overflow due to preceding check in allocate()

            // Retrieve pointer to actual allocation from end of buffer. Use `memcpy()` so we don't have to worry about alignment.
        void* mem;
        std::memcpy(&mem, reinterpret_cast<char*>(ptr) + nbData, sizeof(void*));

        using ByteAllocator = typename std::allocator_traits<A>::template rebind_alloc<char>;
        auto byteAllocator = ByteAllocator(*this); // may not throw
        std::allocator_traits<ByteAllocator>::deallocate(byteAllocator, static_cast<char*>(mem), nbAlloc);
    }
};
template <typename T, alignment Alignment, typename A>
class aligned_allocator_base<T, Alignment, A, false> : public A
{
public:
    using A::A;
};


} // namespace detail

} // namespace sysmakeshift


#endif // INCLUDED_SYSMAKESHIFT_DETAIL_MEMORY_HPP_
