
#include <cstdlib>   // for sscanf()
#include <cstddef>   // for size_t
#include <cstring>   // for strcmp()
#include <string>
#include <fstream>
#include <iostream>
#include <stdexcept> // for logic_error

#include <gsl-lite/gsl-lite.hpp> // for gsl_Expects(), narrow_cast<>()

#include <sysmakeshift/new.hpp>
#include <sysmakeshift/memory.hpp>

#include <sysmakeshift/detail/errors.hpp>
#include <sysmakeshift/detail/cpuinfo.hpp>

#if defined(_WIN32)
# ifndef NOMINMAX
#  define NOMINMAX
# endif
# include <Windows.h>
# include <Memoryapi.h>
#elif defined(__linux__)
# include <unistd.h>
# include <stdio.h>
#elif defined(__APPLE__)
# include <unistd.h>
# include <sys/types.h>
# include <sys/sysctl.h>
#else
# error Unsupported operating system.
#endif


namespace sysmakeshift {


std::size_t
hardware_large_page_size(void) noexcept
{
    static std::size_t result = []
    {
#if defined(_WIN32)
        return GetLargePageMinimum();
#elif defined(__linux__)
            // I can't believe that parsing /proc/meminfo is the accepted way to query the default hugepage size and other
            // parameters.
        auto f = std::ifstream("/proc/meminfo");
        if (!f) throw std::logic_error("cannot open /proc/meminfo"); // something is really wrong if we cannot open that file
        auto line = std::string{ };
        while (std::getline(f, line))
        {
            long hugePageSize = 0;
            char unit[16+1];
            int nFields = std::sscanf(line.c_str(), "Hugepagesize : %ld %16s", &hugePageSize, unit);
            if (nFields == 2)
            {
                    // This is the only unit the kernel currently emits, so I don't bother with speculative M[i]B etc.
                if (std::strcmp(unit, "kB") == 0)
                {
                    hugePageSize *= 1024l;
                    return gsl::narrow<std::size_t>(hugePageSize);
                }
                else throw std::runtime_error("error parsing /proc/meminfo: unrecognized unit '" + std::string(unit) + "'");
            }
        }
        return std::size_t(0); // no "Hugepagesize" entry found
#elif defined(__APPLE__)
        return 0; // MacOS does support huge pages ("superpages") but we currently didn't write any code to support them
#else
# error Unsupported operating system.
#endif
    }();
    return result;
}

std::size_t
hardware_page_size(void) noexcept
{
    static std::size_t result = []
    {
#if defined(_WIN32)
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        return sysInfo.dwPageSize;
#elif defined(__linux__)
        return sysconf(_SC_PAGESIZE);
#elif defined(__APPLE__)
        return getpagesize();
#else
# error Unsupported operating system.
#endif
    }();
    return result;
}

std::size_t
hardware_cache_line_size(void) noexcept
{
#if defined(_WIN32)
    return detail::get_win32_cpu_info().cache_line_size;
#else // ^^^ defined(_WIN32) ^^^ / vvv !defined(_WIN32) vvv
    static std::size_t result = []
    {
# if defined(__linux__)
        long result = 0;
#  ifdef _SC_LEVEL1_DCACHE_LINESIZE
        result = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
        if (result > 0) return gsl::narrow_cast<std::size_t>(result);
#  endif // _SC_LEVEL1_DCACHE_LINESIZE
        FILE* f = std::fopen("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size", "r");
        gsl_Expects(f != nullptr);
        int nf = std::fscanf(f, "%ld", &result);
        std::fclose(f);
        gsl_Expects(nf == 1 && result != 0);
        return gsl::narrow_cast<std::size_t>(result);
# elif defined(__APPLE__)
        std::size_t result = 0;
        std::size_t nbResult = sizeof result;
        int ec = sysctlbyname("hw.cachelinesize", &result, &nbResult, 0, 0);
        gsl_Expects(ec == 0);
        return result;
# else
#  error Unsupported operating system.
# endif
    }();
    return result;
#endif // defined(_WIN32)
}


} // namespace sysmakeshift
