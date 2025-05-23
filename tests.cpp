#include <unordered_set>
#include "fixture.h"


TEST_F(atomics, do_check_atomic_lifo_consistancy) {
    constexpr std::size_t len = data_volume;

    typedef nukes::memory::atomic_lifo<int> container_t;
    container_t container{len};

    for (int i =0; i < thread_count; ++i)
        threads.emplace_back(thr_mempool_walker<container_t>, std::ref(container));

    for (auto& e : threads)
        e.join();
    threads.clear();

    std::vector<ulong> allocated_addresses {};
    allocated_addresses.reserve(len);

    for (int i =0; i < len; ++i) {
        int* mem { nullptr };
        container.capture(mem);
        allocated_addresses.emplace_back(reinterpret_cast<ulong>(mem));
    }

    std::ranges::sort(allocated_addresses);

    ASSERT_EQ(allocated_addresses.size(), len);

    for (int i =0; i < (len -1); ++i) {
        if ((allocated_addresses.at(i) + sizeof(nukes::details::nodes::mem_node<int>)) not_eq allocated_addresses.at(i+1)) {
            std::cout << "container inconcistant cause: "
                      << allocated_addresses.at(i) + sizeof(nukes::details::nodes::mem_node<int>) << " != "
                      << allocated_addresses.at(i+1) << " | STEP: "
                      << i << std::endl;
            FAIL();
        }
    }

    const uint64_t g_fl_memaddr_beg = reinterpret_cast<uint64_t>(container.ptr_by_idx(0)),
                   g_fl_memaddr_end = reinterpret_cast<uint64_t>(container.ptr_by_idx(len - 1)),
                   l_memaddr_beg = allocated_addresses.at(0),
                   l_memaddr_end = allocated_addresses.at(len - 1);

    if ((len - 1) not_eq container.idx_by_ptr(container.ptr_by_idx(len - 1))) {
        std::cout << "memaddr and buffidx mapping is incorrect" << std::endl;
        FAIL();
    } else if (g_fl_memaddr_beg not_eq l_memaddr_beg or
               g_fl_memaddr_end not_eq l_memaddr_end) {
        std::cout << "container is inconcistant in common" << std::endl;
        std::cout << "fl buf addr beg: " << g_fl_memaddr_beg << '\t' << "local addr beg: " << l_memaddr_beg << std::endl;
        std::cout << "fl buf addr end: " << g_fl_memaddr_end << '\t' << "local addr end: " << l_memaddr_end << std::endl;
        FAIL();
    } else std::cout << "container is consistant" << std::endl;

}

TEST_F(atomics, do_check_atomic_fifo_consistancy) {

    constexpr std::size_t len = data_volume;

    typedef nukes::memory::atomic_fifo<int> container_t;
    container_t container(len);

    for (int i =0; i < thread_count; ++i)
        threads.emplace_back(thr_mempool_walker<container_t>, std::ref(container));

    for (auto& e : threads)
        e.join();
    threads.clear();

    std::vector<ulong> allocated_adresses {};
    allocated_adresses.reserve(len);

    for (int i =0; i < len; ++i) {
        int* mem { nullptr };
        container.capture(mem);
        allocated_adresses.emplace_back(reinterpret_cast<ulong>(mem));
    }

    std::ranges::sort(allocated_adresses);

    ASSERT_EQ(allocated_adresses.size(), len);

    int allowed_misses {0}, zero_found {0};
    for (int i {0}; i < (len - 1); ++i) {
        // NOTE: При вычитывании произойдет 1
        //       ошибка с возвратом null-адреса
        //       из-за того что последний элемент не считать
        if (allocated_adresses.at(i) == 0 or allocated_adresses.at(i + 1) == 0 and zero_found == 0) {
            zero_found++;
            continue;
        }
        if ((allocated_adresses.at(i) + sizeof(nukes::details::nodes::mem_node<int>)) not_eq allocated_adresses.at(i+1)) {
            // NOTE: Из-за того что последний элемент
            //       не считать где-то будет 1 разрыв
            //       шириной в 1 чанк -> и адрес будет
            //       отличаться не на 1 элемент, а на 2
            if ((allocated_adresses.at(i) + 2 * sizeof(nukes::details::nodes::mem_node<int>)) == allocated_adresses.at(i+1) and allowed_misses == 0) {
                allowed_misses++;
                continue;
            }
            std::cout << "container inconcistant cause: "
                      << allocated_adresses.at(i) + sizeof(nukes::details::nodes::mem_node<int>) << " != "
                      << allocated_adresses.at(i+1) << " | STEP: "
                      << i << std::endl;
            FAIL();
        }
    }

    const uint64_t g_fl_memaddr_beg = reinterpret_cast<uint64_t>(container.ptr_by_idx(0)),
                   g_fl_memaddr_end = reinterpret_cast<uint64_t>(container.ptr_by_idx(len - 1)),
                   l_memaddr_beg = allocated_adresses.at(1),
                   l_memaddr_end = allocated_adresses.at(len - 1);

    if ((len - 1) not_eq container.idx_by_ptr(container.ptr_by_idx(len - 1))) {
        std::cout << "memaddr and buffidx mapping is incorrect" << std::endl;
        FAIL();
    } else if (g_fl_memaddr_beg not_eq l_memaddr_beg or
               g_fl_memaddr_end not_eq l_memaddr_end) {
        std::cout << "container is inconcistant in common" << std::endl;
        std::cout << "fl buf addr beg: " << g_fl_memaddr_beg << '\t' << "local addr beg: " << l_memaddr_beg << std::endl;
        std::cout << "fl buf addr end: " << g_fl_memaddr_end << '\t' << "local addr end: " << l_memaddr_end << std::endl;
        FAIL();
    } else std::cout << "container is consistant" << std::endl;

}

TEST_F(atomics, do_check_mpmc_consistancy) {

    constexpr std::size_t len = data_volume;

    typedef nukes::bounded_mpmc_queue<int> container_t;
    container_t container(len);

    for (int i =0; i < thread_count; ++i)
        threads.emplace_back(thr_mpmc_container_walker<container_t>, std::ref(container));

    for (auto& e : threads)
        e.join();
    threads.clear();

    std::vector<int> interactive_arr;
    interactive_arr.reserve(len);

    for (int interactive =0, i =0; i < len and not container.empty(); ++i) {
        container.pop(interactive);
        interactive_arr.emplace_back(interactive);
    }

    ASSERT_EQ(interactive_arr.size(), len - 1);

    std::unordered_set<int> thread_ids;

    for (auto& el : interactive_arr) {
        if (el == 0) continue;
        thread_ids.insert(el);
    }

    ASSERT_EQ(thread_ids.size(), thread_count);
}

TEST_F(atomics, do_check_mpsc_consistancy) {

    constexpr std::size_t len = data_volume;

    typedef nukes::bounded_mpmc_queue<int> container_t;
    container_t container(len);

    for (int i =0; i < thread_count; ++i)
        threads.emplace_back(thr_mpsc_container_walker<container_t>, std::ref(container));

    for (auto& e : threads)
        e.join();
    threads.clear();

    std::vector<int> interactive_arr;
    interactive_arr.reserve(len);

    for (int interactive =0, i =0; i < len and not container.empty(); ++i) {
        container.pop(interactive);
        interactive_arr.emplace_back(interactive);
    }

    ASSERT_EQ(interactive_arr.size(), len - 1);

    std::unordered_set<int> thread_ids;

    for (auto& el : interactive_arr) {
        if (el == 0) continue;
        thread_ids.insert(el);
    }

    ASSERT_EQ(thread_ids.size(), thread_count);
}

TEST_F(atomics, do_check_mpmc_batch) {

    constexpr std::size_t len = data_volume;

    typedef nukes::bounded_mpmc_queue<int> container_t;
    container_t container(len);

    for (int i =0; i < thread_count; ++i)
        threads.emplace_back(thr_mpmc_container_walker<container_t>, std::ref(container));

    for (auto& e : threads)
        e.join();
    threads.clear();

    std::vector<int> interactive_arr;
    interactive_arr.reserve(len);

    for (auto& interactive : container.pop_batch()) {
        interactive_arr.emplace_back(interactive);
    }

    ASSERT_EQ(interactive_arr.size(), len - 1);

    std::unordered_set<int> thread_ids;

    for (auto& el : interactive_arr) {
        if (el == 0) continue;
        thread_ids.insert(el);
    }

    ASSERT_EQ(thread_ids.size(), thread_count);
}
