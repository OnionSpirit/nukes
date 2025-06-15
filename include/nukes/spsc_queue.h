#ifndef NUKES_BOUNDED_SPSC_QUEUE
#define NUKES_BOUNDED_SPSC_QUEUE

#include <atomic>

#include "constants.h"
#include "nukes/details/node_types.h"
#include "nukes/details/misc.h"
#include "nukes/details/batch.h"
#include "nukes/memory/atomic_bucketlist.h"


namespace nukes::bounded {

template <typename dataT, std::size_t capacityV>
class spsc_queue {

    // NOTE: Важно, т.к. работаем с абсолютными значениями
    static_assert(not ( capacityV & (capacityV - 1) ), "capacityV must be a power of 2");

    struct node {
        dataT _data;
    };

    node _rbuff[capacityV];

    // NOTE: Разбиваем головные и хвостовые данные по разным кеш-линиям для предотвращения false sharing

    // Cache line 1
    alignas(32) std::atomic<std::size_t> _head{0};
    alignas(32) std::size_t head_cache{0}; // NOTE: Кеш головы чтобы не грузить голову когда это не обязательно

    // Cache line 2
    alignas(32) std::atomic<std::size_t> _tail{0};
    alignas(32) std::size_t tail_cache{0}; // NOTE: Кеш хвоста чтобы не грузить голову когда это не обязательно

public:

    /**
     * @details Atomically pops an element from the queue to the reference
     * from function arg, returns the result of operation
     * @param data Reference to storage of pulled element
     * @return @b True if element successfully pulled,
     * @b False when pulling failed or data node sync failed
     */
    [[nodiscard]] bool pop(dataT& data);

    /**
     * @details Atomically pushes element to the queue
     * @param data Data to be pushed
     * @return @b True if element successfully pushed,
     * @b False when run out of capacity
     */
    [[nodiscard]] bool push(details::misc::fn_forward_t<dataT> data);
};

} // end namespace nukes::bounded


// ================================ DEFINITIONS ================================


#define BOUNDED_SPSC_QUEUE_MEMBER(member_type)   \
    template <typename dataT,                    \
    size_t capacityV                             \
    >                                            \
    member_type nukes::bounded::spsc_queue <     \
    dataT, capacityV>::


BOUNDED_SPSC_QUEUE_MEMBER(bool)
pop(dataT& data) {
    const std::size_t head = _head.load(std::memory_order_relaxed);

    // NOTE: Если голова сравнялась с кешем хвоста
    if (head >= tail_cache) {
        // NOTE: Обновляем кеш
        tail_cache = _tail.load(std::memory_order_acquire);
        // NOTE: Повторяем проверку с актуальным кешем
        if (head >= tail_cache)
            return false;
    }

    data = _rbuff[head % capacityV]._data;
    _head.fetch_add(1, std::memory_order_release);

    return true;
}

BOUNDED_SPSC_QUEUE_MEMBER(bool)
push(nukes::details::misc::fn_forward_t<dataT> data) {
    const std::size_t tail = _tail.load(std::memory_order_relaxed);

    // NOTE: Если хвост догнал кеш головы
    if (tail - head_cache >= capacityV) {
        // NOTE: Обновляем кеш
        head_cache = _head.load(std::memory_order_acquire);
        // NOTE: Повторяем проверку с актуальным кешем
        if (tail - head_cache >= capacityV)
            return false;
    }

    _rbuff[tail % capacityV]._data = data;
    _tail.fetch_add(1, std::memory_order_release);

    return true;
}

#undef BOUNDED_SPSC_QUEUE_MEMBER
#endif // NUKES_BOUNDED_SPSC_QUEUE
