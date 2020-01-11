
#ifndef INCLUDED_SYSMAKESHIFT_DETAIL_TRANSACTION_HPP_
#define INCLUDED_SYSMAKESHIFT_DETAIL_TRANSACTION_HPP_


#include <utility>     // for move(), exchange()
#include <type_traits> // for integral_constant<>


namespace sysmakeshift
{

namespace detail
{


template <typename RollbackFuncT>
class transaction_t : private RollbackFuncT
{
private:
    bool done_;

public:
    constexpr transaction_t(RollbackFuncT&& rollbackFunc)
        : RollbackFuncT(std::move(rollbackFunc)), done_(false)
    {
    }
    ~transaction_t(void)
    {
        if (!done_)
        {
            static_cast<RollbackFuncT&>(*this)();
        }
    }

    constexpr void commit(void) noexcept
    {
        done_ = true;
    }

    constexpr transaction_t(transaction_t&& rhs) // pre-C++17 tax
        : RollbackFuncT(std::move(rhs)), done_(std::exchange(rhs.done_, true))
    {

    }
    transaction_t& operator =(transaction_t&&) = delete;
};
class no_op_transaction_t
{
public:
    constexpr void commit(void) noexcept
    {
    }

    no_op_transaction_t(void) noexcept = default;

    no_op_transaction_t(no_op_transaction_t&&) = default;
    no_op_transaction_t& operator =(no_op_transaction_t&&) = delete;
};
template <typename RollbackFuncT>
constexpr transaction_t<RollbackFuncT> make_transaction(RollbackFuncT rollbackFunc)
{
    return { std::move(rollbackFunc) };
}
template <typename RollbackFuncT>
constexpr transaction_t<RollbackFuncT> make_transaction(std::true_type /*enableRollback*/, RollbackFuncT rollbackFunc)
{
    return { std::move(rollbackFunc) };
}
template <typename RollbackFuncT>
constexpr no_op_transaction_t make_transaction(std::false_type /*enableRollback*/, RollbackFuncT)
{
    return { };
}


} // namespace detail

} // namespace sysmakeshift


#endif // INCLUDED_SYSMAKESHIFT_DETAIL_TRANSACTION_HPP_
