
#ifndef INCLUDED_SYSMAKESHIFT_THREAD_SQUAD_HPP_
#define INCLUDED_SYSMAKESHIFT_THREAD_SQUAD_HPP_


#include <thread>
#include <future>
#include <utility>    // for move(), forward<>()
#include <memory>     // for unique_ptr<>
#include <functional> // for function<>

#include <gsl-lite/gsl-lite.hpp> // for span<>, not_null<>, ssize(), gsl_NODISCARD

#include <sysmakeshift/detail/thread_squad.hpp>


namespace sysmakeshift {


namespace gsl = ::gsl_lite;


    //
    // Simple thread squad with support for thread core affinity.
    //
class thread_squad
{
public:
        //
        // Thread squad parameters.
        //
    struct params
    {
            //
            // How many threads to fork. A value of 0 indicates "as many as hardware threads are available".
            //
        int num_threads = 0;

            //
            // Controls whether threads are pinned to hardware threads, i.e. whether threads have a core affinity. Helps maintain
            // data locality.
            //
        bool pin_to_hardware_threads = false;

            //
            // Maximal number of hardware threads to pin threads to. A value of 0 indicates "as many as possible".
            //ᅟ
            // If `max_num_hardware_threads` is 0 and `hardware_thread_mappings` is non-empty, `hardware_thread_mappings.size()`
            // is taken as the maximal number of hardware threads to pin threads to.
            // If `hardware_thread_mappings` is not empty, `max_num_hardware_threads` must not be larger than
            // `hardware_thread_mappings.size()`.
            // Setting `max_num_hardware_threads` can be useful to increase reproducibility of synchronization and data race bugs
            // by running multiple threads on the same core.
            //
        int max_num_hardware_threads = 0;

            //
            // Maps thread indices to hardware thread ids. If empty, the thread squad uses thread indices as hardware thread ids.
            //ᅟ
            // If non-empty and if `max_num_hardware_threads == 0`, `hardware_thread_mappings.size()` is taken as the maximal
            // number of hardware threads to pin threads to.
            //
        gsl::span<int const> hardware_thread_mappings = { };
    };

        //
        // State passed to tasks that are executed in thread squad.
        //
    class task_context
    {
        friend detail::thread_squad_thread;
        friend detail::thread_squad_impl;

    private:
        detail::thread_squad_impl_base& impl_;
        int threadIdx_;

        explicit task_context(detail::thread_squad_impl_base& _impl, int _threadIdx) noexcept
            : impl_(_impl), threadIdx_(_threadIdx)
        {
        }

    public:
            //
            // The current thread index.
            //
        gsl_NODISCARD int
        thread_index(void) const noexcept
        {
            return threadIdx_;
        }

            //
            // The number of concurrent threads.
            //
        gsl_NODISCARD int
        num_threads(void) const noexcept
        {
            return impl_.numThreads_;
        }
    };

private:
    gsl::not_null<detail::thread_squad_handle> handle_;

    static params const&
    check_params(params const& p)
    {
        gsl_Expects(p.num_threads >= 0);
        gsl_Expects(p.max_num_hardware_threads >= 0 && (p.num_threads == 0 || p.max_num_hardware_threads <= p.num_threads));
        gsl_Expects(p.hardware_thread_mappings.empty() || (p.max_num_hardware_threads <= gsl::ssize(p.hardware_thread_mappings)
            && p.num_threads <= gsl::ssize(p.hardware_thread_mappings)));
        return p;
    }

    static detail::thread_squad_handle
    create(thread_squad::params p);
    
    std::future<void>
    do_run(std::function<void(task_context)> task, int concurrency, bool join);

public:
    explicit thread_squad(params const& p)
        : handle_(create(check_params(p)))
    {
    }

    thread_squad(thread_squad&&) noexcept = default;
    thread_squad&
    operator =(thread_squad&&) noexcept = default;

    thread_squad(thread_squad const&) = delete;
    thread_squad&
    operator =(thread_squad const&) = delete;

        //
        // The number of concurrent threads.
        //
    gsl_NODISCARD int
    num_threads(void) const
    {
        return handle_->numThreads_;
    }

        //
        // Runs the given action on `concurrency` threads and waits until all tasks have run to completion.
        //ᅟ
        // `concurrency == 0` indicates that the maximum concurrency level should be used, i.e. the task is run on all threads in
        // the thread squad. `concurrency` must not exceed the number of threads in the thread squad.
        // The thread squad makes a dedicated copy of `action` for every participating thread and invokes it with an appropriate
        // task context. If `action()` throws an exception, `std::terminate()` is called.
        //
    void
    run(std::function<void(task_context)> action, int concurrency = 0) &
    {
        gsl_Expects(action);
        gsl_Expects(concurrency >= 0 && concurrency <= handle_->numThreads_);

        do_run(std::move(action), concurrency, false).wait();
    }

        //
        // Runs the given action on `concurrency` threads and waits until all tasks have run to completion.
        //ᅟ
        // `concurrency == 0` indicates that the maximum concurrency level should be used, i.e. the task is run on all threads in
        // the thread squad. `concurrency` must not exceed the number of threads in the thread squad.
        // The thread squad makes a dedicated copy of `action` for every participating thread and invokes it with an appropriate
        // task context. If `action()` throws an exception, `std::terminate()` is called.
        //
    void
    run(std::function<void(task_context)> action, int concurrency = 0) &&
    {
        gsl_Expects(action);
        gsl_Expects(concurrency >= 0 && concurrency <= handle_->numThreads_);

        do_run(std::move(action), concurrency, true).wait();
    }

        //
        // Runs the given action on `concurrency` threads and returns a `std::future<void>` which represents the completion state
        // of the tasks.
        //ᅟ
        // `concurrency == 0` indicates that the maximum concurrency level should be used, i.e. the task is run on all threads in
        // the thread squad. `concurrency` must not exceed the number of threads in the thread squad.
        // The thread squad makes a dedicated copy of `action` for every participating thread and invokes it with an appropriate
        // task context. If `action()` throws an exception, `std::terminate()` is called.
        //
    gsl_NODISCARD std::future<void>
    run_async(std::function<void(task_context)> action, int concurrency = 0) &
    {
        gsl_Expects(action);
        gsl_Expects(concurrency >= 0 && concurrency <= handle_->numThreads_);

        return do_run(std::move(action), concurrency, false);
    }

        //
        // Runs the given action on `concurrency` threads and returns a `std::future<void>` which represents the completion state
        // of the tasks.
        //ᅟ
        // `concurrency == 0` indicates that the maximum concurrency level should be used, i.e. the task is run on all threads in
        // the thread squad. `concurrency` must not exceed the number of threads in the thread squad.
        // The thread squad makes a dedicated copy of `action` for every participating thread and invokes it with an appropriate
        // task context. If `action()` throws an exception, `std::terminate()` is called.
        //
    gsl_NODISCARD std::future<void>
    run_async(std::function<void(task_context)> action, int concurrency = 0) &&
    {
        gsl_Expects(action);
        gsl_Expects(concurrency >= 0 && concurrency <= handle_->numThreads_);

        return do_run(std::move(action), concurrency, true);
    }
};


} // namespace sysmakeshift


#endif // INCLUDED_SYSMAKESHIFT_THREAD_SQUAD_HPP_
