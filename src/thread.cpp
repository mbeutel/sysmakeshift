
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
# define WIN32_LEAN_AND_MEAN
# define NOMINMAX 
# include <Windows.h> // GetLastError(), SetThreadAffinityMask()
# include <process.h> // _beginthreadex()
#elif defined(__linux__)
# include <cerrno>
# include <pthread.h> // pthread_self(), pthread_setaffinity_np()
#else
# error Unsupported operating system.
#endif // _WIN32

#include <gsl/gsl-lite.hpp> // for narrow_cast<>()

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
        win32Check(GetLastError());
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
        RaiseException(MS_VC_EXCEPTION, 0, sizeof info / sizeof(ULONG_PTR), (ULONG_PTR*) &info);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
#pragma warning(pop)
}

using SetThreadDescriptionType = HRESULT(HANDLE hThread, PCWSTR lpThreadDescription);

void setCurrentThreadDescription(const wchar_t* threadDescription)
{
    static SetThreadDescriptionType* const setThreadDescription = []
    {
        HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
        Expects(hKernel32 != NULL);
        return (SetThreadDescriptionType*) GetProcAddress(hKernel32, "SetThreadDescriptionType"); // available since Windows 10 1607 or Windows Server 2016
    }();

    if (setThreadDescription != nullptr)
    {
        setThreadDescription(GetCurrentThread(), threadDescription);
    }
    else
    {
        std::string narrowDesc;
        narrowDesc.resize(std::wcslen(threadDescription));
        for (std::ptrdiff_t i = 0, c = std::ptrdiff_t(narrowDesc.size()); i < c; ++i)
        {
            narrowDesc[i] = gsl::narrow_cast<char>(threadDescription[i]);
        }
        setThreadNameViaException(DWORD(-1), narrowDesc.c_str());
    }
}

#elif defined(__linux__)
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
        Expects(coreIdx < cpuCount_);
        std::size_t lsize = size();
        CPU_SET_S(coreIdx, lsize, data_);
    }
    ~cpu_set(void)
    {
        CPU_FREE(data_);
    }
};

#else // TODO: support MacOS
# error Unsupported operating system
#endif // _WIN32

#ifdef _WIN32 // currently not needed on non-Windows platforms, so avoid defining it to suppress "unused function" warning
static void setThreadAffinity(std::thread::native_handle_type handle, std::size_t coreIdx)
{
# ifdef _WIN32
    if (coreIdx >= sizeof(DWORD_PTR) * 8) // bits per byte
    {
        throw std::range_error("cannot currently handle more than 8*sizeof(void*) CPUs on Windows");
    }
    win32Assert(SetThreadAffinityMask((HANDLE) handle, DWORD_PTR(1) << coreIdx) != 0);
# else // _WIN32
    cpu_set cpuSet;
    cpuSet.setCpuFlag(coreIdx);
    posixCheck(pthread_setaffinity_np((pthread_t) handle, cpuSet.size(), cpuSet.data()));
# endif // _WIN32
}
#endif // _WIN32

#ifndef _WIN32
static void setThreadAttrAffinity(pthread_attr_t& attr, std::size_t coreIdx)
{
    cpu_set cpuSet;
    cpuSet.setCpuFlag(coreIdx);
    posixCheck(pthread_attr_setaffinity_np(&attr, cpuSet.size(), cpuSet.data()));
}
#endif // _WIN32


#if defined(_WIN32)
struct win32_handle_deleter
{
    void operator ()(HANDLE handle) const noexcept
    {
        CloseHandle(handle);
    }
};
using win32_handle = std::unique_ptr<std::remove_pointer_t<HANDLE>, win32_handle_deleter>;
#elif defined(__linux__)
class pthread_handle
{
private:
    pthread_t handle_;

public:
    pthread_t handle(void) const noexcept { return handle_; }
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
            pthread_detach(handle_);
        }
    }
};

struct PThreadAttr
{
    pthread_attr_t attr;

    PThreadAttr(void)
    {
        posixCheck(pthread_attr_init(&attr));
    }
    ~PThreadAttr(void)
    {
        posixCheck(pthread_attr_destroy(&attr));
    }
};
#else
# error Unsupported operating system.
#endif // _WIN32




/*static std::atomic<unsigned> forkJoinCallCounter{ };

void forkJoinImpl(std::function<void(ForkJoinTaskParams taskParams)> action, const ForkJoinParams& params)
{
    int hardwareConcurrency = int(std::thread::hardware_concurrency());
    int numThreads = p.numThreads;
    if (numThreads == 0)
    {
        numThreads = hardwareConcurrency;
    }

    int maxNumHardwareThreads = p.maxNumHardwareThreads;
    if (maxNumHardwareThreads == 0)
    {
        maxNumHardwareThreads = !p.hardware_thread_mappings.empty()
            ? int(p.hardware_thread_mappings.size())
            : hardwareConcurrency;
    }

    auto hardwareThreadId = [maxNumHardwareThreads, &params](int threadIdx)
    {
        auto subidx = threadIdx % maxNumHardwareThreads;
        return !p.hardware_thread_mappings.empty()
            ? p.hardware_thread_mappings[subidx]
            : subidx;
    };

    if (numThreads == 1 && !p.pin_to_hardware_threads)
    {
            // Avoid launching any threads at all if only one non-pinned thread is requested, and execute the action synchronously.
        action(ForkJoinTaskParams{ 0, 1 });
        return;
    }

        // allocate call id
    int callId = gsl::narrow_cast<int>(forkJoinCallCounter++);

        // allocate thread data
    auto threadFunctors = std::vector<asc::stencilgen::detail::ThreadFunctor>{ };
    threadFunctors.reserve(numThreads);
    for (int i = 0; i < numThreads; ++i)
    {
        threadFunctors.emplace_back(asc::stencilgen::detail::ThreadFunctor{ action, callId, i, numThreads });
    }

#ifdef _WIN32
        // Create threads (if core affinity is desired, create them suspended, then set the affinity).
    std::vector<asc::stencilgen::detail::win32_handle> threadHandles;
    threadHandles.reserve(numThreads);
    DWORD threadCreationFlags = p.pin_to_hardware_threads ? CREATE_SUSPENDED : 0;
    for (int i = 0; i < numThreads; ++i)
    {
        auto handle = asc::stencilgen::detail::win32_handle(
            (HANDLE) _beginthreadex(NULL, 0, asc::stencilgen::detail::threadFunc, &threadFunctors[i], threadCreationFlags, nullptr));
        asc::stencilgen::detail::win32Assert(handle.handle() != nullptr);
        if (p.pin_to_hardware_threads)
        {
            asc::stencilgen::detail::setThreadAffinity(handle.handle(), std::size_t(hardwareThreadId(i)));
        }
        threadHandles.push_back(std::move(handle));
    }

        // Launch threads if we created them in suspended state.
    if (p.pin_to_hardware_threads)
    {
        for (int i = 0; i < numThreads; ++i)
        {
            DWORD result = ResumeThread(threadHandles[i].handle());
            asc::stencilgen::detail::win32Assert(result != (DWORD) -1);
        }
    }
    
        // Join threads.
    for (int i = 0; i < numThreads; )
    {
            // Join as many threads as possible in a single syscall. It is possible that this can be improved with a tree-like wait chain.
        HANDLE handles[MAXIMUM_WAIT_OBJECTS];
        int n = std::min(numThreads - i, MAXIMUM_WAIT_OBJECTS);
        for (int j = 0; j < n; ++j)
        {
            handles[j] = threadHandles[i + j].handle();
        }
        DWORD result = WaitForMultipleObjects(n, handles, TRUE, INFINITE);
        asc::stencilgen::detail::win32Assert(result != WAIT_FAILED);
        i += n;
    }
#else // ^^^ _WIN32 ^^^ / vvv !_WIN32 vvv
        // Create threads (with core affinity if desired).
    std::vector<asc::stencilgen::detail::pthread_handle> threadHandles;
    threadHandles.reserve(numThreads);
    for (int i = 0; i < numThreads; ++i)
    {
        PThreadAttr attr;
        if (p.pin_to_hardware_threads)
        {
            asc::stencilgen::detail::setThreadAttrAffinity(attr.attr, std::size_t(hardwareThreadId(i)));
        }
        auto handle = pthread_t{ };
        asc::stencilgen::detail::posixCheck(pthread_create(&handle, &attr.attr, asc::stencilgen::detail::threadFunc, &threadFunctors[i]));
        threadHandles.push_back(asc::stencilgen::detail::pthread_handle(handle));
    }

        // Join threads. It is possible that this can be improved with a tree-like wait chain.
    for (int i = 0; i < numThreads; ++i)
    {
        asc::stencilgen::detail::posixCheck(pthread_join(threadHandles[i].handle(), NULL));
        threadHandles[i].release();
    }
#endif // _WIN32
}*/


static std::atomic<unsigned> threadPoolCounter{ };


class barrier
{
private:
    std::atomic<std::size_t> value_;
    std::size_t baseValue_;
    std::condition_variable cv_;
    std::mutex m_;

public:
    barrier(void)
        : value_(0), baseValue_(0)
    {
    }

    bool wait(std::size_t numThreads)
    {
        std::size_t pos = value_.fetch_add(1, std::memory_order::memory_order_acq_rel);
        auto targetValue = baseValue_ + numThreads; // may overflow
        if (pos == targetValue)
        {
                // We are the last thread to arrive at the barrier. Update the base value and signal the condition variable.
            baseValue_ = pos;
            cv_.notify_all();
            return true;
        }
        else
        {
                // We need to wait for other threads to arrive.
            auto lock = std::unique_lock(m_);
            while (value_.load(std::memory_order::memory_order_acquire) != targetValue)
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
    std::shared_future<std::optional<thread_pool_job>> nextJob;
};

struct thread_pool_thread_data
{
    thread_pool_impl& impl_;
    barrier barrier_;
    std::shared_future<std::optional<thread_pool_job>> nextJob_;
    int threadIdx_;

    thread_pool_thread_data(thread_pool_impl& _impl, std::shared_future<std::optional<thread_pool_job>> _nextJob)
        : impl_(_impl),
          nextJob_(std::move(_nextJob)),
          threadIdx_(0) // to be set afterwards
    {
    }

    void operator ()(void);

#ifdef _WIN32
    static unsigned __stdcall threadFunc(void* data);
#else // ^^^ _WIN32 ^^^ / vvv !_WIN32 vvv
    static void* threadFunc(void* data);
#endif // _WIN32
};

struct thread_pool_impl : thread_pool_impl_base
{
#if defined(_WIN32)
    std::unique_ptr<detail::win32_handle[]> handles_;
#elif defined(__linux__)
    std::unique_ptr<detail::pthread_handle[]> handles_;
#else
# error Unsupported operating system.
#endif // _WIN32

    std::promise<std::optional<thread_pool_job>> nextJob_;
    aligned_buffer<thread_pool_thread_data, alignment::cache_line> data_;
    unsigned threadPoolId_; // for runtime thread identification during debugging
    bool running_;
    std::unique_ptr<detail::win32_handle[]> handles_;

    thread_pool_impl(thread_pool::params const& p)
        : thread_pool_impl_base{ p.num_threads },
          nextJob_{ },
          data_(gsl::narrow_cast<std::size_t>(p.num_threads), std::in_place, *this, nextJob_.get_future().share()),
          threadPoolId_(threadPoolCounter++),
          running_(false)
    {
        //handles_.reserve(gsl::narrow_cast<std::size_t>(p.num_threads));

#ifdef _WIN32
        // Create threads (if core affinity is desired, create them suspended, then set the affinity).
    std::vector<asc::stencilgen::detail::win32_handle> threadHandles;
    threadHandles.reserve(numThreads);
    DWORD threadCreationFlags = p.pin_to_hardware_threads ? CREATE_SUSPENDED : 0;
    for (int i = 0; i < numThreads; ++i)
    {
        auto handle = asc::stencilgen::detail::win32_handle(
            (HANDLE) _beginthreadex(NULL, 0, asc::stencilgen::detail::threadFunc, &threadFunctors[i], threadCreationFlags, nullptr));
        asc::stencilgen::detail::win32Assert(handle.handle() != nullptr);
        if (p.pin_to_hardware_threads)
        {
            asc::stencilgen::detail::setThreadAffinity(handle.handle(), std::size_t(hardwareThreadId(i)));
        }
        threadHandles.push_back(std::move(handle));
    }
    }
};

    // Like the parallel overloads of the standard algorithms, we terminate (implicitly) if an exception is thrown
    // by a thread pool job because the semantics of exceptions in multiplexed actions are unclear.
static void runJob(std::function<void(thread_pool::task_context)> action, thread_pool::task_context arg) noexcept
{
    action(arg);
}

void thread_pool_thread_data::operator ()(void)
{
    for (;;)
    {
        auto const& job = nextJob_.get();
        if (!job.has_value())
        {
            nextJob_ = { };
            break;
        }
        nextJob_ = std::move(job->nextJob);

            // Only the first `concurrency` threads shall participate in executing this task.
        if (job->concurrency != 0 && threadIdx_ < job->concurrency)
        {
            runJob(job->action, thread_pool::task_context{ impl_, threadIdx_ });
        }

        bool lastToArrive = barrier_.wait(static_cast<std::size_t>(job->concurrency));
        if (lastToArrive && job->completion.has_value())
        {
            job->completion->set_value();
        }
    }
}

#ifdef _WIN32
unsigned __stdcall thread_pool_thread_data::threadFunc(void* data)
{
    thread_pool_thread_data& self = *static_cast<thread_pool_thread_data*>(data);
    {
        wchar_t buf[64];
        std::swprintf(buf, sizeof buf / sizeof(wchar_t), L"sysmakeshift thread pool #%d, thread %d",
            self.impl_.threadPoolId_, self.threadIdx_);
        detail::setCurrentThreadDescription(buf);
    }
    self();
    return 0;
}
#else // ^^^ _WIN32 ^^^ / vvv !_WIN32 vvv
void* thread_pool_thread_data::threadFunc(void* data)
{
    thread_pool_thread_data& self = *static_cast<thread_pool_thread_data*>(data);
    self();
    return nullptr;
}
#endif // _WIN32


static gsl::not_null<std::unique_ptr<detail::thread_pool_impl_base>> create_thread_pool(thread_pool::params p)
{
        // Replace placeholder arguments with appropriate default values.
    int hardwareConcurrency = gsl::narrow_cast<int>(std::thread::hardware_concurrency());
    if (p.num_threads == 0)
    {
        p.num_threads = hardwareConcurrency;
    }
    if (p.max_num_hardware_threads == 0)
    {
        p.max_num_hardware_threads =
            !p.hardware_thread_mappings.empty() ? gsl::narrow_cast<int>(p.hardware_thread_mappings.size())
          : hardwareConcurrency;
    }
    p.max_num_hardware_threads = std::max(p.max_num_hardware_threads, hardwareConcurrency);

    return gsl::not_null(std::make_unique<detail::thread_pool_impl>(p));
}


} // namespace detail


thread_pool::thread_pool(params const& p, internal_)
    : impl_(detail::create_thread_pool(p))
{
}


thread_pool::impl thread_pool::acquire(params const& p)
{
    int hardwareConcurrency = int(std::thread::hardware_concurrency());
    int numThreads = p.num_threads;
    if (numThreads == 0)
    {
        numThreads = hardwareConcurrency;
    }

    int maxNumHardwareThreads = p.max_num_hardware_threads;
    if (maxNumHardwareThreads == 0)
    {
        maxNumHardwareThreads = !p.hardware_thread_mappings.empty()
            ? int(p.hardware_thread_mappings.size())
            : hardwareConcurrency;
    }

    auto hardwareThreadId = [maxNumHardwareThreads, &p](int threadIdx)
    {
        auto subidx = threadIdx % maxNumHardwareThreads;
        return !p.hardware_thread_mappings.empty()
            ? p.hardware_thread_mappings[subidx]
            : subidx;
    };

        // allocate thread pool id
    int threadPoolId = gsl::narrow_cast<int>(detail::threadPoolCounter++);

        // allocate thread data
    auto threadFunctors = std::vector<asc::stencilgen::detail::ThreadFunctor>{ };
    threadFunctors.reserve(numThreads);
    for (int i = 0; i < numThreads; ++i)
    {
        threadFunctors.emplace_back(asc::stencilgen::detail::ThreadFunctor{ action, callId, i, numThreads });
    }

#ifdef _WIN32
        // Create threads (if core affinity is desired, create them suspended, then set the affinity).
    std::vector<asc::stencilgen::detail::win32_handle> threadHandles;
    threadHandles.reserve(numThreads);
    DWORD threadCreationFlags = p.pin_to_hardware_threads ? CREATE_SUSPENDED : 0;
    for (int i = 0; i < numThreads; ++i)
    {
        auto handle = asc::stencilgen::detail::win32_handle(
            (HANDLE) _beginthreadex(NULL, 0, asc::stencilgen::detail::threadFunc, &threadFunctors[i], threadCreationFlags, nullptr));
        asc::stencilgen::detail::win32Assert(handle.handle() != nullptr);
        if (p.pin_to_hardware_threads)
        {
            asc::stencilgen::detail::setThreadAffinity(handle.handle(), std::size_t(hardwareThreadId(i)));
        }
        threadHandles.push_back(std::move(handle));
    }

        // Launch threads if we created them in suspended state.
    if (p.pin_to_hardware_threads)
    {
        for (int i = 0; i < numThreads; ++i)
        {
            DWORD result = ResumeThread(threadHandles[i].handle());
            asc::stencilgen::detail::win32Assert(result != (DWORD) -1);
        }
    }
    
        // Join threads.
    for (int i = 0; i < numThreads; )
    {
            // Join as many threads as possible in a single syscall. It is possible that this can be improved with a tree-like wait chain.
        HANDLE handles[MAXIMUM_WAIT_OBJECTS];
        int n = std::min(numThreads - i, MAXIMUM_WAIT_OBJECTS);
        for (int j = 0; j < n; ++j)
        {
            handles[j] = threadHandles[i + j].handle();
        }
        DWORD result = WaitForMultipleObjects(n, handles, TRUE, INFINITE);
        asc::stencilgen::detail::win32Assert(result != WAIT_FAILED);
        i += n;
    }
#else // ^^^ _WIN32 ^^^ / vvv !_WIN32 vvv
        // Create threads (with core affinity if desired).
    std::vector<asc::stencilgen::detail::pthread_handle> threadHandles;
    threadHandles.reserve(numThreads);
    for (int i = 0; i < numThreads; ++i)
    {
        PThreadAttr attr;
        if (p.pin_to_hardware_threads)
        {
            asc::stencilgen::detail::setThreadAttrAffinity(attr.attr, std::size_t(hardwareThreadId(i)));
        }
        auto handle = pthread_t{ };
        asc::stencilgen::detail::posixCheck(pthread_create(&handle, &attr.attr, asc::stencilgen::detail::threadFunc, &threadFunctors[i]));
        threadHandles.push_back(asc::stencilgen::detail::pthread_handle(handle));
    }

        // Join threads. It is possible that this can be improved with a tree-like wait chain.
    for (int i = 0; i < numThreads; ++i)
    {
        asc::stencilgen::detail::posixCheck(pthread_join(threadHandles[i].handle(), NULL));
        threadHandles[i].release();
    }
#endif // _WIN32
}

thread_pool::thread_pool(params const& p, internal_)
    : impl_(acquire(p))
{
}

thread_pool::~thread_pool(void)
{
}

thread_pool::thread_pool(thread_pool&& rhs) noexcept
    : impl_(std::move(rhs.impl_))
{
}

thread_pool& thread_pool::operator =(thread_pool&& rhs) noexcept
{
    impl_ = std::move(rhs.impl_);
    return *this;
}


} // namespace sysmakeshift
