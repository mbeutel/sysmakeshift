
#ifndef INCLUDED_SYSMAKESHIFT_DETAIL_THREAD_SQUAD_HPP_
#define INCLUDED_SYSMAKESHIFT_DETAIL_THREAD_SQUAD_HPP_


#include <memory> // for unique_ptr<>


namespace sysmakeshift {

namespace detail {


struct thread_squad_impl_base
{
    int numThreads;
};
struct thread_squad_impl;

class thread_squad_data;

struct thread_squad_impl_deleter
{
    void
    operator ()(thread_squad_impl_base* base);
};

using thread_squad_handle = std::unique_ptr<detail::thread_squad_impl_base, thread_squad_impl_deleter>;


} // namespace detail

} // namespace sysmakeshift


#endif // INCLUDED_SYSMAKESHIFT_DETAIL_THREAD_SQUAD_HPP_
