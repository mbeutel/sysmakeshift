
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <stdexcept> // for runtime_error
#include <algorithm> // for sort(), is_sorted(), adjacent_find()

#include <gsl-lite/gsl-lite.hpp> // for dim, ssize(), gsl_ExpectsAudit(), narrow<>()

#include <sysmakeshift/thread.hpp>

#include <sysmakeshift/detail/lazy-init.hpp>

#if defined(_WIN32)
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


namespace gsl = ::gsl_lite;


#if defined(__linux__)
namespace detail {


struct physical_core_id
{
    int core_id;
    int physical_id;

    friend bool operator ==(physical_core_id const& lhs, physical_core_id const& rhs)
    {
        return lhs.core_id == rhs.core_id && lhs.physical_id == rhs.physical_id;
    }
    friend bool operator <(physical_core_id const& lhs, physical_core_id const& rhs)
    {
        return lhs.core_id < rhs.core_id
            || (lhs.core_id == rhs.core_id && lhs.physical_id < rhs.physical_id);
    }
};

template <typename InIt>
gsl::dim
count_duplicates(InIt first, InIt last)
{
    gsl_ExpectsAudit(std::is_sorted(first, last));

    gsl::dim count = 0;
    for (;;)
    {
        first = std::adjacent_find(first, last);
        if (first == last) return count;
        ++first;
        ++count;
    }
}


} // namespace detail
#endif // defined(__linux__)


#if !defined(_WIN32) // `physical_concurrency()` for Windows is defined in cpuinfo.cpp
static std::atomic<unsigned>
physical_concurrency_value = unsigned(-1);

gsl_NODISCARD unsigned
physical_concurrency(void) noexcept
{
    static constexpr auto initFunc = []
    {
# if defined(__linux__)
            // I can't believe that parsing /proc/cpuinfo is the accepted way to query the number of physical cores.
        auto f = std::ifstream("/proc/cpuinfo");
        if (!f) throw std::runtime_error("cannot open /proc/cpuinfo"); // something is really wrong if we cannot open that file
        auto ids = std::vector<detail::physical_core_id>{ };
        auto line = std::string{ };
        int lastCoreId = -1;
        int lastPhysicalId = -1;
        while (std::getline(f, line))
        {
            int nFields;
            int id;
            nFields = std::sscanf(line.c_str(), "physical id : %d", &id);
            if (nFields == 1)
            {
                if (lastPhysicalId != -1) throw std::runtime_error("error parsing /proc/cpuinfo: missing \"core id\" value");
                lastPhysicalId = id;
            }
            else
            {
                nFields = std::sscanf(line.c_str(), "core id : %d", &id);
                if (nFields == 1)
                {
                    if (lastCoreId != -1) throw std::runtime_error("error parsing /proc/cpuinfo: missing \"physical id\" value");
                    lastCoreId = id;
                }
            }
            if (lastCoreId != -1 && lastPhysicalId != -1)
            {
                ids.push_back(detail::physical_core_id{ lastCoreId, lastPhysicalId });
                lastCoreId = -1;
                lastPhysicalId = -1;
            }
        }
        f.close();

        std::sort(ids.begin(), ids.end());
        auto numUnique = gsl::ssize(ids) - detail::count_duplicates(ids.begin(), ids.end());
        return gsl::narrow<unsigned>(numUnique);
# elif defined(__APPLE__)
        int result = 0;
        std::size_t nbResult = sizeof result;
        int ec = sysctlbyname("hw.physicalcpu", &result, &nbResult, 0, 0);
        if (ec != 0) throw std::runtime_error("cannot query hw.physicalcpu");
        return gsl::narrow<unsigned>(result);
# else
#  error Unsupported operating system.
# endif
    };
    return detail::lazy_init(physical_concurrency_value, unsigned(-1), initFunc);
}
#endif // !defined(_WIN32)


} // namespace sysmakeshift
