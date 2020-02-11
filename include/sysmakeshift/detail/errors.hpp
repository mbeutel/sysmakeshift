
#ifndef INCLUDED_SYSMAKESHIFT_DETAIL_ERRORS_HPP_
#define INCLUDED_SYSMAKESHIFT_DETAIL_ERRORS_HPP_


#include <cstdint>      // for uint32_t
#include <system_error>


namespace sysmakeshift {

namespace detail {


[[noreturn]] void
posix_raise(int errorCode);

[[noreturn]] void
posix_raise_last_error(void);

inline void
posix_check(int errorCode)
{
    if (errorCode != 0) detail::posix_raise(errorCode);
}

inline void
posix_assert(bool success)
{
    if (!success) detail::posix_raise_last_error();
}

#ifdef _WIN32
[[noreturn]] void
win32_raise(std::uint32_t errorCode);

[[noreturn]] void
win32_raise_last_error(void);

inline void
win32_check(std::uint32_t errorCode)
{
    if (errorCode != 0) detail::win32_raise(errorCode);
}
inline void
win32_assert(int success)
{
    if (!success) detail::win32_raise_last_error();
}
#endif // _WIN32


} // namespace detail

} // namespace sysmakeshift


#endif // INCLUDED_SYSMAKESHIFT_DETAIL_ERRORS_HPP_
