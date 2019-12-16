
#ifndef INCLUDED_SYSMAKESHIFT_DETAIL_THREAD_HPP_
#define INCLUDED_SYSMAKESHIFT_DETAIL_THREAD_HPP_


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



} // namespace detail

} // namespace sysmakeshift


#endif // INCLUDED_SYSMAKESHIFT_DETAIL_THREAD_HPP_
