#include <gtest/gtest.h>

#include <thread>
#include <algorithm>
#include <vector>
#include <functional>
#include <latch>


#include "include/nukes/dynamic/mpmc_freelist.h"

#include "include/nukes/dynamic/mpmc_queue.h"
#include "include/nukes/dynamic/mpsc_queue.h"
#include "include/nukes/bounded/atomic_stack.h"
#include "include/nukes/bounded/mpmc_queue.h"
#include "include/nukes/bounded/spsc_queue.h"


static inline std::string current_test_name {};

static void sighandler(int signum) {
    std::cout << "memory corruption case detected for '" + current_test_name << "' test"<< std::endl;
    exit(EXIT_FAILURE);
}

class atomics : public ::testing::Test {

protected:

    std::vector<std::thread> threads;

    void SetUp() override {
        current_test_name = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    }

    void TearDown() override {
        threads.clear();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

public:

    static inline constexpr size_t thread_count = 6;
    static inline constexpr size_t data_volume = 65536 * 8;
    static inline constexpr size_t stack_data_size = thread_count * (data_volume / 2);

    template <typename containerT>
    static void thr_mpmc_container_walker(containerT& container, std::latch& accessor) {

        typedef int data_t;
        typedef nukes::details::misc::argument_t<data_t> data_forward_t;

        static constexpr bool is_contailner = requires(containerT cont, data_forward_t fwd, data_t& pll) {
            { cont.push(fwd) } -> std::same_as<bool>;
            { cont.pop(pll) } -> std::same_as<bool>;
        };

        std::vector<int> arr = {};
        arr.reserve(data_volume / thread_count);

        // NOTE: Collecting thread ID to use it as data in the container. This will allow to check thread access consistency
        std::stringstream thread_alias_ss{};
        thread_alias_ss << std::this_thread::get_id();
        std::string _thread_alias_string = thread_alias_ss.str();
        std::ranges::reverse(_thread_alias_string);
        int thread_id = std::stoi(_thread_alias_string.substr(0, 6));

        // NOTE: Waiting for all threads started
        accessor.arrive_and_wait();

        // NOTE: Pushing our thread ID then reading some thread ID and storing it into local container
        // NOTE: Each thread makes same count of push and pop operations
        for (int interactive =0, i =0; i < data_volume / thread_count; ++i) {
            while (not container.push(thread_id)) {}
            while (not container.pop(interactive)) {}
            arr.emplace_back(interactive);
        }

        // NOTE: Storing all popped IDs from local container to common container
        for (auto& el : arr)
            while (not container.push(el)) {}
    }

    template <typename containerT>
    static void thr_spsc_container_walker(containerT& container) {

        typedef int data_t;
        typedef nukes::details::misc::argument_t<data_t> data_forward_t;

        static constexpr bool is_contailner = requires(containerT cont, data_forward_t fwd, data_t& pll) {
            { cont.push(fwd) } -> std::same_as<bool>;
            { cont.pop(pll) } -> std::same_as<bool>;
        };

        // NOTE: Pushing counter as data
        for (int i =0; i < data_volume; ++i)
            container.push(i);
    }

    template <typename containerT>
    static void thr_mpsc_container_walker(containerT& container, std::latch& accessor) {

        typedef int data_t;
        typedef nukes::details::misc::argument_t<data_t> data_forward_t;

        static constexpr bool is_contailner = requires(containerT cont, data_forward_t fwd, data_t& pll) {
            { cont.push(fwd) } -> std::same_as<bool>;
            { cont.pop(pll) } -> std::same_as<bool>;
        };

        // NOTE: Collecting thread ID to use it as data in the container. This will allow to check thread access consistency
        std::stringstream thread_alias_ss{};
        thread_alias_ss << std::this_thread::get_id();
        std::string _thread_alias_string = thread_alias_ss.str();
        std::ranges::reverse(_thread_alias_string);
        int thread_id = std::stoi(_thread_alias_string.substr(0, 6));

        // NOTE: Waiting for all threads started
        accessor.arrive_and_wait();

        // NOTE: Pushing local thread ID as data
        for (int i =0; i < data_volume / thread_count; ++i)
            while (not container.push(thread_id)) {};
    }

    template <typename mempoolT>
    static void thr_mempool_walker(mempoolT& mempool, std::latch& accessor) {

        typedef int data_t;
        typedef data_t*& data_forward_t;

        static constexpr bool is_container = requires(mempoolT cont, data_forward_t fwd) {
            { cont.capture(fwd) } -> std::same_as<bool>;
            { cont.sync(fwd) } -> std::same_as<bool>;
        };

        ulong arr [data_volume] = {};
        int arr_i = 0;

        int* mem { nullptr };
        // NOTE: Waiting for all threads started
        accessor.arrive_and_wait();

        // NOTE: Allocating memory and collecting its pointers as integer value
        while (mempool.capture(mem) and arr_i <= data_volume) {
            arr[arr_i] = reinterpret_cast<ulong>(mem);
            ++arr_i;
        }

        // NOTE: Synchronizing all allocated memory and checking that synchronization is succeed
        for (int i =0; i < arr_i; ++i) {
            mem = reinterpret_cast<int *>(arr[i]);
            bool res = mempool.sync(mem);
            EXPECT_TRUE(res);
        }
    }

};
