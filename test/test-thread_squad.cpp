
#include <sysmakeshift/thread_squad.hpp>

#include <thread>
#include <mutex>
#include <unordered_set>
#include <unordered_map>

#include <catch2/catch.hpp>


#if defined(_WIN32) || defined(__linux__)
# define THREAD_PINNING_SUPPORTED
#endif // defined(_WIN32) || defined(__linux__)



TEST_CASE("thread_squad")
{
    GENERATE(range(0, 10)); // repetitions

    int numThreads = GENERATE(range(0, 10), 10, 48, 50);
    CAPTURE(numThreads);

    unsigned numActualThreads = static_cast<unsigned>(numThreads);
    if (numActualThreads == 0)
    {
        numActualThreads = std::thread::hardware_concurrency();
    }

    std::mutex mutex;
    auto threadId_Count = std::unordered_map<std::thread::id, int>{ };
    auto threadIndex_Count = std::unordered_map<int, int>{ };
	int count = 0;

    auto params = sysmakeshift::thread_squad::params{
        /*.num_threads = */ numThreads
    };
#ifdef THREAD_PINNING_SUPPORTED
    params.pin_to_hardware_threads = true;
#endif // !THREAD_PINNING_SUPPORTED

    auto action = [&]
    (sysmakeshift::thread_squad::task_context ctx)
    {
        auto lock = std::unique_lock<std::mutex>(mutex);
        ++threadId_Count[std::this_thread::get_id()];
        ++threadIndex_Count[ctx.thread_index()];
		++count;
    };

    SECTION("single task")
    {
        sysmakeshift::thread_squad(params).run(action);
#ifdef THREAD_PINNING_SUPPORTED
	    CHECK(threadId_Count.size() == static_cast<std::size_t>(numActualThreads));
#endif // THREAD_PINNING_SUPPORTED
    	CHECK(threadIndex_Count.size() == static_cast<std::size_t>(numActualThreads));
    }

    SECTION("fixed number of tasks")
    {
        int numTasks = GENERATE(0, 1, 2, 5, 10, 20);
        CAPTURE(numTasks);

	    auto threadSquad = sysmakeshift::thread_squad(params);
    	for (int i = 0; i < numTasks; ++i)
	    {
    	    threadSquad.run(action);
	    }

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

    SECTION("fixed number of tasks")
    {
	    auto threadSquad = sysmakeshift::thread_squad(params);
		for (int i = 1; i <= int(numActualThreads); ++i)
		{
			CAPTURE(i);
			threadSquad.run(action, i);
		}

		CHECK(count == numActualThreads * (numActualThreads + 1) / 2);
	}
}

