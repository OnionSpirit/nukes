#include <gtest/gtest.h>

#include <thread>
#include <algorithm>
#include <vector>
#include <functional>


#include "nukes/memory/atomic_lifo.h"
#include "nukes/memory/atomic_fifo.h"
#include "nukes/memory/atomic_freelist.h"
#include "nukes/memory/atomic_bucketlist.h"

#include "nukes/mpmc_queue.h"
#include "nukes/mpsc_queue.h"
#include "nukes/atomic_stack.h"
#include "nukes/atomic_ringbuf.h"


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

    /* void TearDown() override { */

    /*     ASSERT_TRUE(core.empty()); */
    /*     JunkYard::showAllMsgs(JunkYard::getLoggingLvl()); */
    /*     Test_ID::Reset_ID_counters(); */
    /* } */

public:

    static inline constexpr size_t thread_count = 12;
    static inline constexpr size_t data_volume = 65535;
    static inline constexpr size_t stack_data_size = thread_count * (data_volume / 2);

    template <typename containerT>
        static inline void thr_container_walker(containerT& container) {

        typedef uint8_t data_t;
        typedef nukes::details::misc::fn_forward_t<data_t> data_forward_t;

        static constexpr bool is_contailner = requires(containerT cont, data_forward_t fwd, data_t& pll) {
            { cont.push(fwd) } -> std::same_as<bool>;
            { cont.pop(pll) } -> std::same_as<bool>;
        };

        int arr [data_volume] = {};
        int arr_i = 0;

        uint8_t k = 0;
        for (int i =0; i < data_volume; ++i) {
            if (i % 2 == 0) {
                bool res = container.push(i);
            } else {
                bool res = container.pop(k);
                arr[arr_i++] = k;
            }
        }

        for (int i =0; i < arr_i; ++i) {
            bool res = container->push(arr[i]);
        }
    }

    template <typename mempoolT>
        static inline void thr_mempool_walker(mempoolT& mempool) {

        typedef uint8_t data_t;
        typedef data_t*& data_forward_t;

        static constexpr bool is_contailner = requires(mempoolT cont, data_forward_t fwd) {
            { cont.capture(fwd) } -> std::same_as<bool>;
            { cont.sync(fwd) } -> std::same_as<bool>;
        };

        ulong arr [data_volume] = {};
        int arr_i = 0;

        uint8_t* mem { nullptr };
        while (mempool.capture(mem) and arr_i <= data_volume) {
            arr[arr_i] = (ulong)mem;
            ++arr_i;
        }

        for (int i =0; i < arr_i; ++i) {
            mem = (uint8_t*)(arr[i]);
            bool res = mempool.sync(mem);
            EXPECT_TRUE(res);
        }
    }

};
