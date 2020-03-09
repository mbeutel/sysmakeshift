
#ifndef INCLUDED_SYSMAKESHIFT_THREAD_HPP_
#define INCLUDED_SYSMAKESHIFT_THREAD_HPP_


#include <thread>


namespace sysmakeshift {


    //
    // Reports the number of concurrent physical cores available.
    //ᅟ
    // Unlike `std::thread::hardware_concurrency()`, this only returns the number of cores, not the number of hardware threads.
    // On systems with simultaneous multithreading ("hyper-threading") enabled, `std::thread::hardware_concurrency()` typically
    // returns some multiple of `physical_concurrency()`.
    //
unsigned
physical_concurrency(void) noexcept;


    //
    // Like `std::thread`, but automatically joins the thread upon destruction.
    //ᅟ
    // Superseded by `std::jthread` in C++20, which also supports interruption with `std::stop_token`.
    //
class jthread : private std::thread
{
private:
    std::thread thread_;

public:
    using id = std::thread::id;
    using std::thread::detach;
    using std::thread::get_id;
    using std::thread::hardware_concurrency;
    using std::thread::join;
    using std::thread::joinable;

    template <typename F, typename... Ts>
    jthread(F&& func, Ts&&... args)
        : thread_(std::forward<F>(func), std::forward<Ts>(args)...)
    {
    }

    jthread(jthread&&) noexcept = default;
    jthread& operator =(jthread&&) noexcept = default;

    jthread(jthread const&) noexcept = delete;
    jthread& operator =(jthread const&) noexcept = delete;

    ~jthread(void)
    {
        if (thread_.joinable())
        {
            thread_.join();
        }
    }
};


} // namespace sysmakeshift


#endif // INCLUDED_SYSMAKESHIFT_THREAD_HPP_
