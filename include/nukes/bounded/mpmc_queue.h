#ifndef NUKES_BOUNDED_MPMC_QUEUE
#define NUKES_BOUNDED_MPMC_QUEUE

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
class mpmc_queue {

    // NOTE: Важно, т.к. работаем с абсолютными значениями
    static_assert(not ( capacityV & (capacityV - 1) ), "capacityV must be a power of 2");

    typedef details::constants::hword index_t;
    typedef std::atomic<index_t> atomic_index_t;
    typedef std::pair<dataT*, std::size_t> batch_t;
    typedef details::misc::aligned_data<dataT, alignmentV> node_t;

    static constexpr std::size_t storage_size_v  {
        capacityV
            ? capacityV * sizeof(node_t)
            : details::constants::word_size
    };
    typedef details::misc::meta_data<storage_size_v> storage_t;

    node_t*                          _buffer   { nullptr };  // NOTE: Буфер хранения памяти
    const details::constants::word   _capacity { capacityV };
    alignas(8) storage_t             _storage  {};

    // Cache line 1
    alignas(64) atomic_index_t       _head       {0}; // NOTE: Индекс головы с защитой от false sharing

    // Cache line 2
    alignas(64) atomic_index_t       _tail       {0}; // NOTE: Индекс хвоста с защитой от false sharing

public:

    mpmc_queue() noexcept
        requires ( capacityV != details::constants::runtime_discover );

    mpmc_queue(details::constants::word = 1024) noexcept
        requires ( capacityV == details::constants::runtime_discover );

    ~mpmc_queue() noexcept =default;

    // NOTE: Запись в хвост
    [[nodiscard]] bool push(details::misc::fn_forward_t<dataT> data) noexcept;

    // NOTE: Чтение из головы
    [[nodiscard]] bool pop(dataT& data) noexcept;

    /**
     * @details Weak operation, can show that empty queue is not empty,
     * but it will never show that not empty queue is empty
     * @return @b True when queue is empty (guaranteed), @b False when queue might have elements
     */
    bool empty() noexcept;

    // // NOTE: Чтение пачки из головы
    // [[nodiscard]] std::pair<dataT*, std::size_t> pop_batch() noexcept;
};


} // end namespace nukes::bounded


// ================================ DEFINITIONS ================================

#define BOUNDED_MPMC_QUEUE_MEMBER(member_type)                             \
    template <                                                             \
        typename dataT,                                                    \
        nukes::details::constants::hword capacityV,                        \
        nukes::details::constants::hword alignmentV>                       \
    member_type nukes::bounded::mpmc_queue<dataT, capacityV, alignmentV>::


BOUNDED_MPMC_QUEUE_MEMBER()
mpmc_queue() noexcept
requires ( capacityV != nukes::details::constants::runtime_discover ) {
    // NOTE: При статическом определении размера ссылаем указатель буфера на начало хранилища,
    //       их размер соответствует запрошенному через шаблонный параметр
    _buffer = new (&_storage.template release<node_t>()) node_t[_capacity];
}

BOUNDED_MPMC_QUEUE_MEMBER()
mpmc_queue(nukes::details::constants::word capacity) noexcept
requires(capacityV == nukes::details::constants::runtime_discover)
: _capacity(capacity) {
    // NOTE: При динамическом определении размера, выделяем на куче нужный размер,
    //       сохраняем указатель в хранилище и записываем его в буфер
    _storage = new node_t[_capacity];
    _buffer = _storage.template release<node_t*>();
}


BOUNDED_MPMC_QUEUE_MEMBER(bool)
push(details::misc::fn_forward_t<dataT> data) noexcept {
    index_t current_tail = _tail.load(std::memory_order_relaxed);
    index_t next_tail;
    do {
        // NOTE: Проверка, что буфер полон
        if (current_tail - _head.load(std::memory_order_acquire) >= _capacity)
            return false;

        next_tail = current_tail + 1;
    } while (not _tail.compare_exchange_weak(current_tail, next_tail,
                                             std::memory_order_release,
                                             std::memory_order_relaxed));
    // NOTE: Сохраняем данные в буфер
    _buffer[current_tail % _capacity]._data = std::forward<dataT>(data);
    return true;
}


BOUNDED_MPMC_QUEUE_MEMBER(bool)
pop(dataT& data) noexcept {
    index_t current_head = _head.load(std::memory_order_relaxed);
    index_t next_head;
    do {
        // NOTE: Проверяем, что буфер не пуст
        if (current_head >= _tail.load(std::memory_order_acquire))
            return false;

        next_head = current_head + 1;
    } while (not _head.compare_exchange_weak(current_head, next_head,
                                             std::memory_order_release,
                                             std::memory_order_relaxed));
    // NOTE: Читаем данные из буфера
    data = std::forward<dataT>(_buffer[current_head % _capacity]._data);
    return true;
}


// BOUNDED_MPMC_QUEUE_MEMBER(std::pair<dataT*, std::size_t>)
// pop_batch() noexcept {

//     index_t current_head, next_head;
//     do {
//         current_head = _head.load(std::memory_order_relaxed);

//         // NOTE: Проверяем что буфер не пуст
//         if (current_head == _tail.load(std::memory_order_acquire)) {
//             return false;
//         }

//         // NOTE: Перезапускаем итерацию, если головной элемент ещё не записался в буфер
//         if (((meta_info)_buffer[current_head]._meta_data).load(std::memory_order_acquire))
//             continue;

//         next_head = _tail.load(std::memory_order_acquire);
//     } while (not _head.compare_exchange_weak(current_head, next_head,
//                                              std::memory_order_release,
//                                              std::memory_order_relaxed));

//     // TODO
//     // // NOTE: Читаем данные из буфера
//     // std::pair<dataT*, std::size_t> data_t;
//     // data = std::forward<dataT>(_buffer[current_head]._mem);

//     // // NOTE: Взводим флаг что данные в буфере очищены
//     // ((meta_info)_buffer[current_head]._meta_data).store(false, std::memory_order_release);

//     return std::pair<dataT*, std::size_t>{ nullptr, 0};

// }

BOUNDED_MPMC_QUEUE_MEMBER(bool)
empty() noexcept {

    auto head = _head.load(std::memory_order_acquire);
    if (_tail.compare_exchange_weak(head, head, std::memory_order_release, std::memory_order_relaxed))
        return true;
    else
        return false;
}

#undef BOUNDED_MPMC_QUEUE_MEMBER
#endif // NUKES_BOUNDED_MPMC_QUEUE
