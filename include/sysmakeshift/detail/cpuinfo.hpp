
#ifndef INCLUDED_SYSMAKESHIFT_DETAIL_CPUINFO_HPP_
#define INCLUDED_SYSMAKESHIFT_DETAIL_CPUINFO_HPP_


#include <cstddef> // for size_t


namespace sysmakeshift {

namespace detail {


#if defined(_WIN32)
struct win32_cpu_info
{
    std::size_t cache_line_size;
    unsigned physical_concurrency;
};

win32_cpu_info const&
get_win32_cpu_info(void) noexcept;
#endif // defined(_WIN32)


} // namespace detail

} // namespace sysmakeshift


#endif // INCLUDED_SYSMAKESHIFT_DETAIL_CPUINFO_HPP_
