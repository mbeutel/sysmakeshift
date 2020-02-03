
#include <sysmakeshift/thread_pool.hpp>

#include <thread>
#include <mutex>
#include <unordered_set>
#include <unordered_map>

#include <catch2/catch.hpp>


#if defined(_WIN32) || defined(__linux__)
# define THREAD_PINNING_SUPPORTED
#endif // defined(_WIN32) || defined(__linux__)



TEST_CASE("thread_pool can schedule single task")
{
    GENERATE(range(0, 20)); // repetitions

    int numThreads = GENERATE(0, 1, 3, 10, 50);
    unsigned numActualThreads = static_cast<unsigned>(numThreads);
    if (numActualThreads == 0)
    {
        numActualThreads = std::thread::hardware_concurrency();
    }

    std::mutex mutex;
    std::unordered_set<std::thread::id> threadIds;
    std::unordered_set<int> threadIndices;

    auto params = sysmakeshift::thread_pool::params{
        /*.num_threads = */ numThreads
    };
#ifdef THREAD_PINNING_SUPPORTED
    params.pin_to_hardware_threads = true;
#endif // !THREAD_PINNING_SUPPORTED

    auto action = [&]
    (sysmakeshift::thread_pool::task_context ctx)
    {
        auto lock = std::unique_lock<std::mutex>(mutex);
        threadIds.insert(std::this_thread::get_id());
        threadIndices.insert(ctx.thread_index());
    };

    SECTION("with synchronous completion")
    {
        sysmakeshift::thread_pool(params).run(action);
    }
    SECTION("with asynchronous completion")
    {
        auto completion = sysmakeshift::thread_pool(params).run_async(action);
        completion.wait();
    }

    CAPTURE(numThreads);

#ifdef THREAD_PINNING_SUPPORTED
    CHECK(threadIds.size() == static_cast<std::size_t>(numActualThreads));
#endif // THREAD_PINNING_SUPPORTED
    CHECK(threadIndices.size() == static_cast<std::size_t>(numActualThreads));
}

TEST_CASE("thread_pool can schedule multiple tasks")
{
    GENERATE(range(0, 10)); // repetitions

    int numThreads = GENERATE(0, 1, 3, 10, 50);
    unsigned numActualThreads = static_cast<unsigned>(numThreads);
    if (numActualThreads == 0)
    {
        numActualThreads = std::thread::hardware_concurrency();
    }

    int numTasks = GENERATE(0, 1, 2, 5, 10, 20);

    std::mutex mutex;
    std::unordered_map<std::thread::id, int> threadId_Count;
    std::unordered_map<int, int> threadIndex_Count;

    auto params = sysmakeshift::thread_pool::params{
        /*.num_threads = */ numThreads
    };
#ifdef THREAD_PINNING_SUPPORTED
    params.pin_to_hardware_threads = true;
#endif // !THREAD_PINNING_SUPPORTED

    auto action = [&]
    (sysmakeshift::thread_pool::task_context ctx)
    {
        auto lock = std::unique_lock<std::mutex>(mutex);
        ++threadId_Count[std::this_thread::get_id()];
        ++threadIndex_Count[ctx.thread_index()];
    };

    SECTION("with synchronous completion")
    {
        auto threadPool = sysmakeshift::thread_pool(params);
        for (int i = 0; i < numTasks; ++i)
        {
            threadPool.run(action);
        }
    }
    SECTION("with asynchronous completion")
    {
        std::vector<std::future<void>> completions;
        auto threadPool = sysmakeshift::thread_pool(params);
        for (int i = 0; i < numTasks; ++i)
        {
            completions.push_back(threadPool.run_async(action));
        }
        for (auto& completion : completions)
        {
            completion.wait();
        }
    }

    CAPTURE(numThreads);
    CAPTURE(numTasks);

    if (numTasks != 0)
    {
#ifdef THREAD_PINNING_SUPPORTED
        CHECK(threadId_Count.size() == static_cast<std::size_t>(numActualThreads));
#endif // THREAD_PINNING_SUPPORTED
        CHECK(threadIndex_Count.size() == static_cast<std::size_t>(numActualThreads));
    }
#ifdef THREAD_PINNING_SUPPORTED
    for (auto const& id_count : threadId_Count)
    {
        CHECK(id_count.second == numTasks);
    }
#endif // THREAD_PINNING_SUPPORTED
    for (auto const& index_count : threadIndex_Count)
    {
        CHECK(index_count.second == numTasks);
    }
}
