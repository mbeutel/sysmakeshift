
#include <string>
#include <cstddef>      // for size_t, ptrdiff_t
#include <cstring>      // for wcslen(), swprintf()
#include <utility>      // for move()
#include <memory>       // for unique_ptr<>
#include <algorithm>    // for max()
#include <exception>    // for terminate()
#include <stdexcept>    // for range_error
#include <type_traits>  // for remove_pointer<>
#include <system_error>
#include <optional>
#include <future>
#include <mutex>
#include <condition_variable>

#if defined(_WIN32)
# ifndef NOMINMAX
#  define NOMINMAX
# endif
# include <Windows.h> // GetLastError(), SetThreadAffinityMask()
# include <process.h> // _beginthreadex()
#elif defined(__linux__) || defined(__APPLE__)
# define USE_PTHREAD
# ifdef __linux__
#  define USE_PTHREAD_SETAFFINITY
# endif
# include <cerrno>
# include <pthread.h> // pthread_self(), pthread_setaffinity_np()
#else
# error Unsupported operating system.
#endif // _WIN32

#if defined(_WIN32) || defined(USE_PTHREAD_SETAFFINITY)
# define THREAD_PINNING_SUPPORTED
#endif // defined(_WIN32) || defined(USE_PTHREAD_SETAFFINITY)

#include <gsl-lite/gsl-lite.hpp> // for narrow<>(), narrow_cast<>(), span<>

#include <sysmakeshift/thread.hpp>
#include <sysmakeshift/buffer.hpp> // for aligned_buffer<>


namespace sysmakeshift
{

namespace detail
{


void posixCheck(int errorCode)
{
    if (errorCode != 0)
    {
        throw std::system_error(std::error_code(errorCode, std::generic_category()));
    }
}
void posixAssert(bool success)
{
    if (!success)
    {
        posixCheck(errno);
    }
}

#ifdef _WIN32
void win32Check(DWORD errorCode)
{
    if (errorCode != ERROR_SUCCESS)
    {
        throw std::system_error(std::error_code(errorCode, std::system_category())); // TODO: does this work correctly?
    }
}
void win32Assert(BOOL success)
{
    if (!success)
    {
        win32Check(::GetLastError());
    }
}
#endif // _WIN32


#if defined(_WIN32)
//
// Usage: SetThreadName ((DWORD)-1, "MainThread");
//
constexpr DWORD MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
    DWORD dwType; // Must be 0x1000.
    LPCSTR szName; // Pointer to name (in user address space).
    DWORD dwThreadId; // Thread ID (-1 implies caller thread).
    DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)

    // Sets the name of the thread with the given thread id for debugging purposes.
    // A thread id of -1 can be used to refer to the current thread.
void setThreadNameViaException(DWORD dwThreadId, const char* threadName)
{
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = threadName;
    info.dwThreadId = dwThreadId;
    info.dwFlags = 0;
#pragma warning(push)
# pragma warning(disable: 6320) // C6320: exception-filter expression is the constant EXCEPTION_EXECUTE_HANDLER. This may mask exceptions that were not intended to be handled
# pragma warning(disable: 6322) // C6322: empty __except block
    __try
    {
        ::RaiseException(MS_VC_EXCEPTION, 0, sizeof info / sizeof(ULONG_PTR), (ULONG_PTR*) &info);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
#pragma warning(pop)
}

typedef HRESULT SetThreadDescriptionType(HANDLE hThread, PCWSTR lpThreadDescription);

void setCurrentThreadDescription(const wchar_t* threadDescription)
{
    static SetThreadDescriptionType* const setThreadDescription = []
    {
        HMODULE hKernel32 = ::GetModuleHandleW(L"kernel32.dll");
        gsl_Expects(hKernel32 != NULL);
        return (SetThreadDescriptionType*) ::GetProcAddress(hKernel32, "SetThreadDescriptionType"); // available since Windows 10 1607 or Windows Server 2016
    }();

    if (setThreadDescription != nullptr)
    {
        setThreadDescription(::GetCurrentThread(), threadDescription);
    }
    else
    {
        std::string narrowDesc;
        narrowDesc.resize(std::wcslen(threadDescription));
        for (std::ptrdiff_t i = 0, c = std::ptrdiff_t(narrowDesc.size()); i < c; ++i)
        {
            narrowDesc[i] = gsl::narrow_cast<char>(threadDescription[i]);
        }
        detail::setThreadNameViaException(DWORD(-1), narrowDesc.c_str());
    }
}

#elif defined(USE_PTHREAD_SETAFFINITY)
struct cpu_set
{
private:
    std::size_t cpuCount_;
    cpu_set_t* data_;

public:
    cpu_set(void)
        : cpuCount_(std::thread::hardware_concurrency())
    {
        data_ = CPU_ALLOC(cpuCount_);
        if (data_ == nullptr)
        {
            throw std::bad_alloc();
        }
        std::size_t lsize = size();
        CPU_ZERO_S(lsize, data_);
    }
    std::size_t size(void) const noexcept
    {
        return CPU_ALLOC_SIZE(cpuCount_);
    }
    const cpu_set_t* data(void) const noexcept
    {
        return data_;
    }
    void setCpuFlag(std::size_t coreIdx)
    {
        gsl_Expects(coreIdx < cpuCount_);
        std::size_t lsize = size();
        CPU_SET_S(coreIdx, lsize, data_);
    }
    ~cpu_set(void)
    {
        CPU_FREE(data_);
    }
};

#endif // _WIN32

#ifdef _WIN32 // currently not needed on non-Windows platforms, so avoid defining it to suppress "unused function" warning
static void setThreadAffinity(std::thread::native_handle_type handle, std::size_t coreIdx)
{
# if defined(_WIN32)
    if (coreIdx >= sizeof(DWORD_PTR) * 8) // bits per byte
    {
        throw std::range_error("cannot currently handle more than 8*sizeof(void*) CPUs on Windows");
    }
    win32Assert(SetThreadAffinityMask((HANDLE) handle, DWORD_PTR(1) << coreIdx) != 0);
# elif defined(USE_PTHREAD_SETAFFINITY)
    cpu_set cpuSet;
    cpuSet.setCpuFlag(coreIdx);
    posixCheck(::pthread_setaffinity_np((pthread_t) handle, cpuSet.size(), cpuSet.data()));
# else
#  error Unsupported operating system.
# endif
}
#endif // _WIN32

#ifdef USE_PTHREAD_SETAFFINITY
static void setThreadAttrAffinity(pthread_attr_t& attr, std::size_t coreIdx)
{
    cpu_set cpuSet;
    cpuSet.setCpuFlag(coreIdx);
    posixCheck(::pthread_attr_setaffinity_np(&attr, cpuSet.size(), cpuSet.data()));
}
#endif // USE_PTHREAD_SETAFFINITY


#if defined(_WIN32)
struct win32_handle_deleter
{
    void operator ()(HANDLE handle) const noexcept
    {
        ::CloseHandle(handle);
    }
};
using win32_handle = std::unique_ptr<std::remove_pointer_t<HANDLE>, win32_handle_deleter>;
#elif defined(USE_PTHREAD)
class pthread_handle
{
private:
    pthread_t handle_;

public:
    pthread_t get(void) const noexcept { return handle_; }
    pthread_t release(void) noexcept { return std::exchange(handle_, { }); }

    explicit operator bool(void) const noexcept { return handle_ != pthread_t{ }; }

    pthread_handle(void)
        : handle_{ }
    {
    }
    explicit pthread_handle(pthread_t _handle)
        : handle_(_handle)
    {
    }
    pthread_handle(pthread_handle&& rhs) noexcept
        : handle_(rhs.release())
    {
    }
    pthread_handle& operator =(pthread_handle&& rhs) noexcept
    {
        if (&rhs != this)
        {
            if (handle_ != pthread_t{ })
            {
                pthread_detach(handle_);
            }
            handle_ = rhs.release();
        }
        return *this;
    }
    ~pthread_handle(void)
    {
        if (handle_ != pthread_t{ })
        {
            ::pthread_detach(handle_);
        }
    }
};

struct PThreadAttr
{
    pthread_attr_t attr;

    PThreadAttr(void)
    {
        posixCheck(::pthread_attr_init(&attr));
    }
    ~PThreadAttr(void)
    {
        posixCheck(::pthread_attr_destroy(&attr));
    }
};
#else
# error Unsupported operating system.
#endif // _WIN32


#ifdef _WIN32
static std::atomic<unsigned> threadPoolCounter{ };
#endif // _WIN32


    // TODO: this barrier can be made more efficient:
    //  - by using a better algorithm, e.g. a dissemination barrier
    //  - by using mostly syscall-free waits (e.g. atomics and exponential backoff), especially if all threads run on a
    //    dedicated hardware thread
    //  - by issuing PAUSE instructions, especially if multiple threads share a physical thread
class barrier
{
private:
    std::size_t value_;
    std::size_t baseValue_;
    std::size_t numThreads_;
    std::condition_variable cv_;
    std::mutex m_;

public:
    barrier(std::ptrdiff_t _numThreads)
        : value_(0), baseValue_(0), numThreads_(gsl::narrow<std::size_t>(_numThreads))
    {
            // We permit constructing a barrier with 0 threads, but calling `arrive_and_wait()` on it is illegal.
        gsl_Expects(_numThreads >= 0);

            // This is necessary to permit overlapping rounds.
        gsl_Expects(numThreads_ < std::numeric_limits<std::size_t>::max() / 2);
    }

    bool arrive_and_wait(void)
    {
        gsl_Expects(numThreads_ != 0);

        if (numThreads_ == 1) return true;

        auto lbaseValue = baseValue_;
        auto lock = std::unique_lock(m_);
        std::size_t pos = ++value_;
        if (pos == lbaseValue + numThreads_) // may overflow
        {
                // We are the last thread to arrive at the barrier. Update the base value and signal the condition variable.
            baseValue_ = pos;
            lock.unlock();
            cv_.notify_all();
            return true;
        }
        else
        {
                // We need to wait for other threads to arrive.
            while (value_ - lbaseValue < numThreads_)
            {
                cv_.wait(lock);
            }
            return false;
        }
    }
};


struct thread_pool_job
{
    std::function<void(thread_pool::task_context)> action;
    mutable std::optional<std::promise<void>> completion; // mutable because otherwise we cannot fulfill the promise
    int concurrency = 0;
    bool last = false;
    std::shared_future<std::optional<thread_pool_job>> nextJob;
};

struct thread_pool_thread
{
    thread_pool_impl& impl_;
    std::shared_future<std::optional<thread_pool_job>> nextJob_;
    int threadIdx_;

    thread_pool_thread(thread_pool_impl& _impl, std::shared_future<std::optional<thread_pool_job>> const& _nextJob) noexcept
        : impl_(_impl),
          nextJob_(_nextJob),
          threadIdx_(0) // to be set afterwards
    {
    }

    void operator ()(void);

#if defined(_WIN32)
    static unsigned __stdcall threadFunc(void* data);
#elif defined(USE_PTHREAD)
    static void* threadFunc(void* data);
#else
# error Unsupported operating system.
#endif // _WIN32
};

struct thread_pool_impl : thread_pool_impl_base
{
private:
#if defined(_WIN32)
    std::unique_ptr<detail::win32_handle[]> handles_;
#elif defined(USE_PTHREAD)
    std::unique_ptr<detail::pthread_handle[]> handles_;
#else
# error Unsupported operating system.
#endif // _WIN32

    barrier barrier_;
    std::promise<std::optional<thread_pool_job>> nextJob_;
    aligned_buffer<thread_pool_thread, cache_line_alignment> data_;
#ifdef _WIN32
    unsigned threadPoolId_; // for runtime thread identification during debugging
#endif // _WIN32
    bool running_;
#ifdef USE_PTHREAD
    bool needLaunch_;
#endif // USE_PTHREAD
#ifdef USE_PTHREAD_SETAFFINITY
    bool pinToHardwareThreads_;
    int maxNumHardwareThreads_;
    std::vector<int> hardwareThreadMappings_;
#endif // USE_PTHREAD_SETAFFINITY

    static std::size_t getHardwareThreadId(int threadIdx, int maxNumHardwareThreads, gsl::span<int const> hardwareThreadMappings)
    {
        auto subidx = threadIdx % maxNumHardwareThreads;
        return gsl::narrow<std::size_t>(
            !hardwareThreadMappings.empty() ? hardwareThreadMappings[subidx]
          : subidx);
    }

    void join_threads_and_free(void) noexcept // We cannot really handle join failure.
    {
        auto self = std::unique_ptr<thread_pool_impl>(this);
        
#if defined(_WIN32)
            // Join threads.
        for (int i = 0; i < numThreads_; )
        {
                // Join as many threads as possible in a single syscall. It is possible that this can be improved with a tree-like wait chain.
            HANDLE lhandles[MAXIMUM_WAIT_OBJECTS];
            int n = std::min(numThreads_ - i, MAXIMUM_WAIT_OBJECTS);
            for (int j = 0; j < n; ++j)
            {
                lhandles[j] = handles_[i + j].get();
            }
            DWORD result = ::WaitForMultipleObjects(n, lhandles, TRUE, INFINITE);
            detail::win32Assert(result != WAIT_FAILED);
            i += n;
        }
#elif defined(USE_PTHREAD)
            // Join threads. It is possible that this can be improved with a tree-like wait chain.
        for (int i = 0; i < numThreads_; ++i)
        {
            detail::posixCheck(::pthread_join(handles_[i].get(), NULL));
            handles_[i].release();
        }
#else
# error Unsupported operating system.
#endif
    }

    void schedule_termination(std::future<void>* completion) noexcept // We cannot really handle `bad_alloc` here.
    {
        nextJob_.set_value(std::nullopt);
        if (completion != nullptr)
        {
            *completion = std::async(std::launch::deferred, &thread_pool_impl::join_threads_and_free, this);
        }
    }

public:
    thread_pool_impl(thread_pool::params const& p)
        : thread_pool_impl_base{ p.num_threads },
          barrier_(p.num_threads),
          nextJob_{ },
          data_(gsl::narrow<std::size_t>(p.num_threads), std::in_place, *this, nextJob_.get_future().share()),
#ifdef _WIN32
          threadPoolId_(threadPoolCounter++),
#endif // _WIN32
          running_(false)
    {
        for (int i = 0; i < p.num_threads; ++i)
        {
            data_[i].threadIdx_ = i;
        }

#if defined(_WIN32)
            // Create threads suspended; set core affinity if desired.
        handles_ = std::make_unique<detail::win32_handle[]>(gsl::narrow<std::size_t>(p.num_threads));
        DWORD threadCreationFlags = p.pin_to_hardware_threads ? CREATE_SUSPENDED : 0;
        for (int i = 0; i < p.num_threads; ++i)
        {
            auto handle = detail::win32_handle(
                (HANDLE) ::_beginthreadex(NULL, 0, &thread_pool_thread::threadFunc, &data_[i], threadCreationFlags, nullptr));
            detail::win32Assert(handle != nullptr);
            if (p.pin_to_hardware_threads)
            {
                detail::setThreadAffinity(handle.get(), getHardwareThreadId(i, p.max_num_hardware_threads, p.hardware_thread_mappings));
            }
            handles_[i] = std::move(handle);
        }
#elif defined(USE_PTHREAD_SETAFFINITY)
            // Store hardware thread mappings for delayed thread creation.
        pinToHardwareThreads_ = p.pin_to_hardware_threads;
        maxNumHardwareThreads_ = p.max_num_hardware_threads;
        hardwareThreadMappings_.assign(p.hardware_thread_mappings.begin(), p.hardware_thread_mappings.end());
#endif
#ifdef USE_PTHREAD
        handles_ = std::make_unique<detail::pthread_handle[]>(gsl::narrow<std::size_t>(p.num_threads));
        needLaunch_ = false;
#endif // USE_PTHREAD
    }

    bool wait_at_barrier(void)
    {
        return barrier_.arrive_and_wait();
    }


#ifdef _WIN32
    unsigned thread_pool_id(void) const noexcept { return threadPoolId_; }
#endif // _WIN32

    void launch_threads(void) noexcept // We cannot really handle thread launch/resume failure.
    {
        if (running_) return;
        running_ = true;

#if defined(_WIN32)
            // Resume threads.
        for (int i = 0; i < numThreads_; ++i)
        {
            DWORD result = ::ResumeThread(handles_[i].get());
            detail::win32Assert(result != DWORD(-1));
        }
#elif defined(USE_PTHREAD)
            // Create threads; set core affinity if desired.
        for (int i = 0; i < numThreads_; ++i)
        {
            PThreadAttr attr;
# if defined(USE_PTHREAD_SETAFFINITY)
            if (pinToHardwareThreads_)
            {
                detail::setThreadAttrAffinity(attr.attr, getHardwareThreadId(i, maxNumHardwareThreads_, hardwareThreadMappings_));
            }
# endif // defined(USE_PTHREAD_SETAFFINITY)
            auto handle = pthread_t{ };
            detail::posixCheck(::pthread_create(&handle, &attr.attr, &thread_pool_thread::threadFunc, &data_[i]));
            handles_[i] = pthread_handle(handle);
        }

# if defined(USE_PTHREAD_SETAFFINITY)
            // Release thread mapping data.
        hardwareThreadMappings_ = { };
# endif // defined(USE_PTHREAD_SETAFFINITY)
#else
# error Unsupported operating system.
#endif
    }

    void schedule_job(std::future<void>* completion, std::function<void(thread_pool::task_context)> action, int concurrency, bool last) noexcept // We cannot really handle failure in future chaining.
    {
        auto completionPromise = std::optional<std::promise<void>>{ };
        if (completion != nullptr)
        {
            completionPromise = std::promise<void>{ };
            *completion = completionPromise->get_future();
        }
        auto nextJobPromise = std::promise<std::optional<thread_pool_job>>{ };
        nextJob_.set_value(thread_pool_job{ std::move(action), std::move(completionPromise), concurrency, last, nextJobPromise.get_future().share() });
        nextJob_ = std::move(nextJobPromise);
#ifdef USE_PTHREAD
        needLaunch_ = true;
#endif // USE_PTHREAD
    }

    void close_and_free(std::future<void>* completion)
    {
#if defined(_WIN32)
        schedule_termination(completion);
        launch_threads(); // The only clean way to quit never-resumed threads on Windows is to resume them and let them run to the end.
        if (completion == nullptr)
        {
            join_threads_and_free();
        }
#elif defined(USE_PTHREAD)
        if (needLaunch_ || running_)
        {
            schedule_termination(completion);
            if (needLaunch_)
            {
                launch_threads();
            }
            if (completion == nullptr)
            {
                join_threads_and_free();
            }
        }
        else // Threads were never launched, so we just quit and succeed immediately.
        {
            delete this;
            if (completion != nullptr)
            {
                auto completionPromise = std::promise<void>{ };
                completionPromise.set_value();
                *completion = completionPromise.get_future();
            }
        }
#else
# error Unsupported operating system.
#endif
    }
};

    // Like the parallel overloads of the standard algorithms, we terminate (implicitly) if an exception is thrown
    // by a thread pool job because the semantics of exceptions in multiplexed actions are unclear.
static void runJob(std::function<void(thread_pool::task_context)> action, thread_pool::task_context arg) noexcept
{
    action(arg);
}

void thread_pool_thread::operator ()(void)
{
    for (;;)
    {
        auto const& job = nextJob_.get();
        if (!job.has_value())
        {
            nextJob_ = { };
            break;
        }

            // Only the first `concurrency` threads shall participate in executing this task.
        int concurrency = job->concurrency;
        if (concurrency == 0)
        {
            concurrency = impl_.numThreads_;
        }
        if (threadIdx_ < concurrency)
        {
            runJob(job->action, thread_pool::task_context{ impl_, threadIdx_ });
        }

        if (!job->last || job->completion.has_value())
        {
            bool lastToArrive = impl_.wait_at_barrier();
            if (lastToArrive && job->completion.has_value())
            {
                job->completion->set_value();
            }
        }

            // The extra indirection is necessary to keep the object alive while copying it.
        auto newNextJob = job->nextJob;
        nextJob_ = std::move(newNextJob);
    }
}

#if defined(_WIN32)
unsigned __stdcall thread_pool_thread::threadFunc(void* data)
{
    thread_pool_thread& self = *static_cast<thread_pool_thread*>(data);
    {
        wchar_t buf[64];
        std::swprintf(buf, sizeof buf / sizeof(wchar_t), L"sysmakeshift thread pool #%d, thread %d",
            self.impl_.thread_pool_id(), self.threadIdx_);
        detail::setCurrentThreadDescription(buf);
    }
    self();
    return 0;
}
#elif defined(USE_PTHREAD)
void* thread_pool_thread::threadFunc(void* data)
{
    thread_pool_thread& self = *static_cast<thread_pool_thread*>(data);
    self();
    return nullptr;
}
#else
# error Unsupported operating system.
#endif


void thread_pool_impl_deleter::operator ()(thread_pool_impl_base* impl)
{
    static_cast<detail::thread_pool_impl*>(impl)->close_and_free(nullptr);
}


} // namespace detail


detail::thread_pool_handle thread_pool::create(thread_pool::params p)
{
        // Replace placeholder arguments with appropriate default values.
    int hardwareConcurrency = gsl::narrow<int>(std::thread::hardware_concurrency());
    if (p.num_threads == 0)
    {
        p.num_threads = hardwareConcurrency;
    }
    if (p.max_num_hardware_threads == 0)
    {
        p.max_num_hardware_threads =
            !p.hardware_thread_mappings.empty() ? gsl::narrow<int>(p.hardware_thread_mappings.size())
          : hardwareConcurrency;
    }
    p.max_num_hardware_threads = std::max(p.max_num_hardware_threads, hardwareConcurrency);

        // Check system support for thread pinning.
#ifndef THREAD_PINNING_SUPPORTED
    if (p.pin_to_hardware_threads)
    {
            // Thread pinning not currently supported on this OS.
        throw std::system_error(std::make_error_code(std::errc::not_supported),
            "pinning to hardware threads is not implemented on this operating system");
    }
#endif // !THREAD_PINNING_SUPPORTED

    return detail::thread_pool_handle(new detail::thread_pool_impl(p));
}

std::future<void> thread_pool::do_run(std::function<void(task_context)> action, int concurrency, bool join)
{
    std::future<void> completion;
    auto& impl = static_cast<detail::thread_pool_impl&>(*handle_.get());
    impl.schedule_job(join ? nullptr : &completion, std::move(action), concurrency, join);
    if (join)
    {
        auto lhandle = detail::thread_pool_handle(std::move(handle_));
        lhandle.release();
        impl.close_and_free(&completion);
    }
    else
    {
        impl.launch_threads();
    }
    return completion;
}


} // namespace sysmakeshift
