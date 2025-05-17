#include "fixture.h"


TEST_F(atomics, do_check_atomic_lifo_consistancy) {
    constexpr std::size_t len = data_volume;

    typedef nukes::memory::atomic_lifo<int> container_t;
    container_t container{len};

    for (int i =0; i < thread_count; ++i) {
        threads.emplace_back(thr_mempool_walker<container_t>, std::ref(container));
    }

    for (auto& e : threads) e.join();
    threads.clear();

    std::vector<ulong> allocated_adresses {};
    allocated_adresses.reserve(len);

    bool res { true };
    for (int i =0; i < len and res; ++i) {
        int* mem { nullptr };
        res = container.capture(mem);
        allocated_adresses.emplace_back((ulong)mem);
    }

    std::sort(allocated_adresses.begin(), allocated_adresses.end());

    ASSERT_EQ(allocated_adresses.size(), len);

    for (int i =0; i < (len -1); ++i) {
        if ((allocated_adresses.at(i) + sizeof(nukes::details::nodes::mem_node<int>)) not_eq allocated_adresses.at(i+1)) {
            std::cout << "container inconcistant cause: "
                      << allocated_adresses.at(i) + sizeof(nukes::details::nodes::mem_node<int>) << " != "
                      << allocated_adresses.at(i+1) << " | STEP: "
                      << i << std::endl;
            FAIL();
        }
    }

    const uint64_t g_fl_memaddr_beg = (uint64_t)(container.ptr_by_idx(0)),
                   g_fl_memaddr_end = (uint64_t)(container.ptr_by_idx(len - 1)),
                   l_memaddr_beg = allocated_adresses.at(0),
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

TEST_F(atomics, do_check_atomic_fifo_consistancy) {

    constexpr std::size_t len = data_volume;

    typedef nukes::memory::atomic_fifo<int> container_t;
    container_t container(len);

    for (int i =0; i < thread_count; ++i) {
        threads.emplace_back(thr_mempool_walker<container_t>, std::ref(container));
    }

    for (auto& e : threads) e.join();
    threads.clear();

    std::vector<ulong> allocated_adresses {};
    allocated_adresses.reserve(len);

    bool res { true };
    for (int i =0; i < len; ++i) {
        int* mem { nullptr };
        res = container.capture(mem);
        allocated_adresses.emplace_back((ulong)mem);
    }

    std::sort(allocated_adresses.begin(), allocated_adresses.end());

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

    const uint64_t g_fl_memaddr_beg = (uint64_t)(container.ptr_by_idx(0)),
                   g_fl_memaddr_end = (uint64_t)(container.ptr_by_idx(len - 1)),
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
