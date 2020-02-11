
#include <new>          // for operator new, bad_alloc
#include <cerrno>
#include <cstddef>      // for size_t, align_val_t
#include <algorithm>    // for max()
#include <system_error>

#ifdef _WIN32
# include <Windows.h>
#else
// assume POSIX
# include <sys/mman.h> // for mmap(), munmap(), madvise()
#endif

#include <sysmakeshift/new.hpp>    // for hardware_large_page_size(), hardware_page_size(), hardware_cache_line_size()
#include <sysmakeshift/memory.hpp>

#include <sysmakeshift/detail/arithmetic.hpp> // for try_ceili()
#include <sysmakeshift/detail/errors.hpp>

namespace sysmakeshift {

namespace detail {


void*
aligned_alloc(std::size_t size, std::size_t alignment)
{
    return ::operator new(size, std::align_val_t(alignment));
}
void
aligned_free(void* data, std::size_t size, std::size_t alignment) noexcept
{
    return ::operator delete(data, size, std::align_val_t(alignment));
}

void*
large_page_alloc(std::size_t size)
{
#if defined(__linux__)
    if (hardware_large_page_size() == 0)
    {
        throw std::system_error(std::make_error_code(std::errc::not_supported));
    }
    void* data = ::mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    detail::posix_assert(data != nullptr);
    int ec = ::madvise(data, size, MADV_HUGEPAGE);
    if (ec != 0)
    {
        ec = errno;
        ::munmap(data, size);
        detail::posix_raise(ec);
    }
    return data;
#elif defined(_WIN32)
    // TODO: should we do anything about the privileges here? (cf. https://docs.microsoft.com/en-us/windows/win32/memory/large-page-support, https://stackoverflow.com/a/42380052)
    std::size_t largePageSize = ::GetLargePageMinimum();
    if (largePageSize == 0)
    {
        throw std::system_error(std::make_error_code(std::errc::not_supported));
    }
    auto fullSizeR = detail::try_ceili(size, largePageSize);
    if (fullSizeR.ec != std::errc{ })
    {
        throw std::bad_alloc{ };
    }
    void* data = ::VirtualAlloc(NULL, fullSizeR.value, MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);
    detail::win32_assert(data != nullptr);
    return data;
#else
    throw std::system_error(std::make_error_code(std::errc::not_supported));
#endif
}
void
large_page_free(void* data, std::size_t size) noexcept
{
#if defined(__linux__)
    detail::posix_assert(::munmap(data, size) == 0);
#elif defined(_WIN32)
    (void) size;
    detail::win32_assert(::VirtualFree(data, 0, MEM_RELEASE));
#else
    throw std::system_error(std::make_error_code(std::errc::not_supported));
#endif
}

void*
page_alloc(std::size_t size)
{
#if defined(_WIN32)
    void* data = ::VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    detail::win32_assert(data != nullptr);
    return data;
#else // assume POSIX
    void* data = ::mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    detail::posix_assert(data != nullptr);
# if defined(__linux__)
    int ec = ::madvise(data, size, MADV_NOHUGEPAGE);
    if (ec != 0)
    {
        ec = errno;
        ::munmap(data, size);
        detail::posix_raise(ec);
    }
# endif // defined(__linux__)
    return data;
#endif
}
void
page_free(void* data, std::size_t size) noexcept
{
#if defined(_WIN32)
    (void) size;
    detail::win32_assert(::VirtualFree(data, 0, MEM_RELEASE));
#else // assume POSIX
    detail::posix_assert(::munmap(data, size) == 0);
#endif
}

static std::size_t
floor_2p(std::size_t x)
{
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> (sizeof(std::size_t) > 4 ? 32 : 0);
    x |= x >> (sizeof(std::size_t) > 8 ? 64 : 0); // Sure, we can support 128-bit size_t, but, really?
    return (x + 1) >> 1; // assumes that x < powi(2, sizeof(std::size_t) * 8 - 1), which is given because the most significant bits have special meaning and have been masked out
}

std::size_t
lookup_special_alignments(std::size_t a) noexcept
{
    if ((a & large_page_alignment) != 0)
    {
            // This is without effect if `hardware_large_page_size()` returns 0, i.e. if large pages are not supported.
        a |= hardware_large_page_size();
    }
    if ((a & page_alignment) != 0)
    {
        a |= hardware_page_size();
    }
    if ((a & cache_line_alignment) != 0)
    {
        a |= hardware_cache_line_size();
    }

        // Mask out flags with special meaning.
    a &= ~(large_page_alignment | page_alignment | cache_line_alignment);

    return a;
}
std::size_t
alignment_in_bytes(std::size_t a) noexcept
{
    return std::max(std::size_t(1), floor_2p(lookup_special_alignments(a)));
}


} // namespace detail

} // namespace sysmakeshift
