#ifndef NUKES_BOUNDED_SPSC_QUEUE
#define NUKES_BOUNDED_SPSC_QUEUE

#include <atomic>

#include "nukes/details/constants.h"
#include "nukes/details/misc.h"
#include "nukes/details/batch.h"


namespace nukes::bounded {

template <
    typename dataT,
    details::constants::hword capacityV = details::constants::runtime_discover,
    details::constants::hword alignmentV = 8
>
class spsc_queue {

    // NOTE: Важно, т.к. работаем с абсолютными значениями
    static_assert(not ( capacityV & (capacityV - 1) ), "capacityV must be a power of 2");

    typedef details::misc::aligned_data<dataT, alignmentV> node_t;
    typedef details::constants::hword index_t;

    static constexpr details::constants::hword storage_size_v  {
        capacityV
            ? capacityV * sizeof(node_t)
            : details::constants::word_size
    };
    typedef details::misc::meta_data<storage_size_v> storage_t;

    // NOTE: Разбиваем головные и хвостовые данные по разным кеш-линиям для предотвращения false sharing

    node_t*                          _buffer   { nullptr };  // NOTE: Буфер хранения памяти
    const details::constants::word   _capacity { capacityV };
    alignas(8) storage_t             _storage  {};

    // Cache line 1
    alignas(32) std::atomic<index_t> _head       {0};
    alignas(32) index_t              _tail_cache {0}; // NOTE: Кеш хвоста чтобы не грузить голову когда это не обязательно

    // Cache line 2
    alignas(32) std::atomic<index_t> _tail       {0};
    alignas(32) index_t              _head_cache {0}; // NOTE: Кеш головы чтобы не грузить голову когда это не обязательно

public:


    spsc_queue() noexcept
        requires ( capacityV != details::constants::runtime_discover );

    explicit spsc_queue(details::constants::word = 1024) noexcept
        requires ( capacityV == details::constants::runtime_discover );

    ~spsc_queue() noexcept =default;


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
    nukes::details::constants::hword capacityV,  \
    nukes::details::constants::hword alignmentV  \
    >                                            \
    member_type nukes::bounded::spsc_queue <     \
    dataT, capacityV, alignmentV>::


BOUNDED_SPSC_QUEUE_MEMBER()
spsc_queue() noexcept
requires ( capacityV != nukes::details::constants::runtime_discover ) {
    // NOTE: При статическом определении размера ссылаем указатель буфера на начало хранилища,
    //       их размер соответствует запрошенному через шаблонный параметр
    _buffer = new (&_storage.template release<node_t>()) node_t[_capacity];
}

BOUNDED_SPSC_QUEUE_MEMBER()
spsc_queue(nukes::details::constants::word capacity) noexcept
requires(capacityV == nukes::details::constants::runtime_discover)
: _capacity(capacity) {
    // NOTE: При динамическом определении размера, выделяем на куче нужный размер,
    //       сохраняем указатель в хранилище и записываем его в буфер
    _storage = new node_t[_capacity];
    _buffer = _storage.template release<node_t*>();
}

BOUNDED_SPSC_QUEUE_MEMBER(bool)
pop(dataT& data) {
    const index_t head = _head.load(std::memory_order_relaxed);

    // NOTE: Если голова сравнялась с кешем хвоста
    if (head >= _tail_cache) {
        // NOTE: Обновляем кеш
        _tail_cache = _tail.load(std::memory_order_acquire);
        // NOTE: Повторяем проверку с актуальным кешем
        if (head >= _tail_cache)
            return false;
    }

    data = _buffer[head % _capacity]._data;
    _head.fetch_add(1, std::memory_order_release);

    return true;
}

BOUNDED_SPSC_QUEUE_MEMBER(bool)
push(nukes::details::misc::fn_forward_t<dataT> data) {
    const index_t tail = _tail.load(std::memory_order_relaxed);

    // NOTE: Если хвост догнал кеш головы
    if (tail - _head_cache >= _capacity) {
        // NOTE: Обновляем кеш
        _head_cache = _head.load(std::memory_order_acquire);
        // NOTE: Повторяем проверку с актуальным кешем
        if (tail - _head_cache >= _capacity)
            return false;
    }

    _buffer[tail % _capacity]._data = data;
    _tail.fetch_add(1, std::memory_order_release);

    return true;
}

#undef BOUNDED_SPSC_QUEUE_MEMBER
#endif // NUKES_BOUNDED_SPSC_QUEUE
