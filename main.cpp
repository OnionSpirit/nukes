#include <cassert>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <iterator>
#include <mutex>
#include <vector>
#include <thread>
#include <algorithm>

#include "nukes/mpmc_queue.h"
#include "nukes/mpsc_queue.h"
#include "nukes/atomic_stack.h"
#include "nukes/atomic_ringbuf.h"
// #include "atomic_stack_base.h"
#include "atomic_stack.h"
#include "nukes/memory/atomic_lifo.h"
#include "nukes/memory/atomic_fifo.h"
#include "nukes/memory/atomic_freelist.h"
#include "nukes/memory/atomic_bucketlist.h"

inline constexpr size_t thread_count = 12;
inline constexpr size_t data_volume = 1000000;
inline constexpr size_t stack_data_size = thread_count * (data_volume / 2);

// nukes::atomic_stack_base<uint8_t> g_stack{};
nukes::atomic_stack<uint8_t, data_volume> g_bounded_stack{};
nukes::memory::atomic_lifo<uint8_t, data_volume> g_freelist{};
nukes::memory::atomic_bucketlist<uint8_t> g_unbounded_freelist{};

aba_atomic_stack_base<uint8_t> g_aba_stack{};

nukes::mpmc_queue<uint8_t> g_mpmc_basic_q{};
//nukes::atomic_mpsc_queue_base<int> g_mpsc_basic_q{};

std::string g_stack_name = {"update atomic stack"};

static void sighandler(int signum) {
    std::cout << "memory corruption case detected for " + g_stack_name << std::endl;
    exit(EXIT_FAILURE);
}

// void thread_function() {
//     int arr [data_volume] = {};
//     int arr_i = 0;

//     uint8_t k = 0;
//     for (int i =0; i < data_volume; ++i) {
//         if (i % 2 == 0) {
//             bool res = g_stack.push_new(i);
//         } else {
//             bool res = g_stack.pop_new(k);
//             arr[arr_i++] = k;
//         }
//     }

//     for (int i =0; i < arr_i; ++i) {
//         bool res = g_stack.push_new(arr[i]);
//     }
// }


void bounded_stack_thread_function() {
    int arr [data_volume] = {};
    int arr_i = 0;

    uint8_t k = 0;
    for (int i =0; i < data_volume; ++i) {
        if (i % 2 == 0) {
            bool res = g_bounded_stack.push(i);
        } else {
            bool res = g_bounded_stack.pop(k);
            arr[arr_i++] = k;
        }
    }

    for (int i =0; i < arr_i; ++i) {
        bool res = g_bounded_stack.push(arr[i]);
    }
}


void freelist_thread_function() {
    ulong arr [data_volume] = {};
    int arr_i = 0;

    uint8_t* mem { nullptr };
    while (g_freelist.capture(mem)) {
        arr[arr_i] = (ulong)mem;
        ++arr_i;
    }

    for (int i =0; i < arr_i; ++i) {
        mem = (uint8_t*)(arr[i]);
        bool res = g_freelist.sync(mem);
    }

}


void aba_thread_function() {
    int arr [data_volume] = {};
    int arr_i = 0;

    uint8_t k = 0;
    for (int i =0; i < data_volume; ++i) {
        if (i % 2 == 0) {
            g_aba_stack.push_new(i);
        } else {
            g_aba_stack.pop_new(k);
            arr[arr_i++] = k;
        }
    }

    for (int i =0; i < arr_i; ++i) {
        g_aba_stack.push_new(arr[i]);
    }
}


int main() {

    auto start = std::chrono::steady_clock::now();

    bool res {};
    nukes::details::misc::meta_data m;
    m[0] = 1;
    std::cout << sizeof(nukes::details::misc::meta_data<3>) << std::endl;
    std::cout << (int)m[0] << std::endl;

    // std::cout << std::atomic<void*>::is_always_lock_free << std::endl;
    // std::cout << std::atomic<node_dptr>::is_always_lock_free << std::endl;
    // std::cout << std::atomic<node_ss_ptr>::is_always_lock_free << std::endl;
    // auto a = node_dptr{};
    // std::cout << sizeof(node_dptr) << std::endl;
    // std::cout << __atomic_is_lock_free(16, 0) << std::endl;

    struct sigaction sa;
    sa.sa_handler = sighandler;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);

    std::vector<std::thread> threads;
    std::vector<int> stack_contains {};
    uint8_t output = 0;
    int cnt {}, now_val {};

//    std::cout << "Check for A-B-A for upgraded atomic stack" << std::endl;
//
//    threads.reserve(thread_count);
//    for (int i =0; i < thread_count; ++i) {
//        threads.emplace_back(thread_function);
//    }
//
//    for (auto& e : threads) e.join();
//    threads.clear();
//
//    stack_contains.reserve(stack_data_size);
//
//    for (int i =0; i < stack_data_size; ++i) {
//        stack_contains.emplace_back((res = g_stack.pop_new(output), output));
//    }
//
//    std::sort(stack_contains.begin(), stack_contains.end());
//
//    while (cnt < stack_data_size) {
//        const auto local_cnt = cnt % thread_count;
//        const auto curr_val = stack_contains.at(cnt);
//        if (0 == local_cnt) now_val = curr_val;
//        else if (curr_val != now_val) {
//            std::cout << "A-B-A case detected for upgraded atomic stack" << std::endl;
//            break;
//        }
//        ++cnt;
//    }
//
//    if (cnt == stack_data_size)
//        std::cout << "no A-B-A case detected for upgraded atomic stack" << std::endl;
//
//    assert(stack_contains.size() == stack_data_size);
//    stack_contains.clear();

/// ========================

    // std::cout << "Check for A-B-A for upgraded atomic bounded stack" << std::endl;

    // threads.reserve(thread_count);
    // for (int i =0; i < thread_count; ++i) {
    //     threads.emplace_back(thread_function);
    // }

    // for (auto& e : threads) e.join();
    // threads.clear();

    // output =0;
    // for (int i =0; i < stack_data_size; ++i) {
    //     stack_contains.emplace_back((res = g_stack.pop_new(output), output));
    // }

    // std::sort(stack_contains.begin(), stack_contains.end());

    // cnt =0, now_val =0;
    // while (cnt < stack_data_size) {
    //     const auto local_cnt = cnt % thread_count;
    //     const auto curr_val = stack_contains.at(cnt);
    //     if (0 == local_cnt) now_val = curr_val;
    //     else if (curr_val != now_val) {
    //         std::cout << "A-B-A case detected for upgraded atomic bounded stack" << std::endl;
    //         break;
    //     }
    //     ++cnt;
    // }

    // if (cnt == stack_data_size)
    //     std::cout << "no A-B-A case detected for upgraded atomic bounded stack" << std::endl;

    // assert(stack_contains.size() == stack_data_size);
    // stack_contains.clear();

/// ========================

    std::cout << "Check for consistancy for freelist" << std::endl;

    g_stack_name = "freelist";

    for (int i =0; i < thread_count; ++i) {
        threads.emplace_back(freelist_thread_function);
    }

    for (auto& e : threads) e.join();
    threads.clear();

    std::vector<ulong> allocated_adresses {};
    allocated_adresses.reserve(data_volume);

    for (int i =0; i < data_volume ; ++i) {
        uint8_t* mem { nullptr };
        res = g_freelist.capture(mem);
        allocated_adresses.emplace_back((ulong)mem);
    }

    std::sort(allocated_adresses.begin(), allocated_adresses.end());

    for (int i =0; i < (data_volume -1); ++i) {
        if ((allocated_adresses.at(i) + sizeof(nukes::details::nodes::mem_node<int>)) not_eq allocated_adresses.at(i+1)) {
            std::cout << "freelist inconcistant cause: "
                      << allocated_adresses.at(i) + sizeof(nukes::details::nodes::mem_node<int>) << " != "
                      << allocated_adresses.at(i+1) << " | STEP: "
                      << i << std::endl;
            return EXIT_FAILURE;
        }
    }

    const uint64_t g_fl_memaddr_beg = (uint64_t)(g_freelist.ptr_by_idx(0)),
                   g_fl_memaddr_end = (uint64_t)(g_freelist.ptr_by_idx(data_volume - 1)),
                   l_memaddr_beg = allocated_adresses.at(0),
                   l_memaddr_end = allocated_adresses.at(data_volume - 1);

    if ((data_volume - 1) not_eq g_freelist.idx_by_ptr(g_freelist.ptr_by_idx(data_volume - 1))) {
        std::cout << "memaddr and buffidx mapping is incorrect" << std::endl;
        return EXIT_FAILURE;
    } else if (g_fl_memaddr_beg not_eq l_memaddr_beg or
               g_fl_memaddr_end not_eq l_memaddr_end) {
        std::cout << "freelist is inconcistant in common" << std::endl;
        std::cout << "fl buf addr beg: " << g_fl_memaddr_beg << '\t' << "local addr beg: " << l_memaddr_beg << std::endl;
        std::cout << "fl buf addr end: " << g_fl_memaddr_end << '\t' << "local addr end: " << l_memaddr_end << std::endl;
        return EXIT_FAILURE;
    } else std::cout << "freelist is consistant" << std::endl;


    // ==============

//    g_stack_name = "classic atomic stack";
//
//    std::cout << "Check for A-B-A for classic atomic stack" << std::endl;
//
//    for (int i =0; i < thread_count; ++i) {
//        threads.emplace_back(aba_thread_function);
//    }
//
//    for (auto& e : threads) e.join();
//    threads.clear();
//
//    stack_contains.clear();
//    stack_contains.reserve(stack_data_size);
//
//    for (int i =0, output =0; i < stack_data_size; ++i) {
//        stack_contains.emplace_back((res = g_stack.pop_new(output), output));
//    }
//
//    std::sort(stack_contains.begin(), stack_contains.end());
//
//    cnt  =0; now_val =0;
//    while (cnt < stack_data_size) {
//        const auto local_cnt = cnt % thread_count;
//        const auto curr_val = stack_contains.at(cnt);
//        if (0 == local_cnt) now_val = curr_val;
//        else if (curr_val != now_val) {
//            std::cout << "A-B-A case detected for classic atomic stack" << std::endl;
//            break;
//        }
//        ++cnt;
//    }
//
//    if (cnt == stack_data_size)
//        std::cout << "no A-B-A case detected for classic atomic stack" << std::endl;
//
//    assert(stack_contains.size() == stack_data_size);

    const uint runtime = duration_cast<std::chrono::milliseconds>((std::chrono::steady_clock::now() - start)).count();
    std::cout << runtime << " ms" << std::endl;

    return 0;
}
