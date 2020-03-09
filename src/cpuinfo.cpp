
#include <atomic>
#include <cstddef>   // for ptrdiff_t
#include <memory>    // for unique_ptr<>
#include <stdexcept> // for runtime_error

#if defined(_WIN32)
# ifndef NOMINMAX
#  define NOMINMAX
# endif
# include <Windows.h>
# include <Memoryapi.h>
#endif

#include <sysmakeshift/detail/errors.hpp>


namespace sysmakeshift {

namespace detail {


#if defined(_WIN32)
struct win32_cpu_info
{
    std::atomic<std::size_t> cache_line_size;
    std::atomic<unsigned> physical_concurrency;
};

static win32_cpu_info win32_cpu_info_value{ };

void
init_win32_cpu_info(void) noexcept
{
    auto initFunc = []
    {
        std::size_t newCacheLineSize = 0;
        unsigned newPhysicalConcurrency = 0;
        auto lresult = win32_cpu_info{ };
        std::unique_ptr<SYSTEM_LOGICAL_PROCESSOR_INFORMATION[]> dynSlpi;
        SYSTEM_LOGICAL_PROCESSOR_INFORMATION* pSlpi = nullptr;
        DWORD nbSlpi = 0;
        BOOL success = GetLogicalProcessorInformation(pSlpi, &nbSlpi);
        if (!success && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            dynSlpi = std::make_unique<SYSTEM_LOGICAL_PROCESSOR_INFORMATION[]>((nbSlpi + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) - 1) / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
            pSlpi = dynSlpi.get();
            success = GetLogicalProcessorInformation(pSlpi, &nbSlpi);
        }
        detail::win32_assert(success);
        for (std::ptrdiff_t i = 0, n = nbSlpi / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); i != n; ++i)
        {
            if (pSlpi[i].Relationship == RelationProcessorCore)
            {
                ++newPhysicalConcurrency;
            }
            if (pSlpi[i].Relationship == RelationCache && pSlpi[i].Cache.Level == 1 && (pSlpi[i].Cache.Type == CacheData || pSlpi[i].Cache.Type == CacheUnified))
            {
                if (newCacheLineSize == 0)
                {
                    newCacheLineSize = pSlpi[i].Cache.LineSize;
                }
                else if (newCacheLineSize != pSlpi[i].Cache.LineSize)
                {
                    throw std::runtime_error("GetLogicalProcessorInformation() reports different L1 cache line sizes for different cores"); // ...and we cannot handle that
                }
            }
        }
        if (newCacheLineSize == 0)
        {
            throw std::runtime_error("GetLogicalProcessorInformation() did not report any L1 cache info");
        }
        if (newPhysicalConcurrency == 0)
        {
            throw std::runtime_error("GetLogicalProcessorInformation() did not report any processor cores");
        }

        win32_cpu_info_value.cache_line_size.store(newCacheLineSize, std::memory_order_relaxed);
        win32_cpu_info_value.physical_concurrency.store(newPhysicalConcurrency, std::memory_order_relaxed);

            // A release fence would be sufficient here, but we use sequential consistency by default to have other threads see
            // the results of our hard work as soon as possible.
        std::atomic_thread_fence(std::memory_order_seq_cst);
    };

    auto cacheLineSize = win32_cpu_info_value.cache_line_size.load(std::memory_order_relaxed);
    if (cacheLineSize == 0)
    {
        initFunc();
    }
}
#endif // defined(_WIN32)


} // namespace detail


std::size_t
hardware_cache_line_size(void) noexcept
{
    auto cacheLineSize = detail::win32_cpu_info_value.cache_line_size.load(std::memory_order_relaxed);
    if (cacheLineSize == 0)
    {
        detail::init_win32_cpu_info();
        cacheLineSize = detail::win32_cpu_info_value.cache_line_size.load(std::memory_order_relaxed);
    }
    return cacheLineSize;
}

unsigned
physical_concurrency(void) noexcept
{
    auto physicalConcurrency = detail::win32_cpu_info_value.physical_concurrency.load(std::memory_order_relaxed);
    if (physicalConcurrency == 0)
    {
        detail::init_win32_cpu_info();
        physicalConcurrency = detail::win32_cpu_info_value.physical_concurrency.load(std::memory_order_relaxed);
    }
    return physicalConcurrency;
}


} // namespace sysmakeshift
