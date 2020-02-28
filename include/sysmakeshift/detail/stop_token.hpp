
#ifndef INCLUDED_SYSMAKESHIFT_DETAIL_STOP_TOKEN_HPP_
#define INCLUDED_SYSMAKESHIFT_DETAIL_STOP_TOKEN_HPP_


#include <cstddef> // for size_t
#include <memory>  // for shared_ptr<>
#include <atomic>


namespace sysmakeshift {

namespace detail {


struct stop_token_state
{
    std::atomic<std::size_t> numSources = ATOMIC_VAR_INIT(0);
    std::atomic<int> stopped = ATOMIC_VAR_INIT(0);
};


} // namespace detail

} // namespace sysmakeshift


#endif // INCLUDED_SYSMAKESHIFT_DETAIL_STOP_TOKEN_HPP_
