
#ifndef INCLUDED_SYSMAKESHIFT_STOP_TOKEN_HPP_
#define INCLUDED_SYSMAKESHIFT_STOP_TOKEN_HPP_


#include <memory> // for shared_ptr<>

#include <gsl-lite/gsl-lite.hpp> // for gsl_NODISCARD

#include <sysmakeshift/detail/type_traits.hpp> // for static_const<>


namespace sysmakeshift {


    //
    // Pass `nostopstate` to the `stop_source` constructor to obtain an empty stop source.
    //
struct nostopstate_t
{
    explicit nostopstate_t(void) = default;
};

    //
    // Pass `nostopstate` to the `stop_source` constructor to obtain an empty stop source.
    //
static constexpr nostopstate_t const& nostopstate = detail::static_const<nostopstate_t>;


class stop_source;


    //
    // Partial implementation of C++20 `std::stop_token`.
    //
class stop_token
{
    friend stop_source;

private:
    std::shared_ptr<detail::stop_token_state> state_;

    explicit stop_token(std::shared_ptr<detail::stop_token_state> _state)
        : state_(std::move(_state))
    {
    }

public:
    stop_token(void) noexcept
    {
    }

    stop_token(stop_token&&) noexcept = default;
    stop_token&
    operator =(stop_token&&) noexcept = default;

    stop_token(stop_token const&) noexcept = default;
    stop_token&
    operator =(stop_token const&) noexcept = default;

    gsl_NODISCARD bool
    stop_possible(void) const noexcept
    {
        return state_ != nullptr
            && state_.numSources.load(std::memory_order_acquire) != 0;
    }

    gsl_NODISCARD bool
    stop_requested(void) const noexcept
    {
        return state_ != nullptr
            && state_->stopped.load(std::memory_order_acquire) != 0;
    }
};


    //
    // Partial implementation of C++20 `std::stop_source` without support for callbacks or condition variables.
    //
class stop_source
{
private:
    std::shared_ptr<detail::stop_token_state> state_;

public:
    stop_source(void)
        : state_(std::make_shared<detail::stop_token_state>())
    {
    }

    explicit stop_source(nostopstate_t) noexcept
    {
    }

    stop_source(stop_source&& rhs) noexcept = default;
    stop_source&
    operator =(stop_source&& rhs) noexcept = default;

    stop_source(stop_source const& rhs) noexcept
        : state_(rhs.state_)
    {
        if (state_ != nullptr)
        {
            state_->numSources.fetch_add(1, std::memory_order_relaxed);
        }
    }
    stop_source&
    operator =(stop_source const& rhs) noexcept
    {
        if (this != &rhs)
        {
            if (state_ != nullptr)
            {
                state_->numSources.fetch_sub(1, std::memory_order_acq_rel);
            }
            state_ = rhs.state_;
            if (state_ != nullptr)
            {
                state_->numSources.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    ~stop_source(void)
    {
        if (state_ != nullptr)
        {
            state_->numSources.fetch_sub(1, std::memory_order_acq_rel);
        }
    }

    gsl_NODISCARD bool
    stop_possible(void) const noexcept
    {
        return state_ != nullptr;
    }

    gsl_NODISCARD bool
    stop_requested(void) const noexcept
    {
        return state_ != nullptr
            && state_->stopped.load(std::memory_order_acquire) != 0;
    }

    gsl_NODISCARD stop_token
    get_token(void) const noexcept
    {
        return stop_token(state_);
    }

    bool
    request_stop(void) noexcept
    {
        if (state_ == nullptr) return false;
        return state_->stopped.exchange(1, std::memory_order_acq_rel) == 0;
    }
};


} // namespace sysmakeshift


#endif // INCLUDED_SYSMAKESHIFT_STOP_TOKEN_HPP_
