#include <gtest/gtest.h>

#include <thread>
#include <algorithm>
#include <vector>
#include <functional>


#include "include/nukes/dynamic/atomic_freelist.h"

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
    struct sigaction sa;

    void SetUp() override {
        // sa.sa_handler = sighandler;
        // sigaction(SIGSEGV, &sa, NULL);
        // sigaction(SIGTERM, &sa, NULL);
        // sigaction(SIGABRT, &sa, NULL);

        current_test_name = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    }

    void TearDown() override {
        threads.clear();
        std::this_thread::yield();
    }

    /* void TearDown() override { */

    /*     ASSERT_TRUE(core.empty()); */
    /*     JunkYard::showAllMsgs(JunkYard::getLoggingLvl()); */
    /*     Test_ID::Reset_ID_counters(); */
    /* } */

public:

    static inline constexpr size_t thread_count = 6;
    static inline constexpr size_t data_volume = 65536 * 8;
    static inline constexpr size_t stack_data_size = thread_count * (data_volume / 2);

    template <typename containerT>
        static inline void thr_mpmc_container_walker(containerT& container) {

        typedef int data_t;
        typedef nukes::details::misc::fn_forward_t<data_t> data_forward_t;

        static constexpr bool is_contailner = requires(containerT cont, data_forward_t fwd, data_t& pll) {
            { cont.push(fwd) } -> std::same_as<bool>;
            { cont.pop(pll) } -> std::same_as<bool>;
        };

        std::vector<int> arr = {};
        arr.reserve(data_volume / thread_count);

        std::stringstream _thread_alias_ss{};
        _thread_alias_ss << std::this_thread::get_id();
        std::string _thread_alias_string = _thread_alias_ss.str();
        std::ranges::reverse(_thread_alias_string);
        int _thread_id = std::stoi(_thread_alias_string.substr(0, 6));

        for (int interactive =0, i =0; i < data_volume / thread_count; ++i) {
            while (not container.push(_thread_id)) {}
            if (container.pop(interactive))
                arr.emplace_back(interactive);
        }

        for (auto& el : arr) {
            container.push(el);
        }
    }

    template <typename containerT>
        static inline void thr_spsc_container_walker(containerT& container) {

        typedef int data_t;
        typedef nukes::details::misc::fn_forward_t<data_t> data_forward_t;

        static constexpr bool is_contailner = requires(containerT cont, data_forward_t fwd, data_t& pll) {
            { cont.push(fwd) } -> std::same_as<bool>;
            { cont.pop(pll) } -> std::same_as<bool>;
        };

        for (int i =0; i < data_volume; ++i)
            container.push(i);
    }

    template <typename containerT>
    static inline void thr_mpsc_container_walker(containerT& container) {

        typedef int data_t;
        typedef nukes::details::misc::fn_forward_t<data_t> data_forward_t;

        static constexpr bool is_contailner = requires(containerT cont, data_forward_t fwd, data_t& pll) {
            { cont.push(fwd) } -> std::same_as<bool>;
            { cont.pop(pll) } -> std::same_as<bool>;
        };

        std::stringstream _thread_alias_ss{};
        _thread_alias_ss << std::this_thread::get_id();
        std::string _thread_alias_string = _thread_alias_ss.str();
        std::ranges::reverse(_thread_alias_string);
        int _thread_id = std::stoi(_thread_alias_string.substr(0, 6));

        for (int i =0; i < data_volume / thread_count; ++i)
            container.push(_thread_id);
    }

    template <typename mempoolT>
        static inline void thr_mempool_walker(mempoolT& mempool) {

        typedef int data_t;
        typedef data_t*& data_forward_t;

        static constexpr bool is_container = requires(mempoolT cont, data_forward_t fwd) {
            { cont.capture(fwd) } -> std::same_as<bool>;
            { cont.sync(fwd) } -> std::same_as<bool>;
        };

        ulong arr [data_volume] = {};
        int arr_i = 0;

        int* mem { nullptr };
        while (mempool.capture(mem) and arr_i <= data_volume) {
            arr[arr_i] = reinterpret_cast<ulong>(mem);
            ++arr_i;
        }

        for (int i =0; i < arr_i; ++i) {
            mem = reinterpret_cast<int *>(arr[i]);
            bool res = mempool.sync(mem);
            EXPECT_TRUE(res);
        }
    }

};
