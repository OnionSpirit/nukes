#ifndef NUKES_BOUNDED_MPSC_QUEUE
#define NUKES_BOUNDED_MPSC_QUEUE

#include <atomic>
#include <tuple>
#include "nukes/details/constants.h"
#include "nukes/details/node_types.h"
#include "nukes/details/misc.h"



namespace nukes::bounded {

template <
    typename dataT,
    details::constants::hword capacityV = details::constants::runtime_discover,
    details::constants::hword alignmentV = 8
>
class mpsc_queue {

    // NOTE: Важно, т.к. работаем с абсолютными значениями
    static_assert(not ( capacityV & (capacityV - 1) ), "capacityV must be a power of 2");

    typedef details::constants::hword index_t;
    struct handle { index_t index{0}; index_t cache{0}; };
    typedef std::atomic<handle> atomic_handle_t;
    typedef details::misc::aligned_data<dataT, alignmentV> node_t;

    static constexpr std::size_t storage_size_v  {
        capacityV
            ? capacityV * sizeof(node_t)
            : details::constants::word_size
    };
    typedef details::misc::meta_data<storage_size_v> storage_t;

    class bou_mpsc_iter {

        mpsc_queue* _queue { nullptr };

    public:
        explicit bou_mpsc_iter(mpsc_queue* queue)
            : _queue(queue) {}

        bou_mpsc_iter& postfix_increment(node_t*& ptr) {
            ptr += 1;
            return *this;
        }

        bou_mpsc_iter prefix_increment(node_t*& ptr)  {
            bou_mpsc_iter tmp = *this;
            ptr += 1;
            return tmp;
        }
    };

    typedef details::batch<node_t, bou_mpsc_iter, mpsc_queue*> batch_t;

    node_t*                          _buffer   { nullptr };  // NOTE: Буфер хранения памяти
    const details::constants::word   _capacity { capacityV };
    alignas(8) storage_t             _storage  {};

    // Cache line 1
    alignas(64) atomic_handle_t  _head       {}; // NOTE: Индекс головы с защитой от false sharing

    // Cache line 2
    alignas(64) atomic_handle_t  _tail       {}; // NOTE: Индекс хвоста с защитой от false sharing

public:

    mpsc_queue() noexcept
        requires ( capacityV != details::constants::runtime_discover );

    explicit mpsc_queue(details::constants::word = 1024) noexcept
        requires ( capacityV == details::constants::runtime_discover );

    ~mpsc_queue() noexcept =default;

    // NOTE: Запись в хвост
    [[nodiscard]] bool push(details::misc::fn_forward_t<dataT> data) noexcept;

    // NOTE: Чтение из головы
    [[nodiscard]] bool pop(dataT& data) noexcept;

    // NOTE: Очистка буфера, конструирование дефолтных значений объекта
    void clear() noexcept;

    /**
     * @details Weak operation, can show that empty queue is not empty,
     * but it will never show that not empty queue is empty
     * @return @b True when queue is empty (guaranteed), @b False when queue might have elements
     */
    bool empty() noexcept;


    batch_t pop_batch() noexcept {
        handle current_head = _head.load(std::memory_order_relaxed);
        handle current_tail = _tail.load(std::memory_order_relaxed);

        // NOTE: Проверяем, что буфер не пуст
        if (current_head.index >= current_tail.index)
            return batch_t { nullptr, nullptr, this };

        _head.store(current_tail, std::memory_order_relaxed);

        return batch_t { &_buffer[current_head.index % _capacity], &_buffer[current_tail.index % _capacity], this };
    }


};


} // end namespace nukes::bounded


// ================================ DEFINITIONS ================================

#define BOUNDED_MPSC_QUEUE_MEMBER(member_type)                             \
    template <                                                             \
        typename dataT,                                                    \
        nukes::details::constants::hword capacityV,                        \
        nukes::details::constants::hword alignmentV>                       \
    member_type nukes::bounded::mpsc_queue<dataT, capacityV, alignmentV>::

BOUNDED_MPSC_QUEUE_MEMBER()
mpsc_queue() noexcept
requires ( capacityV != nukes::details::constants::runtime_discover ) {
    // NOTE: При статическом определении размера ссылаем указатель буфера на начало хранилища,
    //       их размер соответствует запрошенному через шаблонный параметр
    _buffer = new (&_storage.template release<node_t>()) node_t[_capacity];
}

BOUNDED_MPSC_QUEUE_MEMBER()
mpsc_queue(nukes::details::constants::word capacity) noexcept
requires(capacityV == nukes::details::constants::runtime_discover)
: _capacity(capacity) {
    // NOTE: При динамическом определении размера, выделяем на куче нужный размер,
    //       сохраняем указатель в хранилище и записываем его в буфер
    _storage = new node_t[_capacity];
    _buffer = _storage.template release<node_t*>();
}

BOUNDED_MPSC_QUEUE_MEMBER(void)
clear() noexcept {
    for (int i = 0; i < _capacity; ++i)
        new (&_buffer[i]._data) dataT();
}

BOUNDED_MPSC_QUEUE_MEMBER(bool)
push(details::misc::fn_forward_t<dataT> data) noexcept {
    handle current_tail = _tail.load(std::memory_order_relaxed);
    handle next_tail;
    index_t cache_head = current_tail.cache;
    do {
        const index_t index_tail = current_tail.index;
        // NOTE: Проверка, что буфер полон
        if (index_tail - cache_head >= _capacity) {
            cache_head = _head.load(std::memory_order_acquire).index;
            if (index_tail - cache_head >= _capacity)
                return false;
        }

        next_tail = { .index = index_tail + 1, .cache = cache_head };
    } while (not _tail.compare_exchange_weak(current_tail, next_tail,
                                             std::memory_order_release,
                                             std::memory_order_relaxed));
    // NOTE: Сохраняем данные в буфер
    _buffer[current_tail.index % _capacity]._data = std::forward<dataT>(data);
    return true;
}


BOUNDED_MPSC_QUEUE_MEMBER(bool)
pop(dataT& data) noexcept {
    handle current_head = _head.load(std::memory_order_relaxed);
    index_t cache_tail = current_head.cache;

    const index_t index_head = current_head.index;
    // NOTE: Проверяем, что буфер не пуст
    if (index_head >= cache_tail) {
        cache_tail = _tail.load(std::memory_order_acquire).index;
        if (index_head >= cache_tail)
            return false;
    }

    handle new_head = { .index = index_head + 1, .cache = cache_tail };
    _head.store(new_head, std::memory_order_release);

    // NOTE: Читаем данные из буфера
    data = std::forward<dataT>(_buffer[current_head.index % _capacity]._data);
    return true;
}


BOUNDED_MPSC_QUEUE_MEMBER(bool)
empty() noexcept {

    auto head = _head.load(std::memory_order_acquire);
    auto tail = _tail.load(std::memory_order_acquire);
    if (tail.index == head.index)
        return true;
    return false;
}

#undef BOUNDED_MPSC_QUEUE_MEMBER
#endif // NUKES_BOUNDED_MPSC_QUEUE
