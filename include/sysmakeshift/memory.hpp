
#ifndef INCLUDED_SYSMAKESHIFT_MEMORY_HPP_
#define INCLUDED_SYSMAKESHIFT_MEMORY_HPP_


#include <new>         // for align_val_t, bad_alloc
#include <limits>
#include <cstddef>     // for size_t, ptrdiff_t, max_align_t
#include <cstdlib>     // for calloc(), free()
#include <cstring>     // for memcpy()
#include <memory>      // for unique_ptr<>, allocator<>, allocator_traits<>, align()
#include <utility>     // for forward<>()
#include <type_traits> // for is_nothrow_default_constructible<>, enable_if<>, is_same<>, remove_cv<>

#include <gsl-lite/gsl-lite.hpp> // for gsl_Expects(), gsl_NODISCARD

#include <sysmakeshift/detail/memory.hpp>
#include <sysmakeshift/detail/type_traits.hpp> // for can_instantiate<>


namespace sysmakeshift {


namespace gsl = ::gsl_lite;


    //
    // Allocator adaptor that interposes construct() calls to convert value initialization into default initialization.
    //
template <typename T, typename A = std::allocator<T>>
class default_init_allocator : public A
{
    // Implementation taken from https://stackoverflow.com/a/21028912.

public:
    using A::A;

    template <typename U>
    struct rebind
    {
        using other = default_init_allocator<U, typename std::allocator_traits<A>::template rebind_alloc<U>>;
    };

    template <typename U>
    void
    construct(U* ptr)
    noexcept(std::is_nothrow_default_constructible<U>::value)
    {
        ::new(static_cast<void*>(ptr)) U;
    }
    template <typename U, typename...ArgsT>
    void
    construct(U* ptr, ArgsT&&... args)
    {
        std::allocator_traits<A>::construct(static_cast<A&>(*this), ptr, std::forward<ArgsT>(args)...);
    }
};


    //
    // Allocator that always returns zero-initialized memory.
    //
template <typename T>
class zero_init_allocator
{
public:
    using value_type = T;

    constexpr zero_init_allocator(void) noexcept
    {
    }
    template <typename U>
    constexpr zero_init_allocator(zero_init_allocator<U> const&) noexcept
    {
    }

    gsl_NODISCARD value_type*
    allocate(std::size_t n)
    {
        auto mem = std::calloc(n, sizeof(value_type));
        if (mem == nullptr) throw std::bad_alloc{ };
        return static_cast<value_type*>(mem);
    }

    void
    deallocate(value_type* p, std::size_t) noexcept
    {
        std::free(p);
    }
};

template <typename T, typename U>
gsl_NODISCARD bool
operator ==(zero_init_allocator<T> const&, zero_init_allocator<U> const&) noexcept
{
    return true;
}
template <typename T, typename U>
gsl_NODISCARD bool
operator !=(zero_init_allocator<T> const& x, zero_init_allocator<U> const& y) noexcept
{
    return !(x == y);
}


    //
    // Special alignment value representing the alignment of large pages.
    //
constexpr std::size_t large_page_alignment =  std::numeric_limits<std::size_t>::max() & ~(std::numeric_limits<std::size_t>::max() >> 1);

    //
    // Special alignment value representing the alignment of pages.
    //
constexpr std::size_t page_alignment       = (std::numeric_limits<std::size_t>::max() & ~(std::numeric_limits<std::size_t>::max() >> 1)) >> 1;

    //
    // Special alignment value representing the alignment of cache lines.
    //
constexpr std::size_t cache_line_alignment = (std::numeric_limits<std::size_t>::max() & ~(std::numeric_limits<std::size_t>::max() >> 1)) >> 2;


    //
    // Computes whether the provided alignment satisfies the requested alignment.
    //ᅟ
    // The alignments corresponding to the special alignment values `large_page_alignment`, `page_alignment`, and `cache_line_alignment` are not known until runtime,
    // hence to satisfy a requested special alignment it must be provided explicitly by the provided alignment.
    //
constexpr bool
provides_static_alignment(std::size_t alignmentProvided, std::size_t alignmentRequested) noexcept
{
    return detail::provides_static_alignment(alignmentProvided, alignmentRequested);
}

    //
    // Computes whether the provided alignment satisfies the requested alignment.
    //ᅟ
    // Looks up the alignments corresponding to the special alignment values `large_page_alignment`, `page_alignment`, and `cache_line_alignment`.
    //
inline bool
provides_dynamic_alignment(std::size_t alignmentProvided, std::size_t alignmentRequested) noexcept
{
    return detail::provides_dynamic_alignment(alignmentProvided, alignmentRequested);
}


    //
    // Provides member functions that query alignment-related properties of allocators.
    //
template <typename A>
struct aligned_allocator_traits
{
private:
    static constexpr bool
    provides_static_alignment_impl(std::size_t a, std::false_type /*hasMember*/)
    {
        return sysmakeshift::provides_static_alignment(alignof(std::max_align_t), a);
    }
    static constexpr bool
    provides_static_alignment_impl(std::size_t a, std::true_type /*hasMember*/)
    {
        return A::provides_static_alignment(a);
    }

public:
    gsl_NODISCARD static constexpr bool
    provides_static_alignment(std::size_t a) noexcept
    {
        return provides_static_alignment_impl(a, detail::has_member_provides_static_alignment<A>{ });
    }
};


    //
    // Allocator that aligns memory allocations for the given alignment using the default allocator, i.e. global `operator new()` with `std::align_val_t`.
    //ᅟ
    // Supports special alignment values such as `cache_line_alignment`.
    // Multiple alignment requirements can be combined using bitmask operations, e.g. `cache_line_alignment | alignof(T)`.
    //
template <typename T, std::size_t Alignment>
class aligned_allocator
{
public:
    using value_type = T;

    template <typename U>
    struct rebind
    {
        using other = aligned_allocator<U, Alignment>;
    };

    constexpr aligned_allocator(void) noexcept
    {
    }
    template <typename U>
    constexpr aligned_allocator(aligned_allocator<U, Alignment> const&) noexcept
    {
    }

    gsl_NODISCARD static constexpr bool
    provides_static_alignment(std::size_t a) noexcept
    {
        return sysmakeshift::provides_static_alignment(Alignment, a);
    }

    gsl_NODISCARD T*
    allocate(std::size_t n)
    {
        std::size_t a = detail::alignment_in_bytes(Alignment | alignof(T));
        if (n >= std::numeric_limits<std::size_t>::max() / sizeof(T)) throw std::bad_alloc{ }; // overflow
        std::size_t nbData = n * sizeof(T);
        return static_cast<T*>(detail::aligned_alloc(nbData, a));
    }
    void
    deallocate(T* ptr, std::size_t n) noexcept
    {
        std::size_t a = detail::alignment_in_bytes(Alignment | alignof(T));
        std::size_t nbData = n * sizeof(T); // cannot overflow due to preceding check in allocate()
        detail::aligned_free(ptr, nbData, a);
    }
};

template <typename T, typename U, std::size_t Alignment>
gsl_NODISCARD bool
operator ==(aligned_allocator<T, Alignment>, aligned_allocator<U, Alignment>) noexcept
{
    return true;
}
template <typename T, typename U, std::size_t Alignment>
gsl_NODISCARD bool
operator !=(aligned_allocator<T, Alignment> x, aligned_allocator<U, Alignment> y) noexcept
{
    return !(x == y);
}


    //
    // Allocator adaptor that aligns memory allocations for the given alignment.
    //ᅟ
    // Supports special alignment values such as `cache_line_alignment`.
    // Multiple alignment requirements can be combined using bitmask operations, e.g. `cache_line_alignment | alignof(T)`.
    //
template <typename T, std::size_t Alignment, typename A>
class aligned_allocator_adaptor : public detail::aligned_allocator_adaptor_base<T, Alignment, A, !aligned_allocator_traits<A>::provides_static_alignment(Alignment)>
{
    using base = detail::aligned_allocator_adaptor_base<T, Alignment, A, !aligned_allocator_traits<A>::provides_static_alignment(Alignment)>;

public:
    using base::base;

    template <typename U>
    struct rebind
    {
        using other = aligned_allocator_adaptor<U, Alignment, typename std::allocator_traits<A>::template rebind_alloc<U>>;
    };
};


    //
    // Deleter for `std::unique_ptr<>` which supports custom allocators.
    //ᅟ
    //ᅟ    auto p1 = allocate_unique<float>(MyAllocator<float>{ }, 3.14159f);
    //ᅟ    // returns `std::unique_ptr<float, allocator_deleter<float, MyAllocator<float>>>`
    //ᅟ    auto p2 = allocate_unique<float[]>(MyAllocator<float>{ }, 42);
    //ᅟ    // returns `std::unique_ptr<float[], allocator_deleter<float[], MyAllocator<float>>>`
    //ᅟ    auto p3 = allocate_unique<float[42]>(MyAllocator<float>{ });
    //ᅟ    // returns `std::unique_ptr<float[42], allocator_deleter<float[42], MyAllocator<float>>>`
    //
template <typename T, typename A>
class allocator_deleter : private A // for EBO
{
public:
    allocator_deleter(const A& _alloc)
        : A(_alloc)
    {
    }
    void
    operator ()(T* ptr) noexcept
    {
        std::allocator_traits<A>::destroy(*this, ptr);
        std::allocator_traits<A>::deallocate(*this, ptr, 1);
    }
};
template <typename T, typename A>
class allocator_deleter<T[], A> : private A // for EBO
{
private:
    std::size_t size_;

public:
    allocator_deleter(const A& _alloc, std::size_t _size)
        : A(_alloc), size_(_size)
    {
    }
    void
    operator ()(T ptr[]) noexcept
    {
        for (std::ptrdiff_t i = 0, n = std::ptrdiff_t(size_); i != n; ++i)
        {
            std::allocator_traits<A>::destroy(*this, &ptr[i]);
        }
        std::allocator_traits<A>::deallocate(*this, ptr, size_);
    }
};
template <typename T, std::ptrdiff_t N, typename A>
class allocator_deleter<T[N], A> : private A // for EBO
{
public:
    allocator_deleter(const A& _alloc)
        : A(_alloc)
    {
    }
    void
    operator ()(T ptr[]) noexcept
    {
        for (std::ptrdiff_t i = 0; i != N; ++i)
        {
            std::allocator_traits<A>::destroy(*this, &ptr[i]);
        }
        std::allocator_traits<A>::deallocate(*this, ptr, N);
    }
};


    //
    // Allocates an object of type `T` with the given allocator, constructs it with the given arguments and returns a `std::unique_ptr<>` to the object.
    //ᅟ
    //ᅟ    auto p = allocate_unique<float>(MyAllocator<float>{ }, 3.14159f);
    //ᅟ    // returns `std::unique_ptr<float, allocator_deleter<float, MyAllocator<float>>>`
    //
template <typename T, typename A, typename... ArgsT>
std::enable_if_t<!detail::can_instantiate_v<detail::remove_extent_only_t, T>, std::unique_ptr<T, allocator_deleter<T, A>>>
allocate_unique(A alloc, ArgsT&&... args)
{
    using NCVT = std::remove_cv_t<T>;
    static_assert(std::is_same<typename std::allocator_traits<A>::value_type, NCVT>::value, "allocator has mismatching value_type");

    NCVT* ptr = detail::allocate<NCVT>(alloc, std::forward<ArgsT>(args)...);
    return { ptr, { std::move(alloc) } };
}

    //
    // Allocates a fixed-size array of type `ArrayT` with the given allocator, default-constructs the elements and returns a `std::unique_ptr<>` to the array.
    //ᅟ
    //ᅟ    auto p = allocate_unique<float[42]>(MyAllocator<float>{ });
    //ᅟ    // returns `std::unique_ptr<float[42], allocator_deleter<float[42], MyAllocator<float>>>`
    //
template <typename ArrayT, typename A>
std::enable_if_t<detail::extent_only<ArrayT>::value != 0, std::unique_ptr<ArrayT, allocator_deleter<ArrayT, A>>>
allocate_unique(A alloc)
{
    using T = std::remove_cv_t<detail::remove_extent_only_t<ArrayT>>;
    static_assert(std::is_same<typename std::allocator_traits<A>::value_type, T>::value, "allocator has mismatching value_type");

    T* ptr = detail::allocate_array<T>(alloc, detail::extent_only<ArrayT>{ });
    return { ptr, { std::move(alloc) } };
}

    //
    // Allocates an array of type `ArrayT` with the given allocator, default-constructs the elements and returns a `std::unique_ptr<>` to the array.
    //ᅟ
    //ᅟ    auto p = allocate_unique<float[]>(MyAllocator<float>{ }, 42);
    //ᅟ    // returns `std::unique_ptr<float[], allocator_deleter<float[], MyAllocator<float>>>`
    //
template <typename ArrayT, typename A>
std::enable_if_t<detail::extent_only<ArrayT>::value == 0, std::unique_ptr<ArrayT, allocator_deleter<ArrayT, A>>>
allocate_unique(A alloc, std::size_t size)
{
    using T = std::remove_cv_t<detail::remove_extent_only_t<ArrayT>>;
    static_assert(std::is_same<typename std::allocator_traits<A>::value_type, T>::value, "allocator has mismatching value_type");

    T* ptr = detail::allocate_array<T>(alloc, size);
    return { ptr, { std::move(alloc), size } };
}


} // namespace sysmakeshift


#endif // INCLUDED_SYSMAKESHIFT_MEMORY_HPP_
