
#include <cstddef>   // for ptrdiff_t
#include <memory>    // for unique_ptr<>
#include <stdexcept> // for logic_error()

#if defined(_WIN32)
# ifndef NOMINMAX
#  define NOMINMAX
# endif
# include <Windows.h>
# include <Memoryapi.h>
#endif

#include <sysmakeshift/detail/errors.hpp>
#include <sysmakeshift/detail/cpuinfo.hpp>


namespace sysmakeshift {

namespace detail {


#if defined(_WIN32)
win32_cpu_info const&
get_win32_cpu_info(void) noexcept
{
    static win32_cpu_info result = []
    {
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
                ++lresult.physical_concurrency;
            }
            if (pSlpi[i].Relationship == RelationCache && pSlpi[i].Cache.Level == 1 && (pSlpi[i].Cache.Type == CacheData || pSlpi[i].Cache.Type == CacheUnified))
            {
                // We don't bother further checking the CPU affinity group here because we don't expect to encounter a system with heterogeneous cache line sizes.

                if (lresult.cache_line_size == 0)
                {
                    lresult.cache_line_size = pSlpi[i].Cache.LineSize;
                }
                else if (lresult.cache_line_size != pSlpi[i].Cache.LineSize)
                {
                    throw std::logic_error("GetLogicalProcessorInformation() reports different L1 cache line sizes for different cores"); // ...and we cannot handle that
                }
            }
        }
        if (lresult.cache_line_size == 0)
        {
            throw std::logic_error("GetLogicalProcessorInformation() did not report any L1 cache info");
        }
        return lresult;
    }();
    return result;
}
#endif // defined(_WIN32)


} // namespace detail

} // namespace sysmakeshift
