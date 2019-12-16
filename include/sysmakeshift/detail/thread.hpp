
#ifndef INCLUDED_SYSMAKESHIFT_DETAIL_THREAD_HPP_
#define INCLUDED_SYSMAKESHIFT_DETAIL_THREAD_HPP_


#include <memory> // for unique_ptr<>

#include <gsl/gsl-lite.hpp> // for not_null<>


namespace sysmakeshift
{

namespace detail
{


struct thread_pool_thread_data;

struct thread_pool_impl_base
{
    int numThreads_;
};
struct thread_pool_impl;

struct thread_pool_impl_deleter
{
    void operator ()(thread_pool_impl_base* impl);
};

using thread_pool_handle = std::unique_ptr<detail::thread_pool_impl_base, thread_pool_impl_deleter>;


} // namespace detail

} // namespace sysmakeshift


#endif // INCLUDED_SYSMAKESHIFT_DETAIL_THREAD_HPP_
