
#ifndef INCLUDED_SYSMAKESHIFT_THREAD_HPP_
#define INCLUDED_SYSMAKESHIFT_THREAD_HPP_


#include <thread>
#include <future>
#include <utility>    // for move(), forward<>()
#include <memory>     // for unique_ptr<>
#include <functional> // for function<>

#include <gsl/gsl-lite.hpp> // for span<>, not_null<>, ssize(), gsl_NODISCARD

#include <sysmakeshift/detail/thread.hpp>


namespace sysmakeshift
{


    //
    // Like `std::thread`, but automatically joins the thread upon destruction.
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
    using std::thread::native_handle;

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


    //
    // Simple thread pool with support for thread core affinity.
    //
class thread_pool
{
public:
        //
        // Thread pool parameters.
        //
    struct params
    {
            //
            // How many threads to fork. A value of 0 indicates "as many as hardware threads are available".
            //
        int num_threads = 0;

            //
            // Controls whether threads are pinned to hardware threads, i.e. whether threads have a core affinity. Helps maintain data locality.
            //
        bool pin_to_hardware_threads = false;

            //
            // Maximal number of hardware threads to pin threads to. A value of 0 indicates "as many as possible".
            // If `max_num_hardware_threads` is 0 and `hardware_thread_mappings` is non-empty, `hardware_thread_mappings.size()` is taken as the
            // maximal number of hardware threads to pin threads to.
            // If `hardware_thread_mappings` is not empty, `max_num_hardware_threads` must not be larger than `hardware_thread_mappings.size()`.
            // Can be useful to increase reproducibility of synchronization and data race bugs by running multiple threads on the same core.
            //
        int max_num_hardware_threads = 0;

            //
            // Maps thread indices to hardware thread ids. If empty, the thread pool uses thread indices as hardware thread ids.
            // If non-empty and if `max_num_hardware_threads == 0`, `hardware_thread_mappings.size()` is taken as the maximal number of hardware
            // threads to pin threads to.
            //
        gsl::span<int const> hardware_thread_mappings = { };
    };

        //
        // State passed to tasks that are executed in thread pool.
        //
    class task_context
    {
        friend detail::thread_pool_thread;
        friend detail::thread_pool_impl;

    private:
        detail::thread_pool_impl_base& impl_;
        int threadIdx_;

        explicit task_context(detail::thread_pool_impl_base& _impl, int _threadIdx) noexcept
            : impl_(_impl), threadIdx_(_threadIdx)
        {
        }

    public:
            //
            // The current thread index.
            //
        gsl_NODISCARD int thread_index(void) const noexcept { return threadIdx_; }

            //
            // The number of concurrent threads.
            //
        gsl_NODISCARD int num_threads(void) const noexcept { return impl_.numThreads_; }
    };

private:
    gsl::not_null<detail::thread_pool_handle> handle_;

    static params const& check_params(params const& p)
    {
        Expects(p.num_threads >= 0);
        Expects(p.max_num_hardware_threads >= 0 && (p.num_threads == 0 || p.max_num_hardware_threads <= p.num_threads));
        Expects(p.hardware_thread_mappings.empty() || p.max_num_hardware_threads <= gsl::std20::ssize(p.hardware_thread_mappings));
        return p;
    }

    static detail::thread_pool_handle create(thread_pool::params p);
    void do_run(std::future<void>* completion, std::function<void(task_context)> task, int concurrency, bool join);

public:
    explicit thread_pool(params const& p)
        : handle_(create(check_params(p)))
    {
    }

    thread_pool(thread_pool&&) noexcept = default;
    thread_pool& operator =(thread_pool&&) noexcept = default;

    thread_pool(thread_pool const&) = delete;
    thread_pool& operator =(thread_pool const&) = delete;

        //
        // The number of concurrent threads.
        //
    gsl_NODISCARD int num_threads(void) const { return handle_->numThreads_; }

        //
        // Runs the given action on as many threads as indicated by `concurrency`, and waits until all tasks have run to completion.
        //ᅟ
        // `concurrency == 0` indicates that the maximum concurrency level should be used, i.e. the task is run on all threads in the thread pool.
        // `concurrency` may not exceed the number of threads in the thread pool.
        // The thread pool makes a dedicated copy of `action` for every participating thread and invokes it with an appropriate task context.
        // If `action()` throws an exception, `std::terminate()` is called.
        //
    void run(std::function<void(task_context)> action, int concurrency = 0) &
    {
        Expects(action);
        Expects(concurrency >= 0 && concurrency <= handle_->numThreads_);

        do_run(nullptr, std::move(action), concurrency, false);
    }

        //
        // Runs the given action on as many threads as indicated by `concurrency`, and waits until all tasks have run to completion.
        //ᅟ
        // `concurrency == 0` indicates that the maximum concurrency level should be used, i.e. the task is run on all threads in the thread pool.
        // `concurrency` may not exceed the number of threads in the thread pool.
        // The thread pool makes a dedicated copy of `action` for every participating thread and invokes it with an appropriate task context.
        // If `action()` throws an exception, `std::terminate()` is called.
        //
    void run(std::function<void(task_context)> action, int concurrency = 0) &&
    {
        Expects(action);
        Expects(concurrency >= 0 && concurrency <= handle_->numThreads_);

        do_run(nullptr, std::move(action), concurrency, true);
    }

        //
        // Runs the given action on as many threads as indicated by `concurrency`, and returns a `std::future<void>` which
        // represents the completion state of the tasks.
        //ᅟ
        // `concurrency == 0` indicates that the maximum concurrency level should be used, i.e. the task is run on all threads in the thread pool.
        // `concurrency` may not exceed the number of threads in the thread pool.
        // The thread pool makes a dedicated copy of `action` for every participating thread and invokes it with an appropriate task context.
        // If `action()` throws an exception, `std::terminate()` is called.
        //
    gsl_NODISCARD std::future<void> run_async(std::function<void(task_context)> action, int concurrency = 0) &
    {
        Expects(action);
        Expects(concurrency >= 0 && concurrency <= handle_->numThreads_);

        std::future<void> result;
        do_run(&result, std::move(action), concurrency, false);
        return result;
    }

        //
        // Runs the given action on as many threads as indicated by `concurrency`, and returns a `std::future<void>` which
        // represents the completion state of the tasks.
        //ᅟ
        // `concurrency == 0` indicates that the maximum concurrency level should be used, i.e. the task is run on all threads in the thread pool.
        // `concurrency` may not exceed the number of threads in the thread pool.
        // The thread pool makes a dedicated copy of `action` for every participating thread and invokes it with an appropriate task context.
        // If `action()` throws an exception, `std::terminate()` is called.
        //
    gsl_NODISCARD std::future<void> run_async(std::function<void(task_context)> action, int concurrency = 0) &&
    {
        Expects(action);
        Expects(concurrency >= 0 && concurrency <= handle_->numThreads_);

        std::future<void> result;
        do_run(&result, std::move(action), concurrency, true);
        return result;
    }
};


} // sysmakeshift


#endif // INCLUDED_SYSMAKESHIFT_THREAD_HPP_
