#ifndef NUKES_BOUNDED_MPMC_QUEUE
#define NUKES_BOUNDED_MPMC_QUEUE

#include <atomic>
#include <cstdint>
#include <tuple>
#include <sys/types.h>
#include "nukes/details/constants.h"
#include "nukes/details/node_types.h"
#include "nukes/details/misc.h"



namespace nukes::bounded {

template <typename dataT, details::constants::hword lenV = nukes::details::constants::runtime_discover>
struct mpmc_queue {

protected:

    typedef std::atomic<bool> meta_info;
    typedef details::constants::hword index_t;
    typedef std::atomic<details::constants::hword> atomic_index_t;
    // typedef details::misc::meta_chunk<dataT, sizeof(meta_info)> data_chunk_t;
    typedef std::pair<dataT*, std::size_t> batch_t;

    static constexpr std::size_t storage_size_v  {
        lenV
            ? lenV * sizeof(dataT)
            : details::constants::word_size
    };
    typedef details::misc::meta_data<storage_size_v> storage_t;

    alignas(8) storage_t             _storage {};
    dataT*                           _buffer  { nullptr };  // NOTE: Буфер хранения памяти
    const details::constants::hword  _len     { lenV };
    alignas(64) atomic_index_t       _head    {0}; // NOTE: Индекс головы с защитой от false sharing
    alignas(64) atomic_index_t       _tail    {0}; // NOTE: Индекс хвоста с защитой от false sharing

public:

    mpmc_queue() noexcept
        requires ( lenV != details::constants::runtime_discover );

    mpmc_queue(details::constants::hword = 1024) noexcept
        requires ( lenV == details::constants::runtime_discover );

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

#define BOUNDED_MPMC_QUEUE_MEMBER(member_type)                          \
    template <typename dataT, nukes::details::constants::hword lenV>    \
    member_type nukes::bounded::mpmc_queue<dataT, lenV>::


BOUNDED_MPMC_QUEUE_MEMBER()
mpmc_queue() noexcept
requires ( lenV != nukes::details::constants::runtime_discover ) {
    // NOTE: При статическом определении размера ссылаем указатель буфера на начало хранилища,
    //       их размер соответствует запрошенному через шаблонный параметр
    _buffer = new (&_storage.template release<dataT>()) dataT[_len];
}

BOUNDED_MPMC_QUEUE_MEMBER()
mpmc_queue(nukes::details::constants::hword l) noexcept
requires(lenV == nukes::details::constants::runtime_discover)
: _len(l) {
    // NOTE: При динамическом определении размера, выделяем на куче нужный размер,
    //       сохраняем указатель в хранилище и записываем его в буфер
    _storage = new dataT[_len];
    _buffer = _storage.template release<dataT*>();
}


BOUNDED_MPMC_QUEUE_MEMBER(bool)
push(details::misc::fn_forward_t<dataT> data) noexcept {

    index_t current_tail, next_tail;

    do {
        current_tail = _tail.load(std::memory_order_relaxed);
        next_tail = (current_tail + 1) % _len;

        // NOTE: Проверка что буфер полон
        if (next_tail == _head.load(std::memory_order_acquire))
            return false;

    } while (not _tail.compare_exchange_weak(current_tail, next_tail,
                                             std::memory_order_release,
                                             std::memory_order_relaxed));

    // NOTE: Сохраняем данные в буфер
    _buffer[current_tail] = std::forward<dataT>(data);

    return true;
}


BOUNDED_MPMC_QUEUE_MEMBER(bool)
pop(dataT& data) noexcept {

    index_t current_head, next_head;
    do {
        current_head = _head.load(std::memory_order_relaxed);

        // NOTE: Проверяем что буфер не пуст
        if (current_head == _tail.load(std::memory_order_acquire))
            return false;

        next_head = (current_head + 1) % _len;
    } while (not _head.compare_exchange_weak(current_head, next_head,
                                             std::memory_order_release,
                                             std::memory_order_relaxed));

    // NOTE: Читаем данные из буфера
    data = std::forward<dataT>(_buffer[current_head]);

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
    else return false;
}

#undef MPMC_QUEUE_MEMBER
#endif // NUKES_BOUNDED_MPMC_QUEUE
