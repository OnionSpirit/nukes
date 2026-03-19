#include <unordered_set>
#include "fixture.h"


TEST_F(atomics, do_check_atomic_freelist_consistancy) {

    constexpr std::size_t len = data_volume;

    typedef nukes::dynamic::mpmc_freelist<int> container_t;
    container_t container{};

    for (int i =0; i < thread_count; ++i)
        threads.emplace_back(thr_mempool_walker<container_t>, std::ref(container));

    for (auto& e : threads)
        e.join();
    threads.clear();

    std::this_thread::yield();

    std::vector<ulong> allocated_adresses {};
    allocated_adresses.reserve(len);

    for (int i =0; i < len; ++i) {
        int* mem { nullptr };
        container.capture(mem);
        allocated_adresses.emplace_back(reinterpret_cast<ulong>(mem));
    }

    ASSERT_EQ(allocated_adresses.size(), len);

    std::set<ulong> unique_addresses;
    for (auto address : allocated_adresses)
        EXPECT_TRUE(unique_addresses.insert(address).second);
}

TEST_F(atomics, do_check_mpmc_consistancy) {

    constexpr std::size_t len = data_volume;

    typedef nukes::bounded::mpmc_queue<int> container_t;
    container_t container{len};
    container.clear();

    for (int cpu_num = 0, i =0; i < thread_count; ++cpu_num, ++i) {
        auto thread_handle = threads
            .emplace_back(thr_mpmc_container_walker<container_t>, std::ref(container))
            .native_handle();
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_num % thread_count, &cpuset);
        pthread_setaffinity_np(thread_handle, sizeof(cpu_set_t), &cpuset);
    }

    for (auto& e : threads)
        e.join();
    threads.clear();

    std::vector<int> interactive_arr;
    interactive_arr.reserve(len);

    for (int interactive =0, i =0; i < len; ++i) {
        container.pop(interactive);
        interactive_arr.emplace_back(interactive);
    }

    ASSERT_EQ(interactive_arr.size(), len);

    std::unordered_set<int> thread_ids;

    for (auto& el : interactive_arr) {
        if (el == 0) continue;
        thread_ids.insert(el);
    }

    ASSERT_EQ(thread_ids.size(), thread_count);
}

TEST_F(atomics, do_check_dynamic_mpmc_consistancy) {

    constexpr std::size_t len = data_volume;

    typedef nukes::dynamic::mpmc_queue<int> container_t;
    container_t container{};

    for (int cpu_num = 0, i =0; i < thread_count; ++cpu_num, ++i) {
        auto thread_handle = threads
            .emplace_back(thr_mpmc_container_walker<container_t>, std::ref(container))
            .native_handle();
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_num % thread_count, &cpuset);
        pthread_setaffinity_np(thread_handle, sizeof(cpu_set_t), &cpuset);
    }

    for (auto& e : threads)
        e.join();
    threads.clear();

    std::this_thread::yield();

    std::vector<int> interactive_arr;
    interactive_arr.reserve(len);

    for (int interactive =0, i =0; i < len; ++i) {
        container.pop(interactive);
        interactive_arr.emplace_back(interactive);
    }

    ASSERT_EQ(interactive_arr.size(), len);

    std::unordered_set<int> thread_ids;

    for (auto& el : interactive_arr) {
        if (el == 0) continue;
        thread_ids.insert(el);
    }

    ASSERT_EQ(thread_ids.size(), thread_count);
}

TEST_F(atomics, do_check_dynamic_mpsc_consistancy) {

    constexpr std::size_t len = data_volume;

    typedef nukes::dynamic::mpmc_queue<int> container_t;
    container_t container{};

    for (int i =0; i < thread_count; ++i)
        threads.emplace_back(thr_mpsc_container_walker<container_t>, std::ref(container));

    for (auto& e : threads)
        e.join();
    threads.clear();

    std::this_thread::yield();

    std::vector<int> interactive_arr;
    interactive_arr.reserve(len);

    for (int interactive =0, i =0; i < len; ++i) {
        container.pop(interactive);
        interactive_arr.emplace_back(interactive);
    }

    ASSERT_EQ(interactive_arr.size(), len);

    std::unordered_set<int> thread_ids;

    for (auto& el : interactive_arr) {
        if (el == 0) continue;
        thread_ids.insert(el);
    }

    ASSERT_EQ(thread_ids.size(), thread_count);
}

TEST_F(atomics, do_check_spsc_consistancy) {

    constexpr std::size_t len = data_volume;

    typedef nukes::bounded::spsc_queue<int> container_t;
    container_t container{len};

    for (int i =0; i < 1; ++i)
        threads.emplace_back(thr_spsc_container_walker<container_t>, std::ref(container));

    for (auto& e : threads)
        e.join();
    threads.clear();

    std::vector<int> interactive_arr;
    interactive_arr.reserve(len);

    for (int interactive =0, i =0; i < len; ++i) {
        container.pop(interactive);
        interactive_arr.emplace_back(interactive);
    }

    ASSERT_EQ(interactive_arr.size(), len);

    for (int i =0; i < len; ++i)
        EXPECT_EQ(i, interactive_arr[i]);
}

TEST_F(atomics, DISABLED_do_check_dynamic_mpmc_batch) {

    constexpr std::size_t len = data_volume;

    typedef nukes::dynamic::mpmc_queue<int> container_t;
    container_t container{};

    for (int i =0; i < thread_count; ++i)
        threads.emplace_back(thr_mpmc_container_walker<container_t>, std::ref(container));

    for (auto& e : threads)
        e.join();
    threads.clear();

    std::this_thread::yield();

    std::vector<int> interactive_arr;
    interactive_arr.reserve(len);

    for (auto& interactive : container.pop_batch()) {
        interactive_arr.emplace_back(interactive);
    }

    ASSERT_TRUE(container.empty());
    ASSERT_EQ(interactive_arr.size(), len);

    std::unordered_set<int> thread_ids;

    for (auto& el : interactive_arr) {
        if (el == 0) continue;
        thread_ids.insert(el);
    }

    ASSERT_EQ(thread_ids.size(), thread_count);
}

TEST_F(atomics, DISABLED_do_check_bounded_mpmc_batch) {

    constexpr std::size_t len = data_volume;

    typedef nukes::bounded::mpmc_queue<int> container_t;
    container_t container{len};
    container.clear();

    for (int i =0; i < thread_count; ++i)
        threads.emplace_back(thr_mpmc_container_walker<container_t>, std::ref(container));

    for (auto& e : threads)
        e.join();
    threads.clear();

    std::this_thread::yield();

    std::vector<int> interactive_arr;
    interactive_arr.reserve(len);

    for (auto& interactive : container.pop_batch()) {
        interactive_arr.emplace_back(interactive);
    }

    ASSERT_TRUE(container.empty());
    ASSERT_EQ(interactive_arr.size(), len);

    std::unordered_set<int> thread_ids;

    for (auto& el : interactive_arr) {
        if (el == 0) continue;
        thread_ids.insert(el);
    }

    ASSERT_EQ(thread_ids.size(), thread_count);
}
