
//#define DEBUG_WAIT_CHAIN
#ifdef DEBUG_WAIT_CHAIN
# include <cstdio>
#endif // DEBUG_WAIT_CHAIN

#include <string>
#include <cstddef>            // for size_t, ptrdiff_t
#include <cstring>            // for wcslen(), swprintf()
#include <utility>            // for move()
#include <memory>             // for unique_ptr<>
#include <algorithm>          // for min(), max()
#include <exception>          // for terminate()
#include <stdexcept>          // for range_error
#include <type_traits>        // for remove_pointer<>
#include <system_error>
#include <thread>
#include <atomic>
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
#endif

#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
# include <emmintrin.h>
#endif // defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)

#if defined(_WIN32) || defined(USE_PTHREAD_SETAFFINITY)
# define THREAD_PINNING_SUPPORTED
#endif // defined(_WIN32) || defined(USE_PTHREAD_SETAFFINITY)

#include <gsl-lite/gsl-lite.hpp> // for narrow<>(), narrow_cast<>(), span<>

#include <sysmakeshift/thread_squad.hpp>
#include <sysmakeshift/buffer.hpp>      // for aligned_buffer<>

#include <sysmakeshift/detail/errors.hpp>


namespace sysmakeshift {

namespace detail {


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
void
setThreadNameViaException(DWORD dwThreadId, const char* threadName)
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

void
setCurrentThreadDescription(const wchar_t* threadDescription)
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
    std::size_t
    size(void) const noexcept
    {
        return CPU_ALLOC_SIZE(cpuCount_);
    }
    const cpu_set_t*
    data(void) const noexcept
    {
        return data_;
    }
    void
    set_cpu_flag(std::size_t coreIdx)
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
static void
setThreadAffinity(std::thread::native_handle_type handle, std::size_t coreIdx)
{
# if defined(_WIN32)
    if (coreIdx >= sizeof(DWORD_PTR) * 8) // bits per byte
    {
        throw std::range_error("cannot currently handle more than 8*sizeof(void*) CPUs on Windows");
    }
    detail::win32_assert(SetThreadAffinityMask((HANDLE) handle, DWORD_PTR(1) << coreIdx) != 0);
# elif defined(USE_PTHREAD_SETAFFINITY)
    cpu_set cpuSet;
    cpuSet.set_cpu_flag(coreIdx);
    detail::posix_check(::pthread_setaffinity_np((pthread_t) handle, cpuSet.size(), cpuSet.data()));
# else
#  error Unsupported operating system.
# endif
}
#endif // _WIN32

#ifdef USE_PTHREAD_SETAFFINITY
static void
setThreadAttrAffinity(pthread_attr_t& attr, std::size_t coreIdx)
{
    cpu_set cpuSet;
    cpuSet.set_cpu_flag(coreIdx);
    detail::posix_check(::pthread_attr_setaffinity_np(&attr, cpuSet.size(), cpuSet.data()));
}
#endif // USE_PTHREAD_SETAFFINITY


#if defined(_WIN32)
struct win32_handle_deleter
{
    void
    operator ()(HANDLE handle) const noexcept
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
    pthread_t
    get(void) const noexcept
    {
        return handle_;
    }
    pthread_t
    release(void) noexcept
    {
        return std::exchange(handle_, { });
    }

    explicit
    operator bool(void) const noexcept
    {
        return handle_ != pthread_t{ };
    }

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
    pthread_handle&
    operator =(pthread_handle&& rhs) noexcept
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
        detail::posix_check(::pthread_attr_init(&attr));
    }
    ~PThreadAttr(void)
    {
        detail::posix_check(::pthread_attr_destroy(&attr));
    }
};
#else
# error Unsupported operating system.
#endif // _WIN32


#ifdef _WIN32
static std::atomic<unsigned> threadSquadCounter{ };
#endif // _WIN32


#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
static inline void pause(void)
{
    _mm_pause();
}
#else
static inline void pause(void)
{
    [[maybe_unused]] volatile int v = 0;
}
#endif // defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)

constexpr int spinCount = 4;
constexpr int spinRep = 2;
constexpr int pauseCount = 9;
constexpr int yieldCount = 6;

template <typename T>
bool wait_equal_exponential_backoff(std::atomic<T>& a, T oldValue)
{
    if (a.load(std::memory_order_relaxed) != oldValue) return true;
    for (int i = 0; i < (1 << pauseCount); ++i)
    {
        int n = 1;
        for (int j = 0; j < spinCount; ++j)
        {
            for (int r = 0; r < spinRep; ++r)
            {
                for (int k = 0; k < n; ++k)
                {
                    [[maybe_unused]] volatile int v = j + k;
                }
                if (a.load(std::memory_order_relaxed) != oldValue) return true;
            }
            n *= 2;
        }
        detail::pause();
    }
    for (int i = 0; i < (1 << yieldCount); ++i)
    {
        if (a.load(std::memory_order_relaxed) != oldValue) return true;
        std::this_thread::yield();
    }
    return false;
}

template <typename T>
T atomic_wait_while_equal(std::atomic<T>& a, T oldValue)
{
    if (!detail::wait_equal_exponential_backoff(a, oldValue))
    {
        while (a.load(std::memory_order_relaxed) == oldValue)
        {
            std::this_thread::yield();
        }
    }
    return a.load(std::memory_order_acquire);
}

template <typename T>
T wait_and_load(
    std::mutex& mutex, std::condition_variable& cv,
    std::atomic<T>& a, T oldValue)
{
    if (!detail::wait_equal_exponential_backoff(a, oldValue))
    {
        auto lock = std::unique_lock(mutex); // implicit acquire
        while (a.load(std::memory_order_relaxed) == oldValue)
        {
            cv.wait(lock); // implicit release/acquire
        }
        // implicit release in destructor of `lock`
    }
    return a.load(std::memory_order_acquire);
}

template <typename T>
T toggle_and_notify(
    std::mutex& mutex, std::condition_variable& cv,
    std::atomic<T>& a)
{
    T oldValue = a.load(std::memory_order_relaxed);
    T newValue = 1 ^ oldValue;
    //std::atomic_thread_fence(std::memory_order_release);
    {
        auto lock = std::lock_guard(mutex); // implicit acquire
        a.store(newValue, std::memory_order_release);
        // implicit release in destructor of `lock`
    }
    cv.notify_one();
    return oldValue;
}

struct thread_notify_data
{
    std::mutex mutex;
    std::condition_variable cv;
};

struct thread_sync_data
{
    std::atomic<int> newSense;
    std::atomic<int> sense;
    int threadIdx;
    int numSubthreads;
    thread_squad_impl& impl;

    thread_sync_data(thread_squad_impl& _impl) noexcept
        : newSense(0),
          sense(0),
          numSubthreads(0),
          impl(_impl)
    {
    }
};

static int
next_substride(int stride)
{
    return (stride + 7) / 8;
    /*if (stride % 3 == 0)
    {
        return stride /= 3;
    }
    if (stride % 2 == 0)
    {
        return stride /= 2;
    }
    return (stride + 1) / 2;*/
}


//
//      Threads:            X X X X X X X X X     X X X X X X X X     X X X X X X X     X X X X X X
//                          X X X X X X X X X     X X X X X X X X     X X X X X X X     X X X X X X
//                          X X X X X X X X X     X X X X X X X X     X X X X X X X     X X X X X X
//
//      Wait sequence:      w-^   w-^   w-^       w-^ w-^ w-^ w-^     w-^ w-^ w-^       w-^   w-^
//                          w---^ w---^ w---^     w---^   w---^       w---^   w---^     w---^ w---^
//                          w-----^               w-------^           w-------^         w-----^
//                          w-----------^
//
//      Subthread counts:   9 1 1 3 1 1 3 1 1     8 1 2 1 4 1 2 1     8 1 2 1 4 1 2     6 1 1 3 1 1
//


static void
init_range(
    aligned_buffer<thread_sync_data, cache_line_alignment>& threadSyncData,
    int first, int last, int stride)
{
    if (stride != 1)
    {
        int substride = next_substride(stride);
        for (int i = first; i < last; i += substride)
        {
            init_range(threadSyncData, i, std::min(i + substride, last), substride);
        }
    }
    threadSyncData[first].numSubthreads = stride;
}

static void
init(
    aligned_buffer<thread_sync_data, cache_line_alignment>& threadSyncData)
{
    int numThreads = gsl::narrow<int>(threadSyncData.size());
    for (int i = 0; i < numThreads; ++i)
    {
        threadSyncData[i].threadIdx = i;
    }
    init_range(threadSyncData, 0, numThreads, numThreads);
}

static void
notify_thread_impl(
    aligned_buffer<thread_sync_data, cache_line_alignment>& threadSyncData,
    aligned_buffer<thread_notify_data, cache_line_alignment>& threadNotifyData,
    int first, int last, int stride)
{
    while (stride != 1)
    {
        int substride = next_substride(stride);
        for (int i = first + substride; i < last; i += substride)
        {
#ifdef DEBUG_WAIT_CHAIN
            std::printf("notifying: %d -> %d\n", first, i);
            std::fflush(stdout);
#endif // DEBUG_WAIT_CHAIN
            toggle_and_notify(threadNotifyData[i].mutex, threadNotifyData[i].cv, threadSyncData[i].newSense);
        }
        last = std::min(first + substride, last);
        stride = substride;
    }
}

static void
notify_thread(
    aligned_buffer<thread_sync_data, cache_line_alignment>& threadSyncData,
    aligned_buffer<thread_notify_data, cache_line_alignment>& threadNotifyData,
    int threadIdx, int concurrency)
{
    int stride = threadSyncData[threadIdx].numSubthreads;
    notify_thread_impl(threadSyncData, threadNotifyData, threadIdx, std::min(threadIdx + stride, concurrency), stride);
}

static void
wait_for_thread_impl(
    aligned_buffer<thread_sync_data, cache_line_alignment>& threadSyncData,
    int first, int last, int stride)
{
    int substride = next_substride(stride);
    if (stride != 1)
    {
        wait_for_thread_impl(threadSyncData, first, std::min(first + substride, last), substride);
    }
    for (int i = first + substride; i < last; i += substride)
    {
        int newSense = threadSyncData[i].newSense.load(std::memory_order_relaxed);
        int oldSense = 1 ^ newSense;
        atomic_wait_while_equal(threadSyncData[i].sense, oldSense);
#ifdef DEBUG_WAIT_CHAIN
        std::printf("waited: %d <- %d\n", first, i);
        std::fflush(stdout);
#endif // DEBUG_WAIT_CHAIN
    }
}

static void
wait_for_thread(
    aligned_buffer<thread_sync_data, cache_line_alignment>& threadSyncData,
    int threadIdx, int concurrency)
{
    int stride = threadSyncData[threadIdx].numSubthreads;
    wait_for_thread_impl(threadSyncData, threadIdx, std::min(threadIdx + stride, concurrency), stride);
}


#ifdef THREAD_PINNING_SUPPORTED
static std::size_t
get_hardware_thread_id(int threadIdx, int maxNumHardwareThreads, gsl::span<int const> hardwareThreadMappings)
{
    auto subidx = threadIdx % maxNumHardwareThreads;
    return gsl::narrow<std::size_t>(
        !hardwareThreadMappings.empty() ? hardwareThreadMappings[subidx]
        : subidx);
}
#endif // THREAD_PINNING_SUPPORTED


#if defined(_WIN32)
static unsigned __stdcall
thread_squad_thread_func(void* data);
#elif defined(USE_PTHREAD)
static void*
thread_squad_thread_func(void* data);
#else
# error Unsupported operating system.
#endif // _WIN32

struct thread_squad_impl : thread_squad_impl_base
{
        // thread handles
#if defined(_WIN32)
    std::unique_ptr<detail::win32_handle[]> handles;
#elif defined(USE_PTHREAD)
    std::unique_ptr<detail::pthread_handle[]> handles;
#else
# error Unsupported operating system.
#endif // _WIN32

        // synchronization data
    aligned_buffer<thread_notify_data, cache_line_alignment> threadNotifyData;
    aligned_buffer<thread_sync_data, cache_line_alignment> threadSyncData;

        // task-specific data
    std::function<void(thread_squad::task_context)> action;
    int concurrency;
    bool terminationRequested;

#ifdef _WIN32
    unsigned threadSquadId; // for runtime thread identification during debugging
#endif // _WIN32
    bool needLaunch;
#ifdef USE_PTHREAD_SETAFFINITY
    bool pinToHardwareThreads;
    int maxNumHardwareThreads;
    std::vector<int> hardwareThreadMappings;
#endif // USE_PTHREAD_SETAFFINITY

    thread_squad_impl(thread_squad::params const& p)
        : thread_squad_impl_base{ p.num_threads },
          threadNotifyData(gsl::narrow<std::size_t>(p.num_threads)),
          threadSyncData(gsl::narrow<std::size_t>(p.num_threads), std::in_place, *this),
#ifdef _WIN32
          threadSquadId(threadSquadCounter++),
#endif // _WIN32
          needLaunch(true)
    {
        init(threadSyncData);

#if defined(_WIN32)
            // Create threads suspended; set core affinity if desired.
        handles = std::make_unique<detail::win32_handle[]>(gsl::narrow<std::size_t>(p.num_threads));
        DWORD threadCreationFlags = p.pin_to_hardware_threads ? CREATE_SUSPENDED : 0;
        for (int i = 0; i < p.num_threads; ++i)
        {
            auto handle = detail::win32_handle(
                (HANDLE) ::_beginthreadex(NULL, 0, thread_squad_thread_func, &threadSyncData[i], threadCreationFlags, nullptr));
            detail::win32_assert(handle != nullptr);
            if (p.pin_to_hardware_threads)
            {
                detail::setThreadAffinity(handle.get(), get_hardware_thread_id(i, p.max_num_hardware_threads, p.hardware_thread_mappings));
            }
            handles[i] = std::move(handle);
        }
#elif defined(USE_PTHREAD_SETAFFINITY)
            // Store hardware thread mappings for delayed thread creation.
        pinToHardwareThreads = p.pin_to_hardware_threads;
        maxNumHardwareThreads = p.max_num_hardware_threads;
        hardwareThreadMappings.assign(p.hardware_thread_mappings.begin(), p.hardware_thread_mappings.end());
#endif
#ifdef USE_PTHREAD
        handles = std::make_unique<detail::pthread_handle[]>(gsl::narrow<std::size_t>(p.num_threads));
#endif // USE_PTHREAD
    }

    thread_squad::task_context make_context(int threadIdx) noexcept
    {
        return { *this, threadIdx };
    }
};


static void
launch_threads(thread_squad_impl& self)
noexcept // We cannot really handle thread launch failure.
{
    gsl_Expects(self.needLaunch);
    self.needLaunch = false;

#if defined(_WIN32)
        // Resume threads.
    for (int i = 0; i < self.numThreads; ++i)
    {
        DWORD result = ::ResumeThread(self.handles[i].get());
        detail::win32_assert(result != DWORD(-1));
    }
#elif defined(USE_PTHREAD)
        // Create threads; set core affinity if desired.
    for (int i = 0; i < self.numThreads; ++i)
    {
        PThreadAttr attr;
# if defined(USE_PTHREAD_SETAFFINITY)
        if (self.pinToHardwareThreads)
        {
            detail::setThreadAttrAffinity(attr.attr, get_hardware_thread_id(i, self.maxNumHardwareThreads, self.hardwareThreadMappings));
        }
# endif // defined(USE_PTHREAD_SETAFFINITY)
        auto handle = pthread_t{ };
        detail::posix_check(::pthread_create(&handle, &attr.attr, thread_squad_thread_func, &self.threadSyncData[i]));
        self.handles[i] = pthread_handle(handle);
    }

# if defined(USE_PTHREAD_SETAFFINITY)
        // Release thread mapping data.
    self.hardwareThreadMappings = { };
# endif // defined(USE_PTHREAD_SETAFFINITY)
#else
# error Unsupported operating system.
#endif
}

static void
join_threads(thread_squad_impl& self)
noexcept // We cannot really handle join failure.
{
#if defined(_WIN32)
    gsl_Expects(!self.needLaunch);

        // Join threads.
    for (int i = 0; i < self.numThreads; )
    {
            // Join as many threads as possible in a single syscall. It is possible that this can be improved with a tree-like wait chain.
        HANDLE lhandles[MAXIMUM_WAIT_OBJECTS];
        int n = std::min(self.numThreads - i, MAXIMUM_WAIT_OBJECTS);
        for (int j = 0; j < n; ++j)
        {
            lhandles[j] = self.handles[i + j].get();
        }
        DWORD result = ::WaitForMultipleObjects(n, lhandles, TRUE, INFINITE);
        detail::win32_assert(result != WAIT_FAILED);
        i += n;
    }
#elif defined(USE_PTHREAD)
    if (!self.needLaunch)
    {
            // Join threads. It is possible that this can be improved with a tree-like wait chain.
        for (int i = 0; i < self.numThreads; ++i)
        {
            detail::posix_check(::pthread_join(self.handles[i].get(), NULL));
            self.handles[i].release();
        }
    }
#else
# error Unsupported operating system.
#endif

    self.handles.reset();
}

static void
run(thread_squad_impl& self,
    std::function<void(thread_squad::task_context)> action, int concurrency, bool join)
noexcept // We cannot really handle exceptions here.
{
    gsl_Expects(action || join);

    self.terminationRequested = join;
    self.concurrency = concurrency;
    self.action = std::move(action);

#ifdef USE_PTHREAD
    bool wakeToJoin = join && !self.needLaunch;
#else // ^^^ USE_PTHREAD ^^^ / vvv !USE_PTHREAD vvv
        // The only clean way to quit never-resumed threads on Windows is to resume them and let them run to the end.
    bool wakeToJoin = join;
#endif // USE_PTHREAD

    int oldSense = 0;

    if ((self.action && concurrency != 0)
        || wakeToJoin)
    {
            // No need to notify threads for nothing if none have been launched yet.
        if (self.needLaunch)
        {
            int numThreadsToWake = join ? self.numThreads : concurrency;
            for (int i = 0; i < numThreadsToWake; ++i)
            {
                self.threadSyncData[i].newSense.store(1, std::memory_order_relaxed);
            }
            detail::launch_threads(self);
        }
        else
        {
            oldSense = toggle_and_notify(self.threadNotifyData[0].mutex, self.threadNotifyData[0].cv, self.threadSyncData[0].newSense);
#ifdef DEBUG_WAIT_CHAIN
            std::printf("notifying: general -> 0\n");
            std::fflush(stdout);
#endif // DEBUG_WAIT_CHAIN
        }
    }

    if (join)
    {
        detail::join_threads(self);
    }
    else
    {
        detail::atomic_wait_while_equal(self.threadSyncData[0].sense, oldSense);
#ifdef DEBUG_WAIT_CHAIN
        std::printf("waited: general <- 0\n");
        std::fflush(stdout);
#endif // DEBUG_WAIT_CHAIN
    }
}

    // Like the parallel overloads of the standard algorithms, we terminate (implicitly) if an exception is thrown
    // by a task because the semantics of exceptions in multiplexed actions are unclear.
static void
run_task_on_thread(std::function<void(thread_squad::task_context)> action, thread_squad::task_context arg) noexcept
{
    action(arg);
}

static void
run_thread(thread_sync_data& syncData, thread_notify_data& notifyData)
{
    bool justWoken = true;
    do
    {
        auto oldSense = syncData.sense.load(std::memory_order_relaxed);
        auto newSense = wait_and_load(notifyData.mutex, notifyData.cv, syncData.newSense, oldSense);

        if (justWoken)
        {
            justWoken = false;
        }
        else
        {
            int numThreadsToWake = syncData.impl.terminationRequested ? syncData.impl.numThreads : syncData.impl.concurrency;
            notify_thread(syncData.impl.threadSyncData, syncData.impl.threadNotifyData, syncData.threadIdx, numThreadsToWake);
        }

        if (syncData.impl.action && syncData.threadIdx < syncData.impl.concurrency)
        {
            detail::run_task_on_thread(syncData.impl.action, syncData.impl.make_context(syncData.threadIdx));
        }

        if (!syncData.impl.terminationRequested)
        {
            wait_for_thread(syncData.impl.threadSyncData, syncData.threadIdx, syncData.impl.concurrency);
#ifdef DEBUG_WAIT_CHAIN
            std::printf("signaling: %d\n", syncData.threadIdx);
            std::fflush(stdout);
#endif // DEBUG_WAIT_CHAIN
            syncData.sense.store(newSense, std::memory_order_release);
        }

    } while (!syncData.impl.terminationRequested);
}

#if defined(_WIN32)
static unsigned __stdcall
thread_squad_thread_func(void* vdata)
{
    thread_sync_data& data = *static_cast<thread_sync_data*>(vdata);
    {
        wchar_t buf[64];
        std::swprintf(buf, sizeof buf / sizeof(wchar_t), L"sysmakeshift thread squad #%u, thread %d", // TODO: %u?
            data.impl.threadSquadId, data.threadIdx);
        detail::setCurrentThreadDescription(buf);
    }

    detail::run_thread(data, data.impl.threadNotifyData[data.threadIdx]);

    return 0;
}
#elif defined(USE_PTHREAD)
static void*
thread_squad_thread_func(void* vdata)
{
    thread_sync_data& data = *static_cast<thread_sync_data*>(vdata);

    detail::run_thread(data, data.impl.threadNotifyData[data.threadIdx]);

    return nullptr;
}
#else
# error Unsupported operating system.
#endif


void
thread_squad_impl_deleter::operator ()(thread_squad_impl_base* base)
{
    auto impl = static_cast<thread_squad_impl*>(base);
    auto memGuard = std::unique_ptr<thread_squad_impl>(impl);

        // Join threads only if they haven't already been joined.
    if (impl->handles)
    {
        detail::run(*impl, { }, 0, true);
    }
}


} // namespace detail


detail::thread_squad_handle
thread_squad::create(thread_squad::params p)
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

    return detail::thread_squad_handle(new detail::thread_squad_impl(p));
}

void
thread_squad::do_run(std::function<void(task_context)> action, int concurrency, bool join)
{
    if (concurrency == 0) concurrency = handle_->numThreads;

    detail::run(static_cast<detail::thread_squad_impl&>(*handle_), std::move(action), concurrency, join);
}


} // namespace sysmakeshift
