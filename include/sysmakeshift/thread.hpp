
#ifndef INCLUDED_SYSMAKESHIFT_THREAD_HPP_
#define INCLUDED_SYSMAKESHIFT_THREAD_HPP_


#include <thread>
#include <utility> // for forward<>()


namespace sysmakeshift
{


    // Like `std::thread`, but automatically joins the thread upon destruction.
    // Superseded by `std::jthread` in C++20.
class joining_thread
{
private:
    std::thread thread_;

public:
    template <typename F, typename... Ts>
    joining_thread(F&& func, Ts&&... args)
        : thread_(std::forward<F>(func), std::forward<Ts>(args)...)
    {
    }

    joining_thread(joining_thread&&) noexcept = default;
    joining_thread& operator =(joining_thread&&) noexcept = default;

    void join(void)
    {
        thread_.join();
    }

    ~joining_thread(void)
    {
        if (thread_.joinable())
        {
            thread_.join();
        }
    }
};


// TODO: implement thread_pool, fork(), and fork_join()


} // sysmakeshift


#endif // INCLUDED_SYSMAKESHIFT_THREAD_HPP_
