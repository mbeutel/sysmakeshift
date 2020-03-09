﻿
#include <atomic>
#include <cstdio>
#include <cstdlib>   // for sscanf()
#include <cstddef>   // for size_t
#include <cstring>   // for strcmp()
#include <string>
#include <fstream>
#include <iostream>
#include <stdexcept> // for runtime_error

#include <gsl-lite/gsl-lite.hpp> // for narrow<>()

#include <sysmakeshift/new.hpp>
#include <sysmakeshift/memory.hpp>

#include <sysmakeshift/detail/errors.hpp>
#include <sysmakeshift/detail/lazy-init.hpp>

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


static std::atomic<std::size_t>
hardware_large_page_size_value = std::size_t(-1);

std::size_t
hardware_large_page_size(void) noexcept
{
    static constexpr auto initFunc = []
    {
#if defined(_WIN32)
        return GetLargePageMinimum();
#elif defined(__linux__)
            // I can't believe that parsing /proc/meminfo is the accepted way to query the default hugepage size and other
            // parameters.
        auto f = std::ifstream("/proc/meminfo");
        if (!f) throw std::runtime_error("cannot open /proc/meminfo"); // something is really wrong if we cannot open that file
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
    };
    return detail::lazy_init(hardware_large_page_size_value, std::size_t(-1), initFunc);
}


static std::atomic<std::size_t>
hardware_page_size_value = std::size_t(-1);

std::size_t
hardware_page_size(void) noexcept
{
    static constexpr auto initFunc = []
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
    };
    return detail::lazy_init(hardware_page_size_value, std::size_t(-1), initFunc);
}

#if !defined(_WIN32) // `hardware_cache_line_size()` for Windows is defined in cpuinfo.cpp
static std::atomic<std::size_t>
hardware_cache_line_size_value = std::size_t(-1);

std::size_t
hardware_cache_line_size(void) noexcept
{
    static constexpr auto initFunc = []
    {
# if defined(__linux__)
        long result = 0;
#  ifdef _SC_LEVEL1_DCACHE_LINESIZE
        result = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
        if (result > 0) return gsl::narrow<std::size_t>(result);
#  endif // _SC_LEVEL1_DCACHE_LINESIZE
        FILE* f = std::fopen("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size", "r");
        if (f == nullptr) throw std::runtime_error("cannot open /sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size");
        int nf = std::fscanf(f, "%ld", &result);
        std::fclose(f);
        if (nf != 1 || result == 0) throw std::runtime_error("error parsing /sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size");
        return gsl::narrow<std::size_t>(result);
# elif defined(__APPLE__)
        std::size_t result = 0;
        std::size_t nbResult = sizeof result;
        int ec = sysctlbyname("hw.cachelinesize", &result, &nbResult, 0, 0);
        if (ec != 0) throw std::runtime_error("cannot query hw.cachelinesize");
        return result;
# else
#  error Unsupported operating system.
# endif
    };
    return detail::lazy_init(hardware_cache_line_size_value, std::size_t(-1), initFunc);
}
#endif // !defined(_WIN32)


} // namespace sysmakeshift
